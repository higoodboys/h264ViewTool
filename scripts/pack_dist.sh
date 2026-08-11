#!/usr/bin/env bash
# 生成可分发的便携目录：dist/H264Tool
set -euo pipefail
export PATH="/ucrt64/bin:/usr/bin:$PATH"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/build/H264Tool.exe"
DIST="$ROOT/dist/H264Tool"

if [[ ! -f "$SRC" ]]; then
  echo "未找到 $SRC，请先编译"
  exit 1
fi

rm -rf "$DIST"
mkdir -p "$DIST"
cp -f "$SRC" "$DIST/"

echo "==> windeployqt6"
(
  cd "$DIST"
  windeployqt6 --release --no-translations --no-system-d3d-compiler --no-opengl-sw H264Tool.exe
)

echo "==> 收集 FFmpeg / MinGW 依赖 DLL"
python3 "$ROOT/scripts/collect_runtime_dlls.py" "$DIST/H264Tool.exe" "$DIST"

# 非运行必需
rm -f "$DIST"/_test_frame.png 2>/dev/null || true

cat > "$DIST/使用说明.txt" <<'EOF'
H264Tool 便携包
================
双击 H264Tool.exe 即可运行（Windows 10/11 x64）。

支持打开：.mp4 / .avi / .h264 / .264
可将文件拖到控制窗口或预览窗口。

请保持本目录内 DLL 与 platforms 等子目录完整，不要只拷贝 exe。
EOF

echo "==> 完成: $DIST"
du -sh "$DIST"
find "$DIST" -type f | wc -l
