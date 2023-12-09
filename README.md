# VirtualDisplay

## Build

- Install WDK
https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
- Build the driver (`VirtualDisplay.Driver` project)
- Go to `src\VirtualDisplay.Driver\bin\x64\Debug`
- Trust the certificate as `vdisplay.cer` in 'Trusted Root Certification Authorities'
- Go to `src\VirtualDisplay.Driver\bin\x64\Debug\VirtualDisplay.Driver`
- Right click on `VirtualDisplay.inf` and select `Install`

## Build ffmpeg
A pre-built version (Dic 2023) is in `lib\ffmpeg-h264`
if you want to update or rebuild the library, follow the next steps:

### Windows

- Clone repository https://github.com/FFmpeg/FFmpeg
- Install https://www.msys2.org/
- Install visual studio 2022 with c++ build tools 
- In msys shell (`msys2.exe`)
  ```
  pacman -S diffutils yasm 
  /C/Program Files/Microsoft Visual Studio/2022/Preview/VC/Auxiliary/Build/vcvars64.bat
  export PATH="/C/Program Files/Microsoft Visual Studio/2022/Preview/VC/Tools/MSVC/14.39.33218/bin/Hostx64/x64":$PATH
  ```
  Ensure cl.exe is running and change visual studio path according to your actual version
- Go in ffmpeg source folder:
	``` 
	./configure --toolchain=msvc --target-os=win64 --arch=x86_64 --disable-everything --enable-encoder=h264_mf --enable-decoder=h264 --prefix=/c/ffmpeg-build
	make
	make install
	``` 
- Your libs and header will be in `c:\ffmpeg-build`

### Linux (or wsl)

- Clone repository https://github.com/FFmpeg/FFmpeg
- Go in ffmpeg source folder:
	``` 
	./configure --disable-everything --enable-encoder=libx264  --prefix=~/ffmpeg-build
	make
	make install
	``` 
- Your libs and header will be in `~/ffmpeg-build`
