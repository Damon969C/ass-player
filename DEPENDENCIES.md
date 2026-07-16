# 依赖说明

仓库只托管项目源码和已静态链接 Qt 6.8.2 的 Windows x86_64 程序，不托管 mpv、FFmpeg、Qt SDK、Qt 源码或它们的构建缓存。

## 运行依赖

| 命令 | 用途 | 获取方式 |
| --- | --- | --- |
| `mpv` | 视频播放、字幕渲染和 JSON IPC 控制 | [mpv 官方安装说明](https://mpv.io/installation/) |
| `ffmpeg` | 抽取和转换内嵌字幕 | [FFmpeg 官方下载页](https://ffmpeg.org/download.html) |
| `ffprobe` | 探测媒体流和内嵌字幕轨 | 随 FFmpeg 一同提供 |

程序按以下顺序查找这些工具：

1. 播放器同级的 `binaries/` 目录；
2. 播放器同级目录；
3. 当前工作目录的 `binaries/` 目录；
4. 当前工作目录的 `src-tauri/binaries/` 目录（兼容旧目录布局）；
5. 系统 `PATH`。

推荐的 Windows 目录布局：

```text
subtitle_sidecar_player_qt.exe
binaries/
  mpv.exe
  ffmpeg.exe
  ffprobe.exe
```

mpv 发行包可能还需要其自身的 DLL；这些文件属于 mpv 的运行依赖，不属于 Qt6 应用产物。

## 构建依赖

- CMake 3.21 或更高版本；
- 支持 C++20 的编译器；
- Qt 6.5 或更高版本，包含 Core、Gui、Widgets、Network；
- Windows 交叉构建使用 MinGW-w64；
- 静态单文件程序已使用 Qt 6.8.2 `qtbase` 和静态 MinGW runtime 验证。

动态开发构建可使用系统 Qt6；静态 Windows 构建需要由同一 MinGW 目标编译的静态 Qt6 前缀。完整命令见 [`docs/static-single-exe.md`](docs/static-single-exe.md)。

## Qt 许可提示

静态链接 Qt 会产生与动态链接不同的许可义务。发布者应根据其使用的 Qt Commercial 或开源许可确认源码、重新链接和许可证文本等要求；本说明不是法律意见。
