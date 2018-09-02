//
//  TTVideoCodec.cpp
//  TTPlayerExample
//
//  Created by liang on 9/9/16.
//  Copyright © 2016 tina. All rights reserved.
//
// https://libav.org/documentation/doxygen/master/group__lavc__encdec.html

#include "easylogging++.h"

#include "TTVideoCodec.hpp"

using namespace TT;

VideoCodec::VideoCodec(const AVStream *avStream, VideoCodecType type) :
_avStream(avStream), _type(type) {
}

VideoCodec::~VideoCodec() {
    close();
}

bool VideoCodec::open() {
    if (_avStream) {
        if (kVideoCodecEncode == _type) {
            _avCodec = avcodec_find_encoder(_avStream->codecpar->codec_id);
            if (_avCodec) {
                _avCodecContext = avcodec_alloc_context3(_avCodec);
                _avCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
                _avCodecContext->codec_id = _avStream->codecpar->codec_id;
                
                _avCodecContext->width = _avStream->codecpar->width;//你想要的宽度
                _avCodecContext->height = _avStream->codecpar->height;//你想要的高度
                _avCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;//受codec->pix_fmts数组限制
                
                _avCodecContext->gop_size = 12;
                
                _avCodecContext->time_base = AVRational{1, 25};//应该根据帧率设置
                _avCodecContext->bit_rate = _avStream->codecpar->bit_rate;
                
                if (avcodec_open2(_avCodecContext, _avCodec, NULL) != 0) {
                    return false;
                } else {
                    avcodec_parameters_from_context(_avStream->codecpar, _avCodecContext);
                    return true;
                }
            }
        } else {
            _avCodec = avcodec_find_decoder(_avStream->codecpar->codec_id);
            if (_avCodec) {
                _avCodecContext = avcodec_alloc_context3(_avCodec);
                avcodec_parameters_to_context(_avCodecContext, _avStream->codecpar);
                if (avcodec_open2(_avCodecContext, _avCodec, NULL) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

void VideoCodec::close() {
    if (_avCodecContext) {
        avcodec_close(_avCodecContext);
        _avCodecContext = nullptr;
        _avCodec = nullptr;
    }
}

void VideoCodec::flush() {
    std::shared_ptr<Frame> frame = std::make_shared<Frame>();
    encode(frame);
}

std::shared_ptr<Frame> VideoCodec::decode(std::shared_ptr<Packet> packet) {
    AVFrame *avFrame = av_frame_alloc();
    int len, gotFrame;
    len = avcodec_decode_video2(_avCodecContext, avFrame, &gotFrame, packet->avpacket());
    if (len < 0) {
        printf("Error decoding video frame (%d), (%s)", len, av_err2str(len));
        av_frame_unref(avFrame);
        av_frame_free(&avFrame);
    } else if (gotFrame) {
        std::shared_ptr<Frame> frame = std::make_shared<Frame>(avFrame);
        frame->mediaType = kMediaTypeVideo;
        frame->dataType = kTextureTypeY420p;
        frame->pts = avFrame->pts == AV_NOPTS_VALUE ? packet->dts : avFrame->pts;
        AVRational tb = _avStream->time_base;
        frame->pts = frame->pts * av_q2d(tb) * 1000;
        av_frame_unref(avFrame);
        av_frame_free(&avFrame);
        return frame;
    }
    
    return nullptr;
}

void VideoCodec::encode(std::shared_ptr<Frame> frame) {
    if (_avCodecContext == nullptr) {
        LOG(ERROR) << "Codec VideoCodec encode avcodeccontext is null";
        return;
    }
    
    int rsend = avcodec_send_frame(_avCodecContext, frame->avFrame());
    if (rsend < 0 && AVERROR(EAGAIN) != rsend) {
        LOG(ERROR) << "Codec VideoCodec encode send frame error: " << av_err2str(rsend);
        return;
    }
    
    do {
        AVPacket *avpacket = av_packet_alloc();
        int rrecv = avcodec_receive_packet(_avCodecContext, avpacket);
        if (rrecv != 0) {
            LOG(ERROR) << "Codec VideoCodec encode receive packet error: " << av_err2str(rrecv);
            av_packet_free(&avpacket);
            if (AVERROR(EAGAIN) == rrecv) {
                LOG(WARNING) << "Codec VideoCodec encode need receive after send frame again";
                if (AVERROR(EAGAIN) == rsend) {
                    LOG(INFO) << "Codec VideocCodec encode try to send input";
                    rsend = avcodec_send_frame(_avCodecContext, frame->avFrame());
                    continue;
                } else {
                    break;
                }
            } else if (AVERROR_EOF == rrecv) {
                LOG(INFO) << "Codec VideocCodec encode eof";
                break;
            }
        } else {
            if (rrecv == 0 && _encodeFrameCallback) {
                std::shared_ptr<Packet> packet = std::make_shared<Packet>(avpacket);
                _encodeFrameCallback(packet);
                continue;
            } else {
                if (!_encodeFrameCallback) {
                    LOG(WARNING) << "Codec VideoCodec encode did not has encodeFrameCallback";
                }
                av_packet_free(&avpacket);
            }
        }
    } while (true);
}

