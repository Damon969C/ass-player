# 第三方软件与源码通知

本文件说明仓库所分发静态程序中的第三方组件。各组件的原始许可证文本、版权声明和对应源码优先于本摘要。

## ASS Player

Copyright (C) 2026 ASS Player contributors.

项目原创源码及静态组合程序按 GNU General Public License version 3 only（`GPL-3.0-only`）分发，完整许可证见 [`LICENSE`](LICENSE)。

## QtBase 6.8.2

Copyright (C) The Qt Company Ltd. and other contributors.

静态程序包含 Qt 6.8.2 的 Core、Gui、Widgets、Network、Windows 平台插件及相关静态组件。本次分发从 Qt 的可选许可证中选择 `GPL-3.0-only`。

- 对应源码：[`third_party/qt/qtbase-everywhere-src-6.8.2.tar.xz`](third_party/qt/qtbase-everywhere-src-6.8.2.tar.xz)
- 源码 SHA-256：`012043ce6d411e6e8a91fdc4e05e6bedcfa10fcb1347d3c33908f7fdd10dfe05`
- 上游源码：[Qt 官方 QtBase 6.8.2 归档](https://download.qt.io/official_releases/qt/6.8/6.8.2/submodules/qtbase-everywhere-src-6.8.2.tar.xz)
- 构建 SBOM：[`dist/windows-static/qtbase-6.8.2.spdx`](dist/windows-static/qtbase-6.8.2.spdx)
- SBOM SHA-256：`59796a7fd254dd4442ce1922623076736d728f924a7cbe8dbc68077a1ecd2e51`
- 构建配置：[`docs/static-single-exe.md`](docs/static-single-exe.md)

提交前已将官方源码归档与实际构建使用的 QtBase 源码树递归比较，未发现差异。QtBase 源码归档中包含 Qt 自身及其捆绑第三方组件的许可证和版权文件；SPDX 2.3 SBOM 记录了本次 Qt 构建的版本、配置、组件关系和许可证标识。

## GCC 与 MinGW-w64 runtime

Windows 程序使用 GCC 14 MinGW-w64 工具链及 `-static` 构建。GCC runtime 的适用文件通常采用 GPLv3 并附带 GCC Runtime Library Exception 3.1；MinGW-w64 runtime 包含公共领域、ZPL、LGPL 及其他不同许可的文件。

构建环境随包提供的版权与许可证清单保存在：

- [`third_party/toolchain/gcc-mingw-w64-x86-64-copyright.txt`](third_party/toolchain/gcc-mingw-w64-x86-64-copyright.txt)
- [`third_party/toolchain/mingw-w64-common-copyright.txt`](third_party/toolchain/mingw-w64-common-copyright.txt)

使用的主要构建包版本为：

- `gcc-mingw-w64-x86-64` 14.2.0；
- `mingw-w64-common` 12.0.0；
- `mingw-w64-x86-64-dev` 12.0.0。

## 外部运行依赖

mpv、ffmpeg、ffprobe 仅由程序作为独立外部进程调用，没有包含在仓库或静态程序中。它们不属于本项目的 GPL 授权内容，取得和再分发时应分别遵守其上游许可证。
