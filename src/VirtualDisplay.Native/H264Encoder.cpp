#include "pch.h"

#define CHECK_HR(x, err) if (FAILED(x)) { throw gcnew System::Runtime::InteropServices::COMException(msclr::interop::marshal_as<String^>(err), hr); }


const D3D_DRIVER_TYPE DriverTypes[] =
{
	D3D_DRIVER_TYPE_HARDWARE,
	D3D_DRIVER_TYPE_WARP,
	D3D_DRIVER_TYPE_REFERENCE,
};;

const D3D_FEATURE_LEVEL FeatureLevels[] =
{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_1
};


namespace VirtualDisplay {

	H264Encoder::H264Encoder() {
		_state = new H264EncoderState();
	}


	H264Encoder::~H264Encoder()
	{
		delete  _state;
		_state = nullptr;
	}

	void H264Encoder::Start(VideoFormat^ format)
	{
		_format = format;

		HRESULT hr = MFStartup(MF_VERSION);
		CHECK_HR(hr, "Failed to start Media Foundation");

		hr = ::CoCreateInstance(
			CLSID_VideoProcessorMFT,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_IMFTransform,
			(LPVOID*)&_state->converter);

		CHECK_HR(hr, "Failed to create video processor");

		CComPtr<IMFMediaType> convInputType;

		hr = MFCreateMediaType(&convInputType);
		CHECK_HR(hr, "Failed to converter create media type");

		hr = convInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		CHECK_HR(hr, "Failed set converter MF_MT_MAJOR_TYPE");

		switch (format->PixelFormat)
		{
		case PixelFormat::RGBA32:
			hr = convInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
			CHECK_HR(hr, "Failed converter set MF_MT_SUBTYPE");
			break;
		default:
			break;
		}

		hr = convInputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		CHECK_HR(hr, "Failed set converter MF_MT_ALL_SAMPLES_INDEPENDENT");

		hr = convInputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		CHECK_HR(hr, "Failed set converter MF_MT_INTERLACE_MODE");

		hr = MFSetAttributeSize(convInputType, MF_MT_FRAME_SIZE, format->Width, format->Height);
		CHECK_HR(hr, "Failed set converter MF_MT_FRAME_SIZE");

		hr = MFSetAttributeRatio(convInputType, MF_MT_FRAME_RATE, format->Fps, 1);
		CHECK_HR(hr, "Failed set converter MF_MT_FRAME_RATE");


		hr = _state->converter->SetInputType(0, convInputType, 0);
		CHECK_HR(hr, "Failed call converter SetInputType");

		CComPtr<IMFMediaType> convOutType;

		for (int i = 0; SUCCEEDED(_state->converter->GetOutputAvailableType(0, i, &convOutType)); i++) {

			GUID outSubType = { 0 };
			hr = convOutType->GetGUID(MF_MT_SUBTYPE, &outSubType);

			if (outSubType == MFVideoFormat_NV12) {

				hr = convOutType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
				CHECK_HR(hr, "Failed set converter out MF_MT_ALL_SAMPLES_INDEPENDENT");

				hr = convOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
				CHECK_HR(hr, "Failed set converter out MF_MT_INTERLACE_MODE");

				hr = MFSetAttributeSize(convOutType, MF_MT_FRAME_SIZE, format->Width, format->Height);
				CHECK_HR(hr, "Failed set converter out MF_MT_FRAME_SIZE")

					hr = _state->converter->SetOutputType(0, convOutType, 0);
				CHECK_HR(hr, "Failed call converter SetOutputType");

				break;
			}
			else
				convOutType.Release();
		}


		hr = MFCreateMemoryBuffer(format->Stride * format->Height, &_state->inputBuffer);
		CHECK_HR(hr, "Failed create Input Buffer");

		CComPtr < IDXGIFactory> factory;
		hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
		CHECK_HR(hr, "Failed CreateDXGIFactory");

		DXGI_ADAPTER_DESC adapterDesc;
		CComPtr<IDXGIAdapter> adapter;

		int adapterIndex = 0;


		D3D_FEATURE_LEVEL featureLevel;


		while (true) {
			CComPtr<IDXGIAdapter> curAdapter;
			hr = factory->EnumAdapters(adapterIndex, &curAdapter);
			if (SUCCEEDED(hr)) {

				hr = curAdapter->GetDesc(&adapterDesc);
				CHECK_HR(hr, "Adapter GetDesc failed");
				if (adapterDesc.VendorId == 0x000010de) {
					adapter = curAdapter;

					hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
						D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
						nullptr, 0, D3D11_SDK_VERSION, &_state->device, &featureLevel, &_state->context);

					CHECK_HR(hr, "Failed to create device");

					break;
				}
				adapterIndex++;
			}
			else
				break;
		}


