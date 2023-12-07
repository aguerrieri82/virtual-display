#include "pch.h"

H264Decoder::H264Decoder() {

}

H264Decoder::~H264Decoder() {

	AVCodecContext* ctx = _ctx;

	if (ctx != nullptr) {
		avcodec_free_context(&ctx);
		_ctx = nullptr;
	}

	_codec = nullptr;

	SafeDelete(_codec)
}

void H264Decoder::Init() {

	int result;

	if (_ctx != nullptr) {
		AVCodecContext* ctx = _ctx;
		avcodec_free_context(&ctx);
	}

	_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	_ctx = avcodec_alloc_context3(_codec);
	result = avcodec_open2(_ctx, _codec, NULL);
}


void H264Decoder::Decode(uint8_t* srcBuffer, size_t srcBufferSize, uint8_t* dstBuffer, size_t dstBufferSize) {

	int result;
	AVFrame* frame;

	frame = av_frame_alloc();

	AVPacket* srcPacket = av_packet_alloc();

	srcPacket->data = srcBuffer;
	srcPacket->size = srcBufferSize;

	result = avcodec_send_packet(_ctx, srcPacket);

	if (result == 0) {
		result = avcodec_receive_frame(_ctx, frame);

		SwsContext* ctx = sws_getContext(frame->width, frame->height,
			(AVPixelFormat)frame->format, frame->width, frame->height,
			AV_PIX_FMT_RGBA, 0, nullptr, nullptr, nullptr);

		uint8_t* outData[1] = { dstBuffer };
		uint8_t* inData[8] = {};
		int outStride[1] = { frame->width * 4 };

		for (int i = 0; i < 8; i++) {
			if (frame->buf[i] == nullptr)
				break;
			inData[i] = frame->buf[i]->data;
		}

		sws_scale(ctx, inData, frame->linesize, 0, frame->height, outData, outStride);

		av_frame_free(&frame);

	}
	av_frame_free(&frame);

	av_packet_free(&srcPacket);
}