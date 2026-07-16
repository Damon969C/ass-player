# ASS Player

ASS Player 是一个使用 Qt6/C++ 编写的 Windows 字幕侧栏播放器。它通过 mpv 嵌入视频画面，用 FFmpeg/ffprobe 处理内嵌字幕，并在右侧显示可点击跳转的字幕列表。

## 直接运行

仓库提供 Windows x86_64 静态 Qt6 单文件程序：

[`dist/windows-static/subtitle_sidecar_player_qt.exe`](dist/windows-static/subtitle_sidecar_player_qt.exe)

该程序已静态链接 Qt 6.8.2 和 MinGW runtime，不需要 Qt DLL；运行时仍需要外部的 `mpv.exe`、`ffmpeg.exe`、`ffprobe.exe`。可以把它们放入播放器同级的 `binaries/` 目录，或加入系统 `PATH`。工具下载位置、查找顺序和目录示例见 [`DEPENDENCIES.md`](DEPENDENCIES.md)。

当前程序 SHA-256：

```text
b191d87ac57828c2f65ee12060e291c5a2ea14886bbaebdb9702bbda10f15542
```

## 功能

- 使用 `mpv --wid` 将常见视频格式嵌入 Qt 视频区域；
- 使用 `ffprobe -show_streams` 探测内嵌字幕轨；
- 使用单个 ffmpeg 多输出进程预取内嵌字幕；
- 支持外部 `srt`、`ass/ssa`、`vtt`、`lrc` 字幕；
- 右侧字幕列表逐条显示、高亮当前字幕，点击即可跳转并继续播放；
- 支持视频/字幕拖拽、播放暂停、5 秒跳转、音量调节和进度条跳转；
- 持久化音量、静音、画面字幕和加载耗时显示设置；
- 使用 mpv 原生字幕轨控制画面字幕，侧栏字幕选择与画面字幕选择相互独立；
- 打开新媒体或退出时清理抽取产生的运行期字幕文件。

## 仓库内容

- `src/`：Qt Widgets UI、mpv 控制、媒体工具和字幕解析；
- `resources/`：应用资源和 Windows 图标；
- `cmake/`：Windows MinGW 交叉编译工具链；
- `scripts/`：Windows 构建入口；
- `docs/`：架构与静态单文件构建说明；
- `dist/windows-static/`：已验证的静态 Qt6 Windows 程序。

Qt 源码、Qt SDK、CMake/Ninja 构建树、动态 Qt 分发目录，以及 mpv/FFmpeg 二进制均由 `.gitignore` 排除。

## 构建

动态 Qt 开发构建：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建要求为 CMake 3.21+、C++20 编译器，以及包含 Core、Gui、Widgets、Network 的 Qt 6.5+。

Windows 静态 Qt 单 exe 构建请见 [`docs/static-single-exe.md`](docs/static-single-exe.md)。模块边界、调试入口和新增功能位置请见 [`docs/architecture.md`](docs/architecture.md)。

本机 Linux 到 Windows 的交叉编译入口：

```bash
QT_WINDOWS_PREFIX=/opt/qt6-windows-static \
  bash scripts/cross-build-windows.sh
```

`QT_WINDOWS_PREFIX` 必须是由同一 MinGW 目标构建的 Windows x86_64 静态 Qt6 安装前缀，不能使用 Linux 本机 Qt6 开发包。当前代码在 Windows 上使用系统 API 解码 GB18030、Shift-JIS 等 legacy 字幕编码，静态 Qt 只需要 `qtbase`。

## 验证状态

当前静态程序已剥离非必要符号，并用 `objdump` 确认不依赖 `Qt6*.dll`、`libstdc++-6.dll` 或 `libgcc_s_seh-1.dll`。它仍会使用 Windows 系统 DLL，以及明确声明的 mpv、ffmpeg、ffprobe 外部程序。最终兼容性应在干净的 Windows x86_64 环境中验收。

静态链接 Qt 的许可注意事项见 [`DEPENDENCIES.md`](DEPENDENCIES.md)。
