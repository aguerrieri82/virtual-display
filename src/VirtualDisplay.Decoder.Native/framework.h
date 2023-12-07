#pragma once

#define inline __inline

extern "C" {
	#include <libswscale/swscale.h>
	#include <libavcodec/avcodec.h>
}

#ifdef _WINDOWS

	#include <windows.h>

	#define FFMPEG_LIB(a) "..\\..\\lib\\ffmpeg-h264\\win64\\lib\\"a

	#pragma comment (lib, FFMPEG_LIB("libavcodec"))
	#pragma comment (lib, FFMPEG_LIB("libavutil"))
	#pragma comment (lib, FFMPEG_LIB("libswscale"))
	#pragma comment (lib, FFMPEG_LIB("libswresample"))
	#pragma comment (lib, "bcrypt.lib")

	#define EXPORT __declspec(dllexport)

#else

	#define EXPORT __attribute__((visibility("default")))
	#define APIENTRY
#endif


#define SafeDelete(obj) if (obj != nullptr) { delete obj; obj = nullptr; }

typedef void* HDECODER;