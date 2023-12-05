#include "pch.h"

#ifdef _WINDOWS

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#endif

extern "C" {

     EXPORT HDECODER APIENTRY CreateDecoder() {
        auto res = new H264Decoder();
        res->Init();
        return res;
    }

     EXPORT void APIENTRY Decode(HDECODER decoder, uint8_t* srcBuffer, size_t srcBufferSize, uint8_t* dstBuffer, size_t dstBufferSize) {

        ((H264Decoder*)decoder)->Decode(srcBuffer, srcBufferSize, dstBuffer, dstBufferSize);
    }

     EXPORT void APIENTRY DestroyDecoder(HDECODER decoder) {
        delete decoder;
    }
}