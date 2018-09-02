//
//  TTVideoCodec.hpp
//  TTPlayerExample
//
//  Created by liang on 9/9/16.
//  Copyright © 2016 tina. All rights reserved.
//

#ifndef TTVideoCodec_hpp
#define TTVideoCodec_hpp

#include <stdio.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#ifdef __cplusplus
};
#endif

#include "TTFrame.hpp"
#include "TTPacket.hpp"

namespace TT {
    typedef enum {
        kVideoCodecNone,
        kVideoCodecDecode,
        kVideoCodecEncode
    } VideoCodecType;
    
    class VideoCodec {
    public:
        VideoCodec(const AVStream *avStream, VideoCodecType type = kVideoCodecDecode);
        ~VideoCodec();
        
        bool open();
        void close();
        void flush();
        std::shared_ptr<Frame> decode(std::shared_ptr<Packet> packet);
        void encode(std::shared_ptr<Frame> frame);
        
        typedef std::function<void (std::shared_ptr<Packet>)> EncodeFrameCallback;
        void setEncodeFrameCallback(EncodeFrameCallback cb) { _encodeFrameCallback = cb; }
        
    private:
        VideoCodecType _type;
        
        const AVStream *_avStream;
        AVCodecContext *_avCodecContext;
        AVCodec *_avCodec;
        
        EncodeFrameCallback _encodeFrameCallback;
    };
}

#endif /* TTVideoCodec_hpp */
