# Qt6 架构与调试说明

## 代码边界

- `MainWindow` 是 UI 编排层，只负责用户输入、列表/进度/音量显示、设置保存和调用服务。
- `MpvController` 是播放器控制层，负责启动/停止 mpv、传入 Qt 原生窗口 ID、JSON IPC、`request_id` 匹配和播放命令。
- `MediaTools` 是媒体工具层，负责查找 `mpv/ffmpeg/ffprobe`、内嵌字幕轨探测、ffmpeg 抽取、内存缓存和运行期字幕文件清理。
- `SubtitleParser` 是纯解析层，负责字幕字节解码和 `srt/vtt/ass/ssa/lrc` 解析，不依赖 UI 或 mpv。
- `subtitle_navigation` 是纯字幕导航逻辑，按当前播放位置选出上一条或下一条字幕，不依赖 UI 或 mpv。

## 主要流程

### 打开视频

1. `MainWindow::openMedia` 调用 `MediaTools::cleanupRuntimeSubtitleFiles` 清理上一视频运行期字幕。
2. `MpvController::openMedia` 使用 `--wid=<videoHost winId>` 启动 mpv，避免弹出独立窗口。
3. `MediaTools::probeSubtitleTracks` 运行 `ffprobe -show_streams` 获取内嵌字幕轨。
4. `MediaTools::resetEmbeddedCache` 建立当前媒体缓存键和轨道扩展名映射。
5. `MediaTools::prefetchEmbeddedSubtitles` 后台启动单个 ffmpeg 多输出提取流程。
6. UI 更新内嵌字幕下拉框、恢复音量/静音/画面字幕设置，并开始 250ms 播放状态轮询。

### 加载外部字幕

1. `SubtitleParser::parseFile` 读取并解析 `srt/ass/ssa/vtt/lrc`。
2. `MpvController::addSubtitle` 发送 `sub-add <path> select`。
3. `MainWindow::setCues` 刷新右侧字幕列表。
4. `MainWindow::syncSubtitleOverlay` 重新应用 `sub-visibility`，避免 `sub-add select` 让画面字幕状态回归默认。

### 选择内嵌字幕

1. `MediaTools::loadEmbeddedSubtitle` 优先读取内存缓存。
2. 缓存未命中时先尝试单轨 `-c:s copy`，失败再转 `srt`。
3. 字幕字节解析成功后刷新右侧字幕列表，并按开关决定是否展示加载耗时。
4. 当前设计不对内嵌侧栏选择调用 `MpvController::addSubtitle`：右侧列表只负责文本解析和跳转，内嵌画面字幕由独立的画面字幕下拉框通过 mpv 原生 `sid`/`track-list` 控制，避免侧栏选择自动改变画面字幕轨。
5. 抽取过程中产生的运行期字幕文件仍纳入 `MediaTools::cleanupRuntimeSubtitleFiles` / `discardRuntimeSubtitleFile` 清理；外部字幕仍通过 `MpvController::addSubtitle` / `sub-add <path> select` 加入 mpv，并跟随右侧列表。

### 播放联动

- 状态轮询读取 `time-pos`、`duration`、`pause`、`volume`、`mute`。
- 当前时间命中字幕区间时，右侧列表高亮并滚动到中间。
- 点击字幕或进度条时，先更新本地 UI，再发起 mpv 精确跳转，并短暂抑制自动滚动抖动。
- 左右键精确跳到上一条/下一条字幕的起始点，并保持当前播放/暂停状态；上下键调整音量 5，空格/K 播放暂停。

## 调试重点

- mpv 没有嵌入：检查 `videoHost_->winId()` 是否创建、mpv 参数是否包含 `--wid`、Windows 下是否被其他窗口层级遮挡。
- 控制命令偶发错乱：检查 `MpvController::request` 是否继续按 `request_id` 过滤响应，不要读到第一行就返回。
- 内嵌字幕显示为空：先确认 `ffprobe -show_streams` 输出是否包含 `codec_type=subtitle`，不要换成轻量探测。
- 内嵌字幕选择慢：看加载耗时中的 ffmpeg 阶段，确认后台多输出预取是否已完成并命中 `memory-cache`。
- 画面字幕开关失效：确认外部字幕加载和内嵌字幕选择后都调用了 `syncSubtitleOverlay`。
- 退出后残留字幕临时文件：检查 `MediaTools::cleanupRuntimeSubtitleFiles` 是否在打开新视频和窗口析构时执行。

## 新功能添加位置

- 新 UI 控件或快捷键：从 `MainWindow::buildUi`、`connectSignals`、`keyPressEvent` 开始。
- 新 mpv 控制命令：添加到 `MpvController`，通过 `sendCommand` 或 `getProperty` 实现。
- 新字幕格式：添加到 `SubtitleParser::parseBytes` 分发和独立解析函数。
- 新媒体工具：添加到 `MediaTools`，保持进程调用和缓存逻辑不进入 UI 层。
- 新持久化设置：使用 `QSettings`，在 `restoreSettings` 和 `saveSettings` 同步。

## 验收清单

1. 动态 Qt 构建能启动窗口。
2. 打开视频后 mpv 画面嵌入左侧区域，不弹独立窗口。
3. 进度、播放暂停、音量、静音、快捷键和 OSD 可用。
4. 外部 `srt/ass/ssa/vtt/lrc` 字幕可加载、显示、点击跳转。
5. 内嵌字幕轨可探测、选择、缓存命中，并在切换视频/退出后清理运行期字幕文件。
6. 静态 Qt 包在干净 Windows 上只需要播放器 exe 和允许例外的 `mpv.exe`、`ffmpeg.exe`、`ffprobe.exe`。