		if (_state->device.p == nullptr) {

			// Create device
			for (UINT DriverTypeIndex = 0; DriverTypeIndex < ARRAYSIZE(DriverTypes); ++DriverTypeIndex)
			{
				hr = D3D11CreateDevice(adapter, DriverTypes[DriverTypeIndex], nullptr,
					D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
					FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION, &_state->device, &featureLevel, &_state->context);

				if (SUCCEEDED(hr))
					break;
			}

			CHECK_HR(hr, "Failed to create device");


			CComPtr<IDXGIDevice> dxi;

			hr = _state->device->QueryInterface<IDXGIDevice>(&dxi);
			CHECK_HR(hr, "device QueryInterface IDXGIDevice failed");

			hr = dxi->GetAdapter(&adapter);
			CHECK_HR(hr, "dxu GetAdapter failed");

			DXGI_ADAPTER_DESC adapterDesc;
			hr = adapter->GetDesc(&adapterDesc);
			CHECK_HR(hr, "Adapter GetDesc failed");
		}


		// Create device manager
		UINT resetToken;
		hr = MFCreateDXGIDeviceManager(&resetToken, &_state->deviceManager);
		CHECK_HR(hr, "Failed to create DXGIDeviceManager");

		hr = _state->deviceManager->ResetDevice(_state->device, resetToken);
		CHECK_HR(hr, "Failed to assign D3D device to device manager");

		D3D11_TEXTURE2D_DESC desc = { 0 };
		desc.Format = DXGI_FORMAT_NV12;
		desc.Width = format->Width;
		desc.Height = format->Height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;

		hr = _state->device->CreateTexture2D(&desc, NULL, &_state->surface);
		CHECK_HR(hr, "Could not create surface");

		CComHeapPtr<IMFActivate*> activateRaw;
		UINT32 activateCount = 0;

		MFT_REGISTER_TYPE_INFO inInfo = { MFMediaType_Video, MFVideoFormat_NV12 };
		MFT_REGISTER_TYPE_INFO outInfo = { MFMediaType_Video, MFVideoFormat_H264 };

		CComPtr<IMFAttributes> enumAttrs;
		hr = MFCreateAttributes(&enumAttrs, 1);
		hr = enumAttrs->SetBlob(MFT_ENUM_ADAPTER_LUID, (BYTE*)&adapterDesc.AdapterLuid, sizeof(LUID));

		UINT32 flags =
			MFT_ENUM_FLAG_HARDWARE |
			MFT_ENUM_FLAG_SORTANDFILTER;

		hr = MFTEnum2(
			MFT_CATEGORY_VIDEO_ENCODER,
			flags,
			&inInfo,
			&outInfo,
			enumAttrs,
			&activateRaw,
			&activateCount
		);
		CHECK_HR(hr, "Failed to enumerate MFTs");


		CComPtr<IMFActivate> activate = activateRaw[0];

		for (UINT32 i = 0; i < activateCount; i++)
			activateRaw[i]->Release();

		// Activate
		hr = activate->ActivateObject(IID_PPV_ARGS(&_state->encoder));
		CHECK_HR(hr, "Failed to activate MFT");

		// Get attributes
		CComPtr<IMFAttributes> attributes;
		hr = _state->encoder->GetAttributes(&attributes);
		CHECK_HR(hr, "Failed to get MFT attributes");

		// Get encoder name
		UINT32 nameLength = 0;
		std::wstring name;

		hr = attributes->GetStringLength(MFT_FRIENDLY_NAME_Attribute, &nameLength);

		if (SUCCEEDED(hr)) {

			name.resize(nameLength + 1);

			hr = attributes->GetString(MFT_FRIENDLY_NAME_Attribute, &name[0], name.size(), &nameLength);
			CHECK_HR(hr, "Failed to get MFT name");

			name.resize(nameLength);
		}


		hr = attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
		CHECK_HR(hr, "Failed MF_TRANSFORM_ASYNC_UNLOCK");

		hr = _state->encoder.QueryInterface< IMFMediaEventGenerator>(&_state->eventGen);
		CHECK_HR(hr, "Failed to QI for event generator");

		// Get stream IDs (expect 1 input and 1 output stream)
		hr = _state->encoder->GetStreamIDs(1, &_state->inputStreamID, 1, &_state->outputStreamID);
		if (hr == E_NOTIMPL)
		{
			_state->inputStreamID = 0;
			_state->outputStreamID = 0;
			hr = S_OK;
		}

		CHECK_HR(hr, "Failed to get stream IDs");


		/*************************************/

		hr = _state->encoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(_state->deviceManager.p));
		CHECK_HR(hr, "Failed to set D3D manager encoder ");


		CComPtr<IMFMediaType> encOutType;

		hr = MFCreateMediaType(&encOutType);
		CHECK_HR(hr, "Failed to converter create media type");

		hr = encOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		CHECK_HR(hr, "Failed to set MF_MT_MAJOR_TYPE on H264 output media type");

		hr = encOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		CHECK_HR(hr, "Failed to set MF_MT_SUBTYPE on H264 output media type");

		hr = encOutType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile::eAVEncH264VProfile_High);
		CHECK_HR(hr, "Failed to set MF_MT_MPEG2_PROFILE on H264 output media type");

		hr = encOutType->SetUINT32(MF_MT_MPEG2_LEVEL, eAVEncH264VLevel::eAVEncH264VLevel5);
		CHECK_HR(hr, "Failed to set MF_MT_MPEG2_LEVEL on H264 output media type");

		hr = encOutType->SetUINT32(MF_MT_AVG_BITRATE, format->BitRate);
		CHECK_HR(hr, "Failed to set average bit rate on H264 output media type");

		hr = MFSetAttributeSize(encOutType, MF_MT_FRAME_SIZE, format->Width, format->Height);
		CHECK_HR(hr, "Failed to set frame size on H264 MFT out type");

		hr = MFSetAttributeRatio(encOutType, MF_MT_FRAME_RATE, format->Fps, 1);
		CHECK_HR(hr, "Failed to set frame rate on H264 MFT out type");

		hr = encOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		CHECK_HR(hr, "Failed to set MF_MT_INTERLACE_MODE on H.264 encoder MFT");

		hr = encOutType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		CHECK_HR(hr, "Failed to set MF_MT_ALL_SAMPLES_INDEPENDENT on H.264 encoder MFT");

		hr = MFSetAttributeRatio(encOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		CHECK_HR(hr, "Failed to set MF_MT_PIXEL_ASPECT_RATIO on H.264 encoder MFT");

		hr = encOutType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, 10);
		CHECK_HR(hr, "Failed to set MF_MT_MAX_KEYFRAME_SPACING on H.264 encoder MFT");


		hr = _state->encoder->SetOutputType(_state->outputStreamID, encOutType, 0);
		CHECK_HR(hr, "Failed to set output media type on H.264 encoder MFT");


		CComPtr<IMFMediaType> encInType;

		for (int i = 0; SUCCEEDED(_state->encoder->GetInputAvailableType(0, i, &encInType)); i++) {

			GUID inSubType = { 0 };
			hr = encInType->GetGUID(MF_MT_SUBTYPE, &inSubType);

			if (inSubType == MFVideoFormat_NV12) {

				hr = _state->encoder->SetInputType(0, encInType, 0);
				CHECK_HR(hr, "Failed call encoder SetInputType");

				break;
			}
			else
				encInType.Release();
		}


		// hr = _state->converter->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(_state->deviceManager.p));
		// CHECK_HR(hr, "Failed to set D3D manager converter ");

		hr = attributes->SetUINT32(MF_LOW_LATENCY, TRUE);
		CHECK_HR(hr, "Failed to set MF_LOW_LATENCY");

		hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), _state->surface, 0, FALSE, &_state->outputBuffer);
		CHECK_HR(hr, "Failed create MFCreateDXGISurfaceBuffer");


		hr = _state->converter->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
		CHECK_HR(hr, "Failed converter BEGIN_STREAMING");

		hr = _state->encoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
		CHECK_HR(hr, "Failed encoder MFT_MESSAGE_COMMAND_FLUSH");
		hr = _state->encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
		CHECK_HR(hr, "Failed encoder BEGIN_STREAMING");
		hr = _state->encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
		CHECK_HR(hr, "Failed encoder MFT_MESSAGE_NOTIFY_START_OF_STREAM");



		_frameCount = 0;
	}

	array<byte>^ H264Encoder::GetHeader() {

		array<Byte>^ result;
		UINT32 headerSize;

		CComPtr<IMFMediaType> encOutType;
		HRESULT hr = _state->encoder->GetOutputCurrentType(_state->outputStreamID, &encOutType);
		CHECK_HR(hr, "Failed encoder GetOutputCurrentType");

	    hr = encOutType->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &headerSize);
		CHECK_HR(hr, "Failed encoder read MF_MT_MPEG_SEQUENCE_HEADER size");

		auto header = new UINT8[headerSize];
		hr = encOutType->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, header, headerSize, &headerSize);
		CHECK_HR(hr, "Failed encoder read MF_MT_MPEG_SEQUENCE_HEADER");

		result = gcnew array<Byte>(headerSize);

		pin_ptr<byte> headerPtr = &result[0];
		memcpy(headerPtr, header, headerSize);

		return result;
	}

	void H264Encoder::Convert(FrameBuffer^ src, FrameBuffer^ dst)
	{
		BYTE* inData;

		HRESULT hr = _state->inputBuffer->Lock(&inData, NULL, NULL);
		CHECK_HR(hr, "Failed Buffer Lock");

		if (src->ByteArray != nullptr) {

			pin_ptr<byte> bufferPtr = &src->ByteArray[0];
			memcpy(inData, bufferPtr, _format->Stride * _format->Height);
		}
		else
			memcpy(inData, src->Pointer.ToPointer(), _format->Stride * _format->Height);

		hr = _state->inputBuffer->Unlock();
		CHECK_HR(hr, "Failed Buffer Unlock");

		CComPtr<IMFSample> inSample;

		hr = MFCreateSample(&inSample);
		CHECK_HR(hr, "Failed create media sample");

		auto curTime = _frameCount * (1000 / _format->Fps) * 1000 * 10;

		inSample->SetSampleTime(curTime);
		CHECK_HR(hr, "Failed in SetSampleTime");

		hr = inSample->AddBuffer(_state->inputBuffer);
		CHECK_HR(hr, "Failed in AddBuffer");

		hr = _state->converter->ProcessInput(0, inSample, 0);
		CHECK_HR(hr, "Failed converter ProcessInput");

		CComPtr<IMFSample> outSample;

		hr = MFCreateSample(&outSample);
		CHECK_HR(hr, "Failed create media sample");

		hr = outSample->AddBuffer(_state->outputBuffer);
		CHECK_HR(hr, "Failed out AddBuffer");

		hr = outSample->SetSampleTime(curTime);
		CHECK_HR(hr, "Failed out SetSampleTime");

		DWORD status;
		MFT_OUTPUT_DATA_BUFFER outputBuffer;
		outputBuffer.dwStreamID = 0;
		outputBuffer.pSample = outSample;
		outputBuffer.dwStatus = 0;
		outputBuffer.pEvents = nullptr;

		hr = _state->converter->ProcessOutput(0, 1, &outputBuffer, &status);
		CHECK_HR(hr, "Failed converter ProcessOutput");

		while (true) {

			CComPtr<IMFMediaEvent> event;
			hr = _state->eventGen->GetEvent(0, &event);
			CHECK_HR(hr, "Failed to get event");

			MediaEventType eventType;
			hr = event->GetType(&eventType);
			CHECK_HR(hr, "Failed to get event type");

			if (eventType == METransformNeedInput) {

				hr = _state->encoder->ProcessInput(_state->inputStreamID, outSample, 0);
				CHECK_HR(hr, "Failed ProcessInput encoder");
			}

			else if (eventType == METransformHaveOutput) {

				outputBuffer.dwStreamID = _state->outputStreamID;
				outputBuffer.pSample = nullptr;
				outputBuffer.dwStatus = 0;
				outputBuffer.pEvents = nullptr;

				hr = _state->encoder->ProcessOutput(0, 1, &outputBuffer, &status);
				CHECK_HR(hr, "ProcessOutput encoder failed");

				CComPtr<IMFMediaBuffer> encOutBuffer;
				hr = outputBuffer.pSample->GetBufferByIndex(0, &encOutBuffer);
				CHECK_HR(hr, "GetBufferByIndex encoder failed");

				BYTE* outData;
				DWORD outLen;
				hr = encOutBuffer->Lock(&outData, NULL, &outLen);
				CHECK_HR(hr, "Failed Buffer Lock");

				if (dst->ByteArray == nullptr || dst->ByteArray->Length != outLen)
					dst->ByteArray = gcnew array<Byte>(outLen);

				pin_ptr<byte> bufferPtr = &dst->ByteArray[0];
				memcpy(bufferPtr, outData, outLen);

				hr = encOutBuffer->Unlock();

				if (outputBuffer.pSample)
					outputBuffer.pSample->Release();

				if (outputBuffer.pEvents)
					outputBuffer.pEvents->Release();

				break;
			}
		}

		_frameCount++;
	}

	void H264Encoder::Stop()
	{
		throw gcnew System::NotImplementedException();
	}

}
