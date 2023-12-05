#pragma once

class H264Decoder 
{
public:
	H264Decoder();
	~H264Decoder();

    void Init();

    void Decode(uint8_t* srcBuffer, size_t srcBufferSize, uint8_t* dstBuffer, size_t dstBufferSize);

protected:
	const AVCodec* _codec = nullptr;
	AVCodecContext* _ctx = nullptr;
};