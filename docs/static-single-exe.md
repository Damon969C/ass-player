# Qt6 单 exe / 无 Qt DLL 分发方案

## 结论

要做到只分发一个播放器 exe，且运行时不释放 Qt DLL 或插件 DLL，推荐使用静态编译 Qt6，然后把本项目链接到静态 Qt。普通 Qt 在线安装器提供的是动态 Qt，使用 `windeployqt` 会复制 `Qt6Core.dll`、`Qt6Widgets.dll`、平台插件等 DLL，不符合本项目的干净分发目标。

`mpv.exe`、`ffmpeg.exe`、`ffprobe.exe` 是外部运行依赖，不纳入单 exe；它们可保留在 `binaries/` 或系统 `PATH`。

## 推荐路线

1. 使用 MSVC 或 MinGW 构建静态 Qt6，保持应用和 Qt 使用同一编译器/运行库。
2. Qt 配置使用 `-static -static-runtime`，并关闭不需要的模块，至少保留 `qtbase` 中的 Core、Gui、Widgets、Network；GB18030、Shift-JIS 等 legacy 字幕编码在 Windows 目标上通过系统 API 解码，避免额外引入 Core5Compat/QML 依赖。TLS 优先使用 Windows Schannel，避免 OpenSSL DLL。
3. 用静态 Qt 的 `qt-cmake` 配置本目录，再构建 `subtitle_sidecar_player_qt.exe`；MSVC 应设置 `/MT`，本目录 CMake 已设置 `MSVC_RUNTIME_LIBRARY`。
4. 如果使用图片、平台主题或 TLS 等插件，需要静态导入插件；本项目当前显式导入 Windows 平台插件 `Qt6::QWindowsIntegrationPlugin`，目标是避免额外插件 DLL。
5. 用干净 Windows 虚拟机验证：exe 同级只保留 `binaries/mpv.exe`、`binaries/ffmpeg.exe`、`binaries/ffprobe.exe`，不放任何 Qt DLL，启动并完成播放/字幕验收。

## 示例命令轮廓

以下命令是方案骨架，实际 Qt 源码版本、编译器路径和安装前缀按本机调整：

```bat
configure.bat -prefix C:\Qt\6-static-msvc -release -static -static-runtime -opensource -confirm-license -nomake examples -nomake tests -skip qtwebengine -schannel -no-openssl
cmake --build . --parallel
cmake --install .

C:\Qt\6-static-msvc\bin\qt-cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Release
cmake --build build-static --config Release
```

Linux 本机交叉编译到 Windows 时，可使用本目录提供的入口脚本：

```bash
QT_WINDOWS_PREFIX=/opt/qt6-windows-static bash scripts/cross-build-windows.sh
```

这里的 `QT_WINDOWS_PREFIX` 必须包含 Windows x86_64 目标的 `Qt6Config.cmake` 和可供当前 Linux 主机运行的 `moc/uic/rcc` 等 Qt host tools。只有 Linux 本机 Qt6 开发包或只有 MinGW 编译器都不够。

已验证的静态 Qt 6.8.2 交叉编译命令如下。先把路径变量替换为本机位置；`QT_BUILD` 和 `QT_PREFIX` 应放在仓库外，避免把 Qt 源码、SDK 或构建缓存提交到项目仓库。

```bash
REPO_ROOT="$PWD"
QT_SRC=/path/to/qt-everywhere-src-6.8.2
QT_BUILD=/path/to/qt-static-build-6.8.2
QT_PREFIX=/opt/qt6-windows-static

cmake -E make_directory "$QT_BUILD"
cd "$QT_BUILD"

"$QT_SRC/configure" \
  -cmake-generator Ninja \
  -release \
  -static \
  -static-runtime \
  -prefix "$QT_PREFIX" \
  -qt-host-path /usr \
  -submodules qtbase \
  -nomake examples \
  -nomake tests \
  -opensource \
  -confirm-license \
  -schannel \
  -no-openssl \
  -no-dbus \
  -no-opengl \
  -no-feature-vulkan \
  -qt-zlib \
  -qt-pcre \
  -qt-freetype \
  -qt-harfbuzz \
  -qt-libpng \
  -qt-libjpeg \
  -no-feature-sql \
  -no-feature-printsupport \
  -- \
  -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/cmake/windows-mingw-toolchain.cmake" \
  -DQT_WINDOWS_PREFIX=/usr/x86_64-w64-mingw32 \
  -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/x86_64-linux-gnu/cmake

cmake --build "$QT_BUILD" --parallel 1
cmake --install "$QT_BUILD"
```

随后用静态前缀构建播放器：

```bash
QT_WINDOWS_PREFIX="$QT_PREFIX" \
BUILD_DIR="$REPO_ROOT/build-windows-static" \
QT_HOST_PREFIX=/usr \
bash "$REPO_ROOT/scripts/cross-build-windows.sh"
```

生成仓库中的发布产物：

```bash
cmake -E make_directory "$REPO_ROOT/dist/windows-static"
cmake -E copy \
  "$REPO_ROOT/build-windows-static/subtitle_sidecar_player_qt.exe" \
  "$REPO_ROOT/dist/windows-static/subtitle_sidecar_player_qt.exe"
x86_64-w64-mingw32-strip --strip-unneeded \
  "$REPO_ROOT/dist/windows-static/subtitle_sidecar_player_qt.exe"
```

当前仓库产物剥离非必要符号后大小约 24 MiB。`objdump` 依赖检查未出现 `Qt6*.dll`、`libstdc++-6.dll`、`libgcc_s_seh-1.dll`，只剩 Windows 系统 DLL。

## 许可证约束

静态链接 Qt LGPL 版本会带来重新链接等合规义务；商业闭源分发通常需要 Qt Commercial License。若无法接受静态 LGPL 义务，应改用动态 Qt 分发，但动态分发必然需要 Qt DLL/插件 DLL。

## 风险和验证

- MSVC 静态运行库可减少 VC Runtime DLL，但需要确认所有第三方库也使用兼容运行库。
- OpenSSL、图像格式、样式、平台插件等模块如果被动态依赖，会破坏单 exe 目标。
- `mpv.exe` 本身可能依赖其自带 DLL；若使用外部 `mpv.exe`，这些依赖属于 mpv 分发问题，不属于 Qt 单 exe 范围。
- 最终验收以干净 Windows 环境启动结果为准，而不是开发机上的 PATH。
