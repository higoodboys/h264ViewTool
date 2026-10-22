# H264Tool — Windows H.264 码流分析与预览工具

功能对照 Elecard StreamEye Tools 2.0：解析 MP4 / AVI / 裸 H.264，图形化展示 I/P/B 帧分布，双独立窗口联动预览。

## 功能概览

| 能力 | 说明 |
|------|------|
| 输入格式 | `.mp4`（含 fMP4/fragmented）/ `.avi` / `.h264` / `.264` |
| NAL 分割 | 起始码（Annex-B）或长度前缀（AVCC） |
| 帧类型 | 解析 SPS/PPS + Slice Header，识别 I/P/B（含 IDR） |
| 条带图 | 横轴帧序号，纵轴帧大小；I 红 / P 蓝 / B 绿 |
| 信息面板 | 序号、类型、大小、QP、POC、参考列表、偏移等 |
| 双窗口 | 控制窗口 + 预览窗口，独立移动/缩放，选帧同步 |
| 打开方式 | 菜单、工具栏、拖拽到任一窗口 |

## 工程结构

```
h264tool/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── ARCHITECTURE.md      # 核心类设计与算法流程
│   └── UI_LAYOUT.md         # 界面布局草图
├── resources/
│   └── app.qrc
└── src/
    ├── main.cpp
    ├── core/                # 解析与分析（不依赖 UI）
    ├── decode/              # 解码抽象 + FFmpeg 实现
    └── ui/                  # Qt 双窗口界面
```

## 环境要求

- Windows 10/11 x64
- CMake ≥ 3.21
- Visual Studio 2019/2022（MSVC）或等价工具链
- Qt 6（Widgets）
- （可选）FFmpeg 共享库：`avformat` / `avcodec` / `avutil` / `swscale`

无 FFmpeg 时仍可完成码流解析与条带图分析；预览窗口显示占位提示。

## 构建

```powershell
# 配置（示例路径请按本机修改）
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.5.0/msvc2019_64" `
  -DH264TOOL_USE_FFMPEG=ON `
  -DH264TOOL_FFMPEG_ROOT="C:/ffmpeg"

cmake --build build --config Release
```

关闭 FFmpeg：

```powershell
cmake -B build -A x64 -DH264TOOL_USE_FFMPEG=OFF -DCMAKE_PREFIX_PATH="C:/Qt/6.x/msvc2019_64"
```

运行前请将 Qt 与 FFmpeg 的 `bin` 加入 `PATH`，或把对应 DLL 拷到可执行文件目录。

## 打包给别人用

在 MSYS2 UCRT64 下编译完成后执行：

```bash
bash scripts/pack_dist.sh
```

会生成精简运行目录：

- `dist/H264Tool/` — 仅含运行所需的 exe、DLL、`platforms` 等插件
- 也可再压缩整个文件夹分发

不要只拷贝 `build/H264Tool.exe`；`build/` 里还有 CMake/Ninja 中间文件，不必打包。

## 许可与参考

| 组件 | 用途 | 许可注意 |
|------|------|----------|
| 本项目自研解析器 | NAL/SPS/Slice/简易 MP4·AVI | 项目自有 |
| [FFmpeg](https://ffmpeg.org/) | 解复用与软解（可选） | LGPL/GPL，动态链接需遵守其条款 |
| [h264bitstream](https://github.com/aizvorski/h264bitstream) | 算法参考（Exp-Golomb、slice 字段语义） | LGPL；本仓库为重写实现，未直接拷贝源码 |
| Elecard StreamEye | UI/交互参考 | 商业软件，仅作功能对照，无代码引用 |

## 快速使用

1. 启动后打开「控制窗口」与「预览窗口」。
2. `文件 → 打开` 或拖入媒体文件。
3. 在条带图点击某一帧，信息面板与预览同步更新。
4. 使用底部导航：逐帧 / 播放 / 跳转。
