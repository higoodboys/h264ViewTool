# FFmpeg 集成说明

## 获取库

推荐使用 Windows 共享库构建（LGPL 动态链接更常见）：

- https://www.gyan.dev/ffmpeg/builds/ （`ffmpeg-release-full-shared`）
- 或自行用 MediaAutobuildToolbox 编译

解压后目录应类似：

```
ffmpeg/
  include/libavcodec/...
  lib/avcodec.lib ...
  bin/avcodec-61.dll ...
```

## CMake 配置

```powershell
cmake -B build -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/msvc2019_64" `
  -DH264TOOL_USE_FFMPEG=ON `
  -DH264TOOL_FFMPEG_ROOT="D:/libs/ffmpeg"
```

运行前将 `ffmpeg/bin` 与 Qt `bin` 加入 PATH。

## 许可提醒

分发时请附带 FFmpeg 对应许可证与源码获取方式；若链接 GPL 构建需整体遵循 GPL。
