#pragma once

namespace VirtualDisplay {

	struct H264EncoderState {

		CComPtr<IMFTransform> converter;
		CComPtr<IMFTransform> encoder;
		CComPtr<IMFMediaBuffer> inputBuffer;
		CComPtr<IMFMediaBuffer> outputBuffer;
		CComPtr<ID3D11Device> device;
		CComPtr<ID3D11DeviceContext> context;
		CComPtr<IMFDXGIDeviceManager> deviceManager;
		CComPtr<ID3D11Texture2D> surface;
		CComPtr<IMFMediaEventGenerator> eventGen;
		DWORD inputStreamID;
		DWORD outputStreamID;

	};


	public ref class H264Encoder : public IVideoConverter
	{
	public:

		H264Encoder();

		~H264Encoder();

		array<byte>^ GetHeader();

		virtual void Start(VideoFormat^ format);

		virtual void Convert(FrameBuffer^ src, FrameBuffer^ dst);

		virtual void Stop();


	protected:
		VideoFormat^ _format;
		int _frameCount;
	private:
		H264EncoderState* _state;
	};

}