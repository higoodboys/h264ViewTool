# FFmpeg 本地运行时

- 编译/链接使用 MSYS2 UCRT64 包：mingw-w64-ucrt-x86_64-ffmpeg（含头文件与 .dll.a）
- D:\work\github\YUView\build\YUViewApp\ffmpeg 仅有运行时 DLL（avcodec-61 等），无 include/lib，且与当前 MSYS2（avcodec-62）ABI 不兼容，故未采用该目录链接
- 本目录 in/ 从 MSYS2 拷贝，便于将 DLL 与可执行文件一并部署
