# 依赖说明

仓库托管项目源码、已静态链接 Qt 6.8.2 的 Windows x86_64 程序，以及满足 GPL 对应源码分发所需的 QtBase 6.8.2 源码归档；不托管 mpv、FFmpeg、Qt SDK 或构建缓存。

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

## 许可证

项目源码及静态组合程序采用 `GPL-3.0-only`。QtBase 6.8.2 在本次分发中同样选择 `GPL-3.0-only`；完整项目许可证见 [`LICENSE`](LICENSE)，Qt 对应源码、SBOM 和第三方通知见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

mpv、ffmpeg、ffprobe 未随仓库分发，不属于本项目 GPL 授权范围；用户取得和使用这些独立程序时应遵守其各自许可证。
