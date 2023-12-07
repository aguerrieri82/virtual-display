cmake_minimum_required(VERSION 3.1...3.27)

project(
  VirtualDisplay.Decoder.Native
  VERSION 1.0
  LANGUAGES CXX)

set(ffmpegBase "${PROJECT_SOURCE_DIR}/../../lib/ffmpeg-h264/linux/")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/linux-x64/")

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")


add_library(VirtualDisplay.Decoder.Native SHARED 
H264Decoder.cpp 
dllmain.cpp
)

target_include_directories(VirtualDisplay.Decoder.Native PRIVATE 
	"${ffmpegBase}/include"
)

target_link_libraries(VirtualDisplay.Decoder.Native  
	"${ffmpegBase}/lib/libavcodec.a"
	"${ffmpegBase}/lib/libavutil.a"
	"${ffmpegBase}/lib/libswscale.a"
	"${ffmpegBase}/lib/libswresample.a"
)

 set_target_properties(VirtualDisplay.Decoder.Native PROPERTIES PREFIX "")