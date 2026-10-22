@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

if not exist "build\H264Tool.exe" (
  echo [错误] 未找到 build\H264Tool.exe，请先编译。
  exit /b 1
)

if not exist "D:\msys64\usr\bin\bash.exe" (
  echo [错误] 未找到 D:\msys64\usr\bin\bash.exe，请确认已安装 MSYS2。
  exit /b 1
)

rem Windows 路径 -> MSYS 路径（如 D:\work\... -> /d/work/...）
for /f "delims=" %%I in ('D:\msys64\usr\bin\cygpath.exe -u "%CD%"') do set "MSYS_ROOT=%%I"

echo ==> 仓库: %CD%
echo ==> 通过 MSYS2 UCRT64 执行 pack_dist.sh ...
"D:\msys64\usr\bin\bash.exe" -lc "cd '%MSYS_ROOT%' && bash scripts/pack_dist.sh"
set ERR=%ERRORLEVEL%

if %ERR% neq 0 (
  echo [失败] 退出码 %ERR%
  exit /b %ERR%
)

echo.
echo 完成。输出目录: %CD%\dist\H264Tool
explorer "%CD%\dist\H264Tool"
exit /b 0
