#include "main_window.h"

#include "subtitle_parser.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractButton>
#include <QColor>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QCursor>
#include <QEasingCurve>
#include <QMetaObject>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QRegion>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextOption>
#include <QTextLayout>
#include <QThread>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int kSubtitleCardRadius = 12;
constexpr int kSubtitleItemMarginX = 4;
constexpr int kSubtitleItemMarginY = 4;
constexpr int kSubtitleItemPaddingX = 14;
constexpr int kSubtitleItemPaddingY = 10;
constexpr int kSubtitleTextVerticalSafetyMargin = 3;
constexpr qreal kSubtitleTextLineHeightRatio = 1.45;
constexpr int kSubtitleTimeColumnWidth = 58;
constexpr int kSubtitleTextGap = 10;
constexpr int kSubtitleSizeFallbackWidth = 320;
constexpr int kWindowControlsWidth = 118;
constexpr int kWindowControlsHeight = 38;
constexpr int kWindowControlsMargin = 4;
constexpr int kWindowControlsRevealPadding = 8;
constexpr int kResizeBorderWidth = 8;
constexpr int kTopResizeBorderWidth = 6;
constexpr int kMoveZoneHeight = 86;
constexpr int kPictureSubtitleNone = -1;
constexpr int kPictureSubtitleAuto = -2;
constexpr int kPictureSubtitleExternalFollow = -3;
constexpr qint64 kManualSubtitleScrollSuppressMs = 2000;
constexpr int kSubtitleScrollAnimationMs = 240;
constexpr int kRootLayoutMargin = 18;
constexpr int kRootLayoutSpacing = 18;
constexpr int kPlayerLayoutSpacing = 14;
constexpr int kVideoShellMargin = 4;
constexpr int kImmersiveControlsMargin = 18;
constexpr int kImmersiveControlsHideMs = 2000;
constexpr int kPlaybackStatePollMs = 250;
constexpr int kRepeatPlaybackStatePollMs = 50;
constexpr qreal kVideoHostCornerRadius = 18.0;

const QColor kSubtitleItemBackground("#101826");
const QColor kSubtitleItemHoverBackground("#142033");
const QColor kSubtitleItemSelectedBackground("#123244");
const QColor kSubtitleItemBorder("#223146");
const QColor kSubtitleItemSelectedBorder("#38bdf8");
const QColor kSubtitleTextColor("#d7e3ef");
const QColor kSubtitleSelectedTextColor("#f8fafc");
const QColor kSubtitleTimeColor("#7dd3fc");
const QColor kComboArrowColor("#7dd3fc");
const QColor kComboArrowDisabledColor("#64748b");

void refreshWidgetStyle(QWidget *widget)
{
    if (!widget) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void enableMouseTrackingForTree(QWidget *root)
{
    if (!root) {
        return;
    }
    root->setMouseTracking(true);
    root->setAttribute(Qt::WA_Hover, true);
    for (QWidget *child : root->findChildren<QWidget *>()) {
        child->setMouseTracking(true);
        child->setAttribute(Qt::WA_Hover, true);
    }
}

void addComboItemWithTooltip(QComboBox *combo, const QString &label, const QVariant &data = QVariant())
{
    combo->addItem(label, data);
    const int index = combo->count() - 1;
    combo->setItemData(index, label, Qt::ToolTipRole);
}

QTextOption subtitleTextOption()
{
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setAlignment(Qt::AlignLeft);
    return textOption;
}

qreal subtitleLineHeight(const QFont &font)
{
    const QFontMetricsF metrics(font);
    return std::max(metrics.height(), metrics.lineSpacing() * kSubtitleTextLineHeightRatio);
}

qreal layoutSubtitleParagraph(QTextLayout &layout, int width, const QFont &font)
{
    layout.setTextOption(subtitleTextOption());
    layout.beginLayout();
    qreal height = 0.0;
    const qreal targetLineHeight = subtitleLineHeight(font);
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(std::max(1, width));
        const qreal lineHeight = std::max(targetLineHeight, line.height());
        line.setPosition(QPointF(0.0, height + ((lineHeight - line.height()) / 2.0)));
        height += lineHeight;
    }
    layout.endLayout();
    return height;
}

QStringList subtitleTextParagraphs(const QString &text)
{
    return (text.isEmpty() ? QStringLiteral(" ") : text).split('\n', Qt::KeepEmptyParts);
}

qreal layoutSubtitleTextHeight(const QString &text, const QFont &font, int width)
{
    qreal height = 0.0;
    for (const QString &paragraph : subtitleTextParagraphs(text)) {
        QTextLayout layout(paragraph.isEmpty() ? QStringLiteral(" ") : paragraph, font);
        height += layoutSubtitleParagraph(layout, width, font);
    }
    return height;
}

void drawSubtitleText(QPainter *painter, const QString &text, const QFont &font, const QRect &rect)
{
    qreal y = rect.top();
    for (const QString &paragraph : subtitleTextParagraphs(text)) {
        QTextLayout layout(paragraph.isEmpty() ? QStringLiteral(" ") : paragraph, font);
        const qreal paragraphHeight = layoutSubtitleParagraph(layout, rect.width(), font);
        layout.draw(painter, QPointF(rect.left(), y));
        y += paragraphHeight;
    }
}

int wrappedSubtitleTextHeight(const QString &text, const QFont &font, int width)
{
    return static_cast<int>(std::ceil(layoutSubtitleTextHeight(text, font, width)));
}

int subtitleItemWidth(const QStyleOptionViewItem &option)
{
    const auto *view = qobject_cast<const QListView *>(option.widget);
    if (view && view->viewport() && view->viewport()->width() > 0) {
        return view->viewport()->width();
    }
    return option.rect.width() > 0 ? option.rect.width() : kSubtitleSizeFallbackWidth;
}

QRect subtitleTextRectForItemRect(const QRect &itemRect)
{
    const QRect cardRect = itemRect.adjusted(kSubtitleItemMarginX, kSubtitleItemMarginY, -kSubtitleItemMarginX, -kSubtitleItemMarginY);
    const QRect contentRect = cardRect.adjusted(kSubtitleItemPaddingX, kSubtitleItemPaddingY, -kSubtitleItemPaddingX, -kSubtitleItemPaddingY);
    const int textLeft = contentRect.left() + kSubtitleTimeColumnWidth + kSubtitleTextGap;
    return QRect(textLeft,
        contentRect.top() + kSubtitleTextVerticalSafetyMargin,
        std::max(80, contentRect.right() - textLeft + 1),
        std::max(1, contentRect.height() - (2 * kSubtitleTextVerticalSafetyMargin)));
}

class ChevronComboBox final : public QComboBox {
public:
    explicit ChevronComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QComboBox::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(isEnabled() ? kComboArrowColor : kComboArrowDisabledColor, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QRect dropDownRect = style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, this);
        const QPointF center = QRectF(dropDownRect.isValid() ? dropDownRect : rect()).center();
        painter.drawLine(QPointF(center.x() - 4.5, center.y() - 2.0), QPointF(center.x(), center.y() + 2.5));
        painter.drawLine(QPointF(center.x(), center.y() + 2.5), QPointF(center.x() + 4.5, center.y() - 2.0));
    }
};

class SubtitleItemDelegate final : public QStyledItemDelegate {
public:
    explicit SubtitleItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const int itemWidth = subtitleItemWidth(option);
        const QRect textRect = subtitleTextRectForItemRect(QRect(0, 0, itemWidth, 1));
        const QString text = index.data(Qt::UserRole + 2).toString().isEmpty()
            ? index.data(Qt::DisplayRole).toString()
            : index.data(Qt::UserRole + 2).toString();
        const int textHeight = wrappedSubtitleTextHeight(text, option.font, textRect.width());
        const int contentHeight = static_cast<int>(std::ceil(std::max(subtitleLineHeight(option.font), static_cast<qreal>(textHeight))));
        const int height = contentHeight + (2 * kSubtitleTextVerticalSafetyMargin) + (2 * kSubtitleItemPaddingY) + (2 * kSubtitleItemMarginY);
        return QSize(itemWidth, height);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const QRect itemRect(option.rect.left(), option.rect.top(), subtitleItemWidth(option), option.rect.height());
        const QRect cardRect = itemRect.adjusted(kSubtitleItemMarginX, kSubtitleItemMarginY, -kSubtitleItemMarginX, -kSubtitleItemMarginY);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(selected ? kSubtitleItemSelectedBorder : kSubtitleItemBorder, selected ? 1.4 : 1.0));
        painter->setBrush(selected ? kSubtitleItemSelectedBackground : hovered ? kSubtitleItemHoverBackground : kSubtitleItemBackground);
        painter->drawRoundedRect(QRectF(cardRect).adjusted(0.5, 0.5, -0.5, -0.5), kSubtitleCardRadius, kSubtitleCardRadius);

        const QRect contentRect = cardRect.adjusted(kSubtitleItemPaddingX, kSubtitleItemPaddingY, -kSubtitleItemPaddingX, -kSubtitleItemPaddingY);
        const QRect timeRect(contentRect.left(), contentRect.top() + kSubtitleTextVerticalSafetyMargin, kSubtitleTimeColumnWidth, std::max(1, contentRect.height() - (2 * kSubtitleTextVerticalSafetyMargin)));
        const QRect textRect = subtitleTextRectForItemRect(itemRect);
        const QString subtitleText = index.data(Qt::UserRole + 2).toString().isEmpty()
            ? index.data(Qt::DisplayRole).toString()
            : index.data(Qt::UserRole + 2).toString();

        QFont timeFont = option.font;
        timeFont.setBold(true);
        painter->setFont(timeFont);
        painter->setPen(kSubtitleTimeColor);
        painter->drawText(timeRect, Qt::AlignLeft | Qt::AlignTop, index.data(Qt::UserRole + 1).toString());

        painter->setFont(option.font);
        painter->setPen(selected ? kSubtitleSelectedTextColor : kSubtitleTextColor);
        drawSubtitleText(painter, subtitleText, option.font, textRect);
        painter->restore();
    }
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("字幕侧栏播放器"));
    setAcceptDrops(true);
    buildUi();
    restoreSettings();
    connectSignals();
    stateTimer_.setInterval(kPlaybackStatePollMs);
}

MainWindow::~MainWindow()
{
    if (immersiveMode_) {
        exitImmersivePlayback();
    }
    stopSubtitleScrollAnimation();
    ++embeddedSubtitleLoadRequestId_;
    qApp->removeEventFilter(this);
    for (QThread *thread : std::as_const(subtitleLoadThreads_)) {
        if (!thread) {
            continue;
        }
        thread->disconnect(this);
        thread->wait();
        delete thread;
    }
    subtitleLoadThreads_.clear();
    saveSettings();
    mediaTools_.cleanupRuntimeSubtitleFiles();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appSurface"));
    rootLayout_ = new QHBoxLayout(central);
    rootLayout_->setContentsMargins(kRootLayoutMargin, kRootLayoutMargin, kRootLayoutMargin, kRootLayoutMargin);
    rootLayout_->setSpacing(kRootLayoutSpacing);

    playerPane_ = new QWidget(central);
    playerPane_->setObjectName(QStringLiteral("playerPane"));
    playerLayout_ = new QVBoxLayout(playerPane_);
    playerLayout_->setContentsMargins(0, 0, 0, 0);
    playerLayout_->setSpacing(kPlayerLayoutSpacing);

    toolbarStrip_ = new QWidget(playerPane_);
    toolbarStrip_->setObjectName(QStringLiteral("toolbarStrip"));
    auto *toolbar = new QHBoxLayout(toolbarStrip_);
    toolbar->setContentsMargins(12, 10, 12, 10);
    toolbar->setSpacing(10);
    openVideoButton_ = new QPushButton(QStringLiteral("打开视频"), playerPane_);
    openSubtitleButton_ = new QPushButton(QStringLiteral("手动添加字幕"), playerPane_);
    playPauseButton_ = new QPushButton(QStringLiteral("播放/暂停"), playerPane_);
    playPauseButton_->setObjectName(QStringLiteral("playPauseButton"));
    repeatToggle_ = new QCheckBox(QStringLiteral("重复"), playerPane_);
    repeatToggle_->setObjectName(QStringLiteral("repeatToggle"));
    repeatToggle_->setToolTip(QStringLiteral("逐条练习：下一条字幕前暂停，间隔过长时保留 2 秒尾音；空格重播当前字幕"));
    overlayToggle_ = new QCheckBox(QStringLiteral("画面字幕"), playerPane_);
    pictureSubtitleSelect_ = new ChevronComboBox(playerPane_);
    auto *pictureSubtitleSelectPopup = new QListView(pictureSubtitleSelect_);
    pictureSubtitleSelectPopup->setObjectName(QStringLiteral("pictureSubtitleSelectPopup"));
    pictureSubtitleSelectPopup->setMouseTracking(true);
    pictureSubtitleSelectPopup->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    pictureSubtitleSelect_->setView(pictureSubtitleSelectPopup);
    pictureSubtitleSelectPopupViewport_ = pictureSubtitleSelectPopup->viewport();
    pictureSubtitleSelect_->setMinimumWidth(190);
    QSizePolicy pictureSubtitleSelectPolicy = pictureSubtitleSelect_->sizePolicy();
    pictureSubtitleSelectPolicy.setRetainSizeWhenHidden(true);
    pictureSubtitleSelect_->setSizePolicy(pictureSubtitleSelectPolicy);
    pictureSubtitleSelect_->setToolTip(QStringLiteral("选择 mpv 画面字幕轨道"));
    toolbar->addWidget(openVideoButton_);
    toolbar->addWidget(openSubtitleButton_);
    toolbar->addWidget(playPauseButton_);
    toolbar->addWidget(repeatToggle_);
    toolbar->addStretch();
    toolbar->addWidget(pictureSubtitleSelect_);
    toolbar->addWidget(overlayToggle_);
    playerLayout_->addWidget(toolbarStrip_);

    videoShell_ = new QWidget(playerPane_);
    videoShell_->setObjectName(QStringLiteral("videoShell"));
    videoShellLayout_ = new QVBoxLayout(videoShell_);
    videoShellLayout_->setContentsMargins(kVideoShellMargin, kVideoShellMargin, kVideoShellMargin, kVideoShellMargin);
    videoShellLayout_->setSpacing(0);

    videoHost_ = new QWidget(videoShell_);
    videoHost_->setObjectName(QStringLiteral("videoHost"));
    videoHost_->setAttribute(Qt::WA_NativeWindow, true);
    videoHost_->setMinimumHeight(360);
    videoHost_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *videoLayout = new QVBoxLayout(videoHost_);
    videoLayout->setContentsMargins(24, 24, 24, 24);
    videoLayout->setAlignment(Qt::AlignCenter);
    mediaStatus_ = new QLabel(QStringLiteral("尚未加载媒体"), videoHost_);
    mediaStatus_->setObjectName(QStringLiteral("mediaStatus"));
    mediaStatus_->setAlignment(Qt::AlignCenter);
    videoLayout->addWidget(mediaStatus_);
    videoShellLayout_->addWidget(videoHost_);
    playerLayout_->addWidget(videoShell_, 1);

    controlStrip_ = new QWidget(playerPane_);
    controlStrip_->setObjectName(QStringLiteral("controlStrip"));
    auto *statusbar = new QHBoxLayout(controlStrip_);
    statusbar->setContentsMargins(12, 12, 12, 12);
    statusbar->setSpacing(12);
    timeLabel_ = new QLabel(QStringLiteral("00:00 / 00:00"), playerPane_);
    progressSlider_ = new QSlider(Qt::Horizontal, playerPane_);
    progressSlider_->setRange(0, 1000);
    muteButton_ = new QPushButton(QStringLiteral("🔊"), playerPane_);
    volumeSlider_ = new QSlider(Qt::Horizontal, playerPane_);
    volumeSlider_->setRange(0, 100);
    fullscreenButton_ = new QPushButton(QStringLiteral("全屏"), playerPane_);
    fullscreenButton_->setObjectName(QStringLiteral("fullscreenButton"));
    fullscreenButton_->setMinimumWidth(58);
    fullscreenButton_->setEnabled(false);
    fullscreenButton_->setToolTip(QStringLiteral("沉浸播放；外层窗口最大化时进入系统全屏"));
    statusbar->addWidget(timeLabel_);
    statusbar->addWidget(progressSlider_, 1);
    statusbar->addWidget(muteButton_);
    statusbar->addWidget(volumeSlider_);
    statusbar->addWidget(fullscreenButton_);
    playerLayout_->addWidget(controlStrip_);

    subtitlePane_ = new QWidget(central);
    subtitlePane_->setObjectName(QStringLiteral("subtitlePane"));
    auto *subtitleLayout = new QVBoxLayout(subtitlePane_);
    subtitleLayout->setContentsMargins(14, 14, 14, 14);
    subtitleLayout->setSpacing(10);
    embeddedSelect_ = new ChevronComboBox(subtitlePane_);
    auto *embeddedSelectPopup = new QListView(embeddedSelect_);
    embeddedSelectPopup->setObjectName(QStringLiteral("embeddedSelectPopup"));
    embeddedSelectPopup->setMouseTracking(true);
    embeddedSelectPopup->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    embeddedSelect_->setView(embeddedSelectPopup);
    embeddedSelectPopupViewport_ = embeddedSelectPopup->viewport();
    timingLogToggle_ = new QCheckBox(QStringLiteral("显示加载耗时"), subtitlePane_);
    subtitleStatus_ = new QLabel(QStringLiteral("可拖入 srt/ass/lrc 字幕"), subtitlePane_);
    subtitleStatus_->setObjectName(QStringLiteral("subtitleStatus"));
    subtitleStatus_->setWordWrap(true);
    loadingOverlay_ = new QLabel(QStringLiteral("正在加载内嵌字幕..."), subtitlePane_);
    loadingOverlay_->setAlignment(Qt::AlignCenter);
    loadingOverlay_->hide();
    subtitleList_ = new QListWidget(subtitlePane_);
    subtitleList_->setItemDelegate(new SubtitleItemDelegate(subtitleList_));
    subtitleList_->setFocusPolicy(Qt::StrongFocus);
    subtitleList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    subtitleList_->setTextElideMode(Qt::ElideNone);
    subtitleList_->setWordWrap(true);
    subtitleList_->setResizeMode(QListView::Adjust);
    subtitleList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    subtitleList_->setSelectionMode(QAbstractItemView::SingleSelection);
    subtitleList_->setMouseTracking(true);
    subtitleList_->setUniformItemSizes(false);
    subtitleList_->setSpacing(2);
    subtitleListViewport_ = subtitleList_->viewport();
    subtitleListScrollBar_ = subtitleList_->verticalScrollBar();
    if (subtitleListScrollBar_) {
        subtitleScrollAnimation_ = new QPropertyAnimation(subtitleListScrollBar_, "value", this);
        subtitleScrollAnimation_->setDuration(kSubtitleScrollAnimationMs);
        subtitleScrollAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    }
    videoHost_->installEventFilter(this);
    progressSlider_->installEventFilter(this);
    subtitleList_->installEventFilter(this);
    if (subtitleListViewport_) {
        subtitleListViewport_->installEventFilter(this);
    }
    if (subtitleListScrollBar_) {
        subtitleListScrollBar_->installEventFilter(this);
    }
    if (pictureSubtitleSelectPopupViewport_) {
        pictureSubtitleSelectPopupViewport_->installEventFilter(this);
    }
    if (embeddedSelectPopupViewport_) {
        embeddedSelectPopupViewport_->installEventFilter(this);
    }
    subtitleLayout->addWidget(embeddedSelect_);
    subtitleLayout->addWidget(timingLogToggle_);
    subtitleLayout->addWidget(subtitleStatus_);
    subtitleLayout->addWidget(loadingOverlay_);
    subtitleLayout->addWidget(subtitleList_, 1);

    rootLayout_->addWidget(playerPane_, 1);
    rootLayout_->addWidget(subtitlePane_, 0);
    subtitlePane_->setFixedWidth(390);
    setCentralWidget(central);

    windowControls_ = new QWidget(central);
    windowControls_->setObjectName(QStringLiteral("windowControls"));
    auto *windowControlsLayout = new QHBoxLayout(windowControls_);
    windowControlsLayout->setContentsMargins(4, 4, 4, 4);
    windowControlsLayout->setSpacing(4);
    minimizeButton_ = new QToolButton(windowControls_);
    maximizeButton_ = new QToolButton(windowControls_);
    closeButton_ = new QToolButton(windowControls_);
    const QList<QToolButton *> buttons = {minimizeButton_, maximizeButton_, closeButton_};
    for (QToolButton *button : buttons) {
        button->setObjectName(QStringLiteral("windowControlButton"));
        button->setAutoRaise(true);
        button->setFixedSize(32, 30);
        button->setFocusPolicy(Qt::TabFocus);
        windowControlsLayout->addWidget(button);
    }
    closeButton_->setObjectName(QStringLiteral("windowCloseButton"));
    minimizeButton_->setText(QStringLiteral("−"));
    closeButton_->setText(QStringLiteral("×"));
    updateMaximizeButton();
    windowControls_->setFixedSize(kWindowControlsWidth, kWindowControlsHeight);
    windowControls_->hide();
    updateWindowControlGeometry();

    setStyleSheet(R"(
        QMainWindow { background:#050912; }
        QWidget { color:#e5edf7; font-family:'Microsoft YaHei UI','Segoe UI',sans-serif; font-size:14px; }
        QWidget#appSurface { background:qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #111b2b, stop:0.42 #07101d, stop:0.72 #091827, stop:1 #152033); }
        QWidget#appSurface[immersive="true"] { background:#01040a; }
        QWidget#playerPane { background:transparent; }
        QWidget#toolbarStrip, QWidget#controlStrip, QWidget#subtitlePane { background:qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 rgba(18, 29, 47, 226), stop:1 rgba(8, 15, 27, 222)); border:1px solid rgba(75, 102, 137, 120); border-radius:18px; }
        QWidget#controlStrip[immersive="true"] { background:rgba(8, 15, 27, 238); border:1px solid rgba(125, 211, 252, 100); border-radius:16px; }
        QWidget#windowControls { background:rgba(10, 17, 29, 236); border:1px solid rgba(125, 211, 252, 92); border-radius:16px; }
        QWidget#videoShell { background:qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #0d1726, stop:0.36 #030712, stop:1 #0a1422); border:1px solid rgba(56, 189, 248, 62); border-radius:22px; }
        QWidget#videoHost { background:#01040a; border:1px solid rgba(148, 163, 184, 36); border-radius:18px; }
        QWidget#videoShell[immersive="true"], QWidget#videoHost[immersive="true"] { background:#01040a; border:0; border-radius:0; }
        QLabel { background:transparent; }
        QLabel#mediaStatus { color:#bae6fd; font-size:16px; font-weight:600; }
        QLabel#subtitleStatus { color:#a8b8ca; line-height:145%; }
        QPushButton, QComboBox { border:1px solid rgba(96, 165, 250, 96); border-radius:11px; background:qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #18263b, stop:1 #101a2b); color:#f8fafc; selection-background-color:#123244; selection-color:#f8fafc; }
        QPushButton { padding:8px 13px; }
        QPushButton#fullscreenButton { font-weight:600; padding:8px 11px; }
        QComboBox { padding:8px 42px 8px 13px; }
        QPushButton:hover, QComboBox:hover { border-color:#7dd3fc; background:qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #20324b, stop:1 #132239); }
        QPushButton:pressed { background:#0e7490; border-color:#67e8f9; }
        QToolButton#windowControlButton, QToolButton#windowCloseButton { border:0; border-radius:10px; background:transparent; color:#d7e3ef; font-size:15px; font-weight:700; padding:0; }
        QToolButton#windowControlButton:hover { background:#1d2b40; color:#f8fafc; }
        QToolButton#windowControlButton:pressed { background:#0e7490; color:#f8fafc; }
        QToolButton#windowCloseButton:hover { background:#ef4444; color:#ffffff; }
        QToolButton#windowCloseButton:pressed { background:#b91c1c; color:#ffffff; }
        QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:34px; border-left:1px solid rgba(96, 165, 250, 72); border-top-right-radius:11px; border-bottom-right-radius:11px; background:rgba(56, 189, 248, 18); }
        QComboBox::down-arrow { image:none; width:16px; height:16px; border:0; background:transparent; }
        QComboBox:disabled { color:#7f8da3; background:#101827; border-color:rgba(71, 85, 105, 110); }
        QComboBox::drop-down:disabled { border-left-color:rgba(71, 85, 105, 72); background:rgba(15, 23, 42, 96); }
        QComboBox::down-arrow:disabled { image:none; border:0; background:transparent; }
        QComboBox QAbstractItemView, QListView#embeddedSelectPopup { outline:0; background:#0b1422; color:#e5edf7; border:1px solid rgba(96, 165, 250, 112); border-radius:12px; padding:6px; selection-background-color:#123244; selection-color:#f8fafc; }
        QComboBox QAbstractItemView::item, QListView#embeddedSelectPopup::item { min-height:30px; padding:7px 10px; border-radius:8px; color:#d7e3ef; background:transparent; }
        QComboBox QAbstractItemView::item:hover, QListView#embeddedSelectPopup::item:hover { color:#f8fafc; background:#142033; }
        QComboBox QAbstractItemView::item:selected, QListView#embeddedSelectPopup::item:selected { color:#f8fafc; background:#123244; border:1px solid rgba(56, 189, 248, 126); }
        QComboBox QAbstractItemView::item:disabled, QListView#embeddedSelectPopup::item:disabled { color:#728198; background:transparent; }
        QCheckBox { spacing:8px; color:#d7e3ef; background:transparent; }
        QCheckBox::indicator { width:17px; height:17px; border-radius:5px; border:1px solid rgba(96, 165, 250, 96); background:#0f172a; }
        QCheckBox::indicator:checked { background:#38bdf8; border-color:#7dd3fc; }
        QSlider::groove:horizontal { height:7px; background:#1f2a3d; border-radius:3px; }
        QSlider::sub-page:horizontal { background:#38bdf8; border-radius:3px; }
        QSlider::handle:horizontal { width:16px; background:#f8fafc; border:2px solid #38bdf8; margin:-6px 0; border-radius:8px; }
        QListWidget { outline:0; background:rgba(7, 15, 27, 206); border:1px solid rgba(75, 102, 137, 104); border-radius:15px; padding:8px; }
        QListWidget::item { outline:0; border:0; background:transparent; }
        QListWidget::item:selected, QListWidget::item:focus { outline:0; border:0; background:transparent; }
        QScrollBar:vertical { background:rgba(7, 15, 27, 190); width:10px; margin:6px 2px 6px 0; border-radius:5px; }
        QScrollBar::handle:vertical { background:#26364d; border-radius:5px; min-height:32px; }
        QScrollBar::handle:vertical:hover { background:#35506d; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { height:0; background:transparent; }
    )");
    enableMouseTrackingForTree(central);
    updateVideoHostMask();
}

void MainWindow::connectSignals()
{
    connect(openVideoButton_, &QPushButton::clicked, this, &MainWindow::openVideoDialog);
    connect(openSubtitleButton_, &QPushButton::clicked, this, &MainWindow::openSubtitleDialog);
    connect(playPauseButton_, &QPushButton::clicked, this, [this]() { togglePlayPause(true); });
    connect(repeatToggle_, &QCheckBox::toggled, this, &MainWindow::setRepeatMode);
    connect(overlayToggle_, &QCheckBox::toggled, this, [this]() {
        settings_.setValue("subtitle-overlay", overlayToggle_->isChecked());
        syncSubtitleOverlay();
    });
    connect(timingLogToggle_, &QCheckBox::toggled, this, [this](bool checked) {
        settings_.setValue("timing-log", checked);
    });
    connect(embeddedSelect_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::selectEmbeddedSubtitle);
    connect(pictureSubtitleSelect_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::selectPictureSubtitle);
    connect(muteButton_, &QPushButton::clicked, this, [this]() { setMuted(!lastState_.muted); });
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int value) { applyVolume(value, true); });
    connect(fullscreenButton_, &QPushButton::clicked, this, &MainWindow::toggleImmersivePlayback);
    connect(minimizeButton_, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(maximizeButton_, &QToolButton::clicked, this, &MainWindow::toggleMaximized);
    connect(closeButton_, &QToolButton::clicked, this, &MainWindow::close);
    connect(subtitleList_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int cueId = item->data(Qt::UserRole).toInt();
        const auto it = std::find_if(cues_.begin(), cues_.end(), [cueId](const SubtitleCue &cue) { return cue.id == cueId; });
        if (it != cues_.end()) {
            suppressAutoScrollUntil_ = 0;
            seekTo(it->startSeconds, false);
            if (repeatToggle_->isChecked()) {
                repeatCueId_ = it->id;
            }
            setActiveCue(it->id, true);
        }
    });
    connect(&stateTimer_, &QTimer::timeout, this, &MainWindow::pollPlaybackState);
    connect(&windowControlRevealTimer_, &QTimer::timeout, this, &MainWindow::updateWindowControlVisibility);
    immersiveControlsTimer_ = new QTimer(this);
    immersiveControlsTimer_->setObjectName(QStringLiteral("immersiveControlsTimer"));
    immersiveControlsTimer_->setSingleShot(true);
    immersiveControlsTimer_->setInterval(kImmersiveControlsHideMs);
    connect(immersiveControlsTimer_, &QTimer::timeout, this, &MainWindow::hideImmersiveControls);
    windowControlRevealTimer_.setInterval(120);
    windowControlRevealTimer_.start();
    qApp->installEventFilter(this);
}

void MainWindow::openVideoDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开视频"), QString(), QStringLiteral("视频 (*.mp4 *.mkv *.avi *.mov *.wmv *.flv *.webm *.m4v *.ts *.m2ts *.mpg *.mpeg *.3gp *.ogv)"));
    if (!path.isEmpty()) {
        openMedia(path);
    }
}

void MainWindow::openSubtitleDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("手动添加字幕"), QString(), QStringLiteral("字幕/歌词 (*.srt *.ass *.ssa *.vtt *.lrc)"));
    if (!path.isEmpty()) {
        loadExternalSubtitle(path);
    }
}

void MainWindow::openMedia(const QString &path)
{
    QString error;
    ++embeddedSubtitleLoadRequestId_;
    loadingOverlay_->hide();
    openSubtitleButton_->setEnabled(true);
    mediaTools_.cleanupRuntimeSubtitleFiles();
    if (!mpv_.openMedia(mediaTools_.findTool("mpv"), path, videoHost_->winId(), &error)) {
        embeddedSelect_->setEnabled(mediaLoaded_ && !tracks_.isEmpty());
        setStatus(QStringLiteral("打开视频失败: %1").arg(error));
        return;
    }

    currentMediaPath_ = path;
    mediaLoaded_ = true;
    fullscreenButton_->setEnabled(true);
    mediaStatus_->setText(basename(path));
    tracks_ = mediaTools_.probeSubtitleTracks(path, &error);
    mediaTools_.resetEmbeddedCache(path, tracks_);
    mediaTools_.prefetchEmbeddedSubtitles(path, tracks_);
    populatePictureSubtitleSelect();

    embeddedSelect_->blockSignals(true);
    embeddedSelect_->clear();
    if (tracks_.isEmpty()) {
        addComboItemWithTooltip(embeddedSelect_, QStringLiteral("无内嵌字幕"));
        embeddedSelect_->setEnabled(false);
    } else {
        embeddedSelect_->setEnabled(true);
        addComboItemWithTooltip(embeddedSelect_, QStringLiteral("选择内嵌字幕"));
        for (const SubtitleTrack &track : tracks_) {
            addComboItemWithTooltip(embeddedSelect_, subtitleTrackLabel(track), track.id);
        }
    }
    embeddedSelect_->blockSignals(false);

    setCues({}, tracks_.isEmpty() ? QStringLiteral("无内嵌字幕，可手动添加字幕") : QStringLiteral("请选择内嵌字幕或手动加载字幕"));
    applyVolume(settings_.value("volume", 100).toInt(), false);
    setMuted(settings_.value("muted", false).toBool());
    syncSubtitleOverlay();
    stateTimer_.start();
}

void MainWindow::loadExternalSubtitle(const QString &path)
{
    const int requestId = ++embeddedSubtitleLoadRequestId_;
    const QString mediaPath = currentMediaPath_;
    embeddedSelect_->setEnabled(mediaLoaded_ && !tracks_.isEmpty());
    if (!isSubtitlePath(path)) {
        setStatus(QStringLiteral("请拖入 srt、ass、ssa、vtt 或 lrc 文件"));
        return;
    }
    loadingOverlay_->setText(QStringLiteral("正在加载字幕..."));
    loadingOverlay_->show();
    openSubtitleButton_->setEnabled(false);
    setStatus(QStringLiteral("正在加载字幕，请稍候..."));

    QThread *thread = QThread::create([this, requestId, mediaPath, path]() {
        QString error;
        const QVector<SubtitleCue> loaded = SubtitleParser::parseFile(path, &error);
        QMetaObject::invokeMethod(this, [this, requestId, mediaPath, path, loaded, error]() {
            finishExternalSubtitleLoad(requestId, mediaPath, path, loaded, error);
        }, Qt::QueuedConnection);
    });
    subtitleLoadThreads_.push_back(thread);
    connect(thread, &QThread::finished, this, [this, thread]() {
        subtitleLoadThreads_.removeAll(thread);
        thread->deleteLater();
    });
    thread->start();
}

void MainWindow::finishExternalSubtitleLoad(int requestId, const QString &mediaPath, const QString &subtitlePath, const QVector<SubtitleCue> &cues, const QString &error)
{
    const bool stale = requestId != embeddedSubtitleLoadRequestId_ || mediaPath != currentMediaPath_;
    if (stale) {
        return;
    }

    loadingOverlay_->hide();
    openSubtitleButton_->setEnabled(true);
    embeddedSelect_->setEnabled(mediaLoaded_ && !tracks_.isEmpty());
    if (cues.isEmpty()) {
        setStatus(QStringLiteral("加载字幕失败: %1").arg(error));
        return;
    }
    QString mpvError;
    if (mediaLoaded_ && !mpv_.addSubtitle(subtitlePath, &mpvError)) {
        setStatus(QStringLiteral("加载字幕到 mpv 失败: %1").arg(mpvError));
        return;
    }
    setPictureSubtitleExternalFollowing();
    setCues(cues, QStringLiteral("已加载字幕：%1").arg(basename(subtitlePath)));
    syncSubtitleOverlay();
}

void MainWindow::selectEmbeddedSubtitle(int index)
{
    if (!mediaLoaded_ || index <= 0) {
        return;
    }
    const int trackId = embeddedSelect_->itemData(index).toInt();
    const QString mediaPath = currentMediaPath_;
    const int requestId = ++embeddedSubtitleLoadRequestId_;
    loadingOverlay_->setText(QStringLiteral("正在加载内嵌字幕..."));
    loadingOverlay_->show();
    embeddedSelect_->setEnabled(false);
    setStatus(QStringLiteral("正在加载内嵌字幕，请稍候..."));

    QThread *thread = QThread::create([this, requestId, mediaPath, trackId]() {
        QString error;
        EmbeddedSubtitleLoadResult result = mediaTools_.loadEmbeddedSubtitle(mediaPath, trackId, &error);
        QMetaObject::invokeMethod(this, [this, requestId, mediaPath, trackId, result, error]() {
            finishEmbeddedSubtitleLoad(requestId, mediaPath, trackId, result, error);
        }, Qt::QueuedConnection);
    });
    subtitleLoadThreads_.push_back(thread);
    connect(thread, &QThread::finished, this, [this, thread]() {
        subtitleLoadThreads_.removeAll(thread);
        thread->deleteLater();
    });
    thread->start();
}

void MainWindow::finishEmbeddedSubtitleLoad(int requestId, const QString &mediaPath, int trackId, EmbeddedSubtitleLoadResult result, const QString &error)
{
    const bool stale = requestId != embeddedSubtitleLoadRequestId_ || mediaPath != currentMediaPath_ || !mediaLoaded_;
    if (stale) {
        mediaTools_.discardRuntimeSubtitleFile(result.runtimePath);
        return;
    }

    if (result.cues.isEmpty()) {
        loadingOverlay_->hide();
        openSubtitleButton_->setEnabled(true);
        embeddedSelect_->setEnabled(mediaLoaded_ && !tracks_.isEmpty());
        setStatus(QStringLiteral("加载内嵌字幕失败: %1").arg(error));
        return;
    }
    mediaTools_.discardRuntimeSubtitleFile(result.runtimePath);
    if (pictureSubtitleSelect_->count() == 1 && pictureSubtitleSelect_->itemData(0).toInt() == kPictureSubtitleExternalFollow) {
        populatePictureSubtitleSelect();
    }
    setCues(result.cues, QStringLiteral("已加载内嵌字幕轨 %1").arg(trackId));
    syncSubtitleOverlay();
    if (timingLogToggle_->isChecked()) {
        setStatus(QStringLiteral("已加载内嵌字幕轨 %1，共 %2 条；策略=%3 ffmpeg=%4ms parse=%5ms mpv=%6ms")
            .arg(trackId).arg(result.cues.size()).arg(result.extractStrategy)
            .arg(result.ffmpegExtractMs).arg(result.parseMs).arg(result.mpvIpcMs));
    }
    loadingOverlay_->hide();
    openSubtitleButton_->setEnabled(true);
    embeddedSelect_->setEnabled(mediaLoaded_ && !tracks_.isEmpty());
}

void MainWindow::selectPictureSubtitle(int index)
{
    if (!mediaLoaded_ || index < 0 || !pictureSubtitleSelect_->isEnabled()) {
        return;
    }

    const QVariant selectionData = pictureSubtitleSelect_->itemData(index);
    const int selection = selectionData.isValid() ? selectionData.toInt() : kPictureSubtitleNone;
    QString error;
    bool ok = false;
    if (selection == kPictureSubtitleNone) {
        ok = mpv_.disablePictureSubtitle(&error);
    } else if (selection == kPictureSubtitleAuto) {
        ok = mpv_.setAutoPictureSubtitle(&error);
    } else if (selection >= 0) {
        ok = mpv_.selectPictureSubtitle(selection, &error);
    } else {
        return;
    }
    if (!ok) {
        setStatus(QStringLiteral("切换画面字幕轨失败: %1").arg(error));
    }
}

void MainWindow::populatePictureSubtitleSelect()
{
    pictureSubtitleTracks_.clear();
    pictureSubtitleSelect_->blockSignals(true);
    pictureSubtitleSelect_->clear();

    if (!mediaLoaded_) {
        addComboItemWithTooltip(pictureSubtitleSelect_, QStringLiteral("无媒体"), kPictureSubtitleNone);
        pictureSubtitleSelect_->setEnabled(false);
        pictureSubtitleSelect_->blockSignals(false);
        updatePictureSubtitleSelectVisibility();
        return;
    }
    if (tracks_.isEmpty()) {
        addComboItemWithTooltip(pictureSubtitleSelect_, QStringLiteral("无内嵌"), kPictureSubtitleNone);
        pictureSubtitleSelect_->setEnabled(false);
        pictureSubtitleSelect_->blockSignals(false);
        updatePictureSubtitleSelectVisibility();
        return;
    }

    QVector<MpvSubtitleTrack> mpvTracks;
    QString error;
    if (!mpv_.subtitleTracks(&mpvTracks, &error)) {
        setPictureSubtitleUnavailable(QStringLiteral("读取失败"));
        pictureSubtitleSelect_->blockSignals(false);
        updatePictureSubtitleSelectVisibility();
        return;
    }

    for (const SubtitleTrack &ffprobeTrack : std::as_const(tracks_)) {
        const auto match = std::find_if(mpvTracks.cbegin(), mpvTracks.cend(), [&ffprobeTrack](const MpvSubtitleTrack &mpvTrack) {
            return mpvTrack.ffIndex == ffprobeTrack.id;
        });
        if (match != mpvTracks.cend()) {
            pictureSubtitleTracks_.push_back(*match);
        }
    }

    if (pictureSubtitleTracks_.isEmpty()) {
        setPictureSubtitleUnavailable(QStringLiteral("轨道不可映射"));
        pictureSubtitleSelect_->blockSignals(false);
        updatePictureSubtitleSelectVisibility();
        return;
    }

    addComboItemWithTooltip(pictureSubtitleSelect_, QStringLiteral("无"), kPictureSubtitleNone);
    addComboItemWithTooltip(pictureSubtitleSelect_, QStringLiteral("自动"), kPictureSubtitleAuto);
    for (const SubtitleTrack &track : std::as_const(tracks_)) {
        const auto match = std::find_if(pictureSubtitleTracks_.cbegin(), pictureSubtitleTracks_.cend(), [&track](const MpvSubtitleTrack &mpvTrack) {
            return mpvTrack.ffIndex == track.id;
        });
        if (match != pictureSubtitleTracks_.cend()) {
            addComboItemWithTooltip(pictureSubtitleSelect_, subtitleTrackLabel(track), match->id);
        }
    }
    pictureSubtitleSelect_->setCurrentIndex(0);
    pictureSubtitleSelect_->setEnabled(true);
    pictureSubtitleSelect_->blockSignals(false);
    updatePictureSubtitleSelectVisibility();
}

void MainWindow::setPictureSubtitleExternalFollowing()
{
    pictureSubtitleTracks_.clear();
    pictureSubtitleSelect_->blockSignals(true);
    pictureSubtitleSelect_->clear();
    addComboItemWithTooltip(pictureSubtitleSelect_, QStringLiteral("跟随外部字幕"), kPictureSubtitleExternalFollow);
    pictureSubtitleSelect_->setEnabled(false);
    pictureSubtitleSelect_->blockSignals(false);
    updatePictureSubtitleSelectVisibility();
}

void MainWindow::setPictureSubtitleUnavailable(const QString &label)
{
    pictureSubtitleTracks_.clear();
    pictureSubtitleSelect_->clear();
    addComboItemWithTooltip(pictureSubtitleSelect_, label, kPictureSubtitleNone);
    pictureSubtitleSelect_->setEnabled(false);
    updatePictureSubtitleSelectVisibility();
}

void MainWindow::updatePictureSubtitleSelectVisibility()
{
    pictureSubtitleSelect_->setVisible(overlayToggle_->isChecked());
}

void MainWindow::handleDroppedPath(const QString &path)
{
    if (isVideoPath(path)) {
        openMedia(path);
    } else if (isSubtitlePath(path)) {
        loadExternalSubtitle(path);
    } else {
        setStatus(QStringLiteral("请拖入视频文件或 srt、ass、ssa、vtt、lrc 字幕文件"));
    }
}

void MainWindow::togglePlayPause(bool showHint)
{
    if (!mediaLoaded_) return;
    QString error;
    bool paused = false;
    if (!mpv_.togglePlayPause(&paused, &error)) {
        setStatus(QStringLiteral("播放/暂停失败: %1").arg(error));
        return;
    }
    lastState_.paused = paused;
    if (showHint) {
        showOsd(paused ? QStringLiteral("暂停") : QStringLiteral("播放"));
    }
}

void MainWindow::handleSpacePlaybackAction()
{
    if (repeatToggle_->isChecked()) {
        repeatCurrentSubtitle();
        return;
    }
    togglePlayPause(true);
}

void MainWindow::setRepeatMode(bool enabled)
{
    repeatCueId_ = -1;
    stateTimer_.setInterval(enabled ? kRepeatPlaybackStatePollMs : kPlaybackStatePollMs);
    if (enabled) {
        if (const SubtitleCue *cue = resolveRepeatCue(lastState_.position, lastState_.paused)) {
            repeatCueId_ = cue->id;
            setActiveCue(cue->id, false);
        }
    }
    if (mediaLoaded_) {
        showOsd(enabled ? QStringLiteral("重复模式：开启") : QStringLiteral("重复模式：关闭"));
    }
}

void MainWindow::repeatCurrentSubtitle()
{
    if (!mediaLoaded_) return;
    if (cues_.isEmpty()) {
        showOsd(QStringLiteral("未加载侧栏字幕"));
        return;
    }

    const SubtitleCue *cue = resolveRepeatCue(lastState_.position, true);
    if (!cue) {
        showOsd(QStringLiteral("没有可重复的字幕"));
        return;
    }

    QString error;
    if (!mpv_.seekTo(cue->startSeconds, &error)) {
        setStatus(QStringLiteral("重复播放失败: %1").arg(error));
        return;
    }

    repeatCueId_ = cue->id;
    lastState_.paused = false;
    suppressAutoScrollUntil_ = 0;
    updateTimeline(cue->startSeconds, lastState_.duration);
    setActiveCue(cue->id, true);
    showOsd(QStringLiteral("重复: %1").arg(formatClock(cue->startSeconds)));
}

void MainWindow::seekToAdjacentSubtitle(SubtitleNavigationDirection direction)
{
    if (!mediaLoaded_) return;

    if (cues_.isEmpty()) {
        showOsd(QStringLiteral("未加载侧栏字幕"));
        return;
    }

    const SubtitleCue *targetCue = nullptr;
    if (repeatToggle_->isChecked()) {
        const SubtitleCue *anchorCue = resolveRepeatCue(lastState_.position, true);
        if (anchorCue) {
            targetCue = findAdjacentSubtitleCueById(cues_, anchorCue->id, direction);
        }
    } else {
        targetCue = findAdjacentSubtitleCue(cues_, lastState_.position, direction);
    }
    if (!targetCue) {
        showOsd(direction == SubtitleNavigationDirection::Previous
                ? QStringLiteral("没有上一条字幕")
                : QStringLiteral("没有下一条字幕"));
        return;
    }

    QString error;
    if (!mpv_.seekAbsolute(targetCue->startSeconds, &error)) {
        setStatus(QStringLiteral("跳转失败: %1").arg(error));
        return;
    }

    if (repeatToggle_->isChecked()) {
        repeatCueId_ = targetCue->id;
    }
    suppressAutoScrollUntil_ = QDateTime::currentMSecsSinceEpoch() + 800;
    updateTimeline(targetCue->startSeconds, lastState_.duration);
    setActiveCue(targetCue->id, true);
    showOsd(QStringLiteral("%1: %2")
            .arg(direction == SubtitleNavigationDirection::Previous
                    ? QStringLiteral("上一条字幕")
                    : QStringLiteral("下一条字幕"),
                formatClock(targetCue->startSeconds)));
}

void MainWindow::seekTo(double seconds, bool scroll)
{
    if (!mediaLoaded_) return;
    QString error;
    if (!mpv_.seekTo(seconds, &error)) {
        setStatus(QStringLiteral("跳转失败: %1").arg(error));
        return;
    }
    updateTimeline(seconds, lastState_.duration);
    if (const SubtitleCue *cue = findSeekCue(seconds)) {
        if (repeatToggle_->isChecked()) {
            repeatCueId_ = cue->id;
        }
        if (scroll || cue->id != activeCueId_) {
            setActiveCue(cue->id, scroll);
        }
    }
    showOsd(QStringLiteral("搜索: %1").arg(formatClock(seconds)));
}

void MainWindow::applyVolume(int volume, bool showHint)
{
    if (!mediaLoaded_) {
        volumeSlider_->setValue(volume);
        return;
    }
    QString error;
    double applied = volume;
    if (!mpv_.setVolume(volume, &applied, &error)) {
        setStatus(QStringLiteral("音量调整失败: %1").arg(error));
        return;
    }
    lastState_.volume = applied;
    lastState_.muted = false;
    settings_.setValue("volume", qRound(applied));
    settings_.setValue("muted", false);
    updateVolumeUi(applied, false);
    if (showHint) {
        showVolumeOsd(applied);
    }
}

void MainWindow::setMuted(bool muted)
{
    if (!mediaLoaded_) {
        lastState_.muted = muted;
        updateVolumeUi(lastState_.volume, muted);
        return;
    }
    QString error;
    if (!mpv_.setMuted(muted, &error)) {
        setStatus(QStringLiteral("静音切换失败: %1").arg(error));
        return;
    }
    lastState_.muted = muted;
    settings_.setValue("muted", muted);
    updateVolumeUi(lastState_.volume, muted);
    showVolumeOsd(muted ? 0.0 : lastState_.volume);
}

void MainWindow::syncSubtitleOverlay()
{
    updatePictureSubtitleSelectVisibility();
    if (!mediaLoaded_) return;
    QString error;
    if (!mpv_.setSubtitleOverlay(overlayToggle_->isChecked(), &error)) {
        setStatus(QStringLiteral("切换画面字幕失败: %1").arg(error));
    }
}

void MainWindow::pollPlaybackState()
{
    PlaybackState state;
    QString error;
    if (!mpv_.playbackState(&state, &error)) {
        stateTimer_.stop();
        return;
    }
    const SubtitleCue *cue = findActiveCue(state.position);
    if (repeatToggle_->isChecked() && !findSubtitleCueById(cues_, repeatCueId_)) {
        if (const SubtitleCue *repeatCue = resolveRepeatCue(state.position, state.paused)) {
            repeatCueId_ = repeatCue->id;
        }
    }
    const bool stoppedAtRepeatBoundary = pauseAtRepeatBoundary(&state);
    if (stoppedAtRepeatBoundary) {
        cue = findSubtitleCueById(cues_, repeatCueId_);
    }
    lastState_ = state;
    updateTimeline(state.position, state.duration);
    updateVolumeUi(state.volume, state.muted);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (cue && cue->id != activeCueId_) {
        setActiveCue(cue->id, now >= suppressAutoScrollUntil_);
    }
    if (suppressAutoScrollUntil_ > 0 && now >= suppressAutoScrollUntil_) {
        suppressAutoScrollUntil_ = 0;
        if (activeCueId_ >= 0) {
            setActiveCue(activeCueId_, true);
        }
    }
}

void MainWindow::setCues(const QVector<SubtitleCue> &cues, const QString &message)
{
    stopSubtitleScrollAnimation();
    cues_ = cues;
    activeCueId_ = -1;
    repeatCueId_ = -1;
    subtitleList_->clear();
    for (const SubtitleCue &cue : cues_) {
        auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(formatTime(cue.startSeconds), cue.text));
        item->setData(Qt::UserRole, cue.id);
        item->setData(Qt::UserRole + 1, formatTime(cue.startSeconds));
        item->setData(Qt::UserRole + 2, cue.text);
        subtitleList_->addItem(item);
    }
    subtitleList_->doItemsLayout();
    subtitleListViewportWidth_ = subtitleListViewport_ ? subtitleListViewport_->width() : -1;
    if (repeatToggle_->isChecked()) {
        if (const SubtitleCue *cue = resolveRepeatCue(lastState_.position, lastState_.paused)) {
            repeatCueId_ = cue->id;
            setActiveCue(cue->id, false);
        }
    }
    setStatus(cues_.isEmpty() ? message : QStringLiteral("%1，共 %2 条").arg(message).arg(cues_.size()));
}

void MainWindow::setActiveCue(int cueId, bool scroll)
{
    activeCueId_ = cueId;
    for (int row = 0; row < subtitleList_->count(); ++row) {
        QListWidgetItem *item = subtitleList_->item(row);
        if (item->data(Qt::UserRole).toInt() == cueId) {
            QScrollBar *scrollBar = subtitleListScrollBar_ ? subtitleListScrollBar_ : subtitleList_->verticalScrollBar();
            const int scrollBarValue = scrollBar ? scrollBar->value() : 0;
            stopSubtitleScrollAnimation();
            subtitleList_->setCurrentItem(item);
            if (scrollBar) {
                scrollBar->setValue(scrollBarValue);
            }
            if (scroll) {
                const QRect itemRect = subtitleList_->visualItemRect(item);
                if (!scrollBar || !subtitleListViewport_ || !itemRect.isValid()) {
                    subtitleList_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                } else {
                    const int centerOffset = (subtitleListViewport_->height() - itemRect.height()) / 2;
                    const int targetValue = std::clamp(scrollBar->value() + itemRect.top() - centerOffset, scrollBar->minimum(), scrollBar->maximum());
                    if (subtitleScrollAnimation_ && targetValue != scrollBar->value()) {
                        subtitleScrollAnimation_->setTargetObject(scrollBar);
                        subtitleScrollAnimation_->setPropertyName("value");
                        subtitleScrollAnimation_->setStartValue(scrollBar->value());
                        subtitleScrollAnimation_->setEndValue(targetValue);
                        subtitleScrollAnimation_->start();
                    } else {
                        scrollBar->setValue(targetValue);
                    }
                }
            }
            return;
        }
    }
}

void MainWindow::suppressSubtitleAutoScrollForManualNavigation()
{
    stopSubtitleScrollAnimation();
    suppressAutoScrollUntil_ = QDateTime::currentMSecsSinceEpoch() + kManualSubtitleScrollSuppressMs;
}

void MainWindow::stopSubtitleScrollAnimation()
{
    if (subtitleScrollAnimation_ && subtitleScrollAnimation_->state() != QAbstractAnimation::Stopped) {
        subtitleScrollAnimation_->stop();
    }
}

void MainWindow::refreshSubtitleListLayoutForWidthChange()
{
    if (!subtitleList_ || !subtitleListViewport_) {
        return;
    }
    const int viewportWidth = subtitleListViewport_->width();
    if (viewportWidth == subtitleListViewportWidth_) {
        return;
    }
    subtitleListViewportWidth_ = viewportWidth;
    subtitleList_->doItemsLayout();
    subtitleListViewport_->update();
}

const SubtitleCue *MainWindow::findActiveCue(double seconds) const
{
    for (const SubtitleCue &cue : cues_) {
        const double end = cue.endSeconds >= 0.0 ? cue.endSeconds : std::numeric_limits<double>::infinity();
        if (seconds >= cue.startSeconds && seconds < end) {
            return &cue;
        }
    }
    return nullptr;
}

const SubtitleCue *MainWindow::findSeekCue(double seconds) const
{
    if (const SubtitleCue *active = findActiveCue(seconds)) {
        return active;
    }
    for (const SubtitleCue &cue : cues_) {
        if (cue.startSeconds >= seconds) {
            return &cue;
        }
    }
    return cues_.isEmpty() ? nullptr : &cues_.last();
}

const SubtitleCue *MainWindow::resolveRepeatCue(double seconds, bool preferHighlightedCue) const
{
    if (const SubtitleCue *selected = findSubtitleCueById(cues_, repeatCueId_)) {
        return selected;
    }
    if (preferHighlightedCue) {
        if (const SubtitleCue *highlighted = findSubtitleCueById(cues_, activeCueId_)) {
            return highlighted;
        }
    }
    if (const SubtitleCue *active = findActiveCue(seconds)) {
        return active;
    }
    return findSeekCue(seconds);
}

bool MainWindow::pauseAtRepeatBoundary(PlaybackState *state)
{
    if (!state || !repeatToggle_->isChecked() || state->paused) {
        return false;
    }
    const SubtitleCue *cue = findSubtitleCueById(cues_, repeatCueId_);
    if (!cue) {
        return false;
    }
    const double boundary = subtitleCueRepeatBoundary(cues_, cue->id, state->duration);
    if (boundary < 0.0 || state->position < boundary) {
        return false;
    }

    QString error;
    if (!mpv_.setPaused(true, &error)) {
        setStatus(QStringLiteral("重复模式暂停失败: %1").arg(error));
        return false;
    }
    state->paused = true;

    QString seekError;
    if (mpv_.seekAbsolute(boundary, &seekError)) {
        state->position = boundary;
    } else {
        setStatus(QStringLiteral("重复模式定位失败: %1").arg(seekError));
    }
    suppressAutoScrollUntil_ = 0;
    setActiveCue(cue->id, true);
    return true;
}

void MainWindow::updateTimeline(double position, double duration)
{
    lastState_.position = position;
    lastState_.duration = duration;
    timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(position), formatTime(duration)));
    progressSlider_->blockSignals(true);
    progressSlider_->setValue(duration > 0.0 ? qRound((position / duration) * progressSlider_->maximum()) : 0);
    progressSlider_->blockSignals(false);
}

void MainWindow::updateVolumeUi(double volume, bool muted)
{
    volumeSlider_->blockSignals(true);
    volumeSlider_->setValue(qRound(std::clamp(volume, 0.0, 100.0)));
    volumeSlider_->blockSignals(false);
    muteButton_->setText(muted || volume <= 0.0 ? QStringLiteral("🔇") : volume < 50.0 ? QStringLiteral("🔉") : QStringLiteral("🔊"));
}

void MainWindow::setStatus(const QString &message)
{
    subtitleStatus_->setText(message);
}

void MainWindow::showVolumeOsd(double volume)
{
    showOsd(QStringLiteral("音量: %1%").arg(qRound(volume), 2, 10, QLatin1Char('0')));
}

void MainWindow::showOsd(const QString &text)
{
    if (!mediaLoaded_) return;
    QString error;
    if (!mpv_.showOsd(text, &error)) {
        setStatus(QStringLiteral("OSD 显示失败: %1").arg(error));
    }
}

void MainWindow::toggleImmersivePlayback()
{
    if (immersiveMode_) {
        exitImmersivePlayback();
    } else {
        enterImmersivePlayback();
    }
}

void MainWindow::enterImmersivePlayback()
{
    if (immersiveMode_ || !rootLayout_ || !playerLayout_ || !videoShellLayout_ || !controlStrip_) {
        return;
    }

    immersiveMode_ = true;
    immersiveWindowMode_ = isMaximized()
        ? ImmersiveWindowMode::SystemFullScreen
        : ImmersiveWindowMode::InWindow;
    toolbarStrip_->hide();
    subtitlePane_->hide();
    rootLayout_->setContentsMargins(0, 0, 0, 0);
    rootLayout_->setSpacing(0);
    playerLayout_->setSpacing(0);
    videoShellLayout_->setContentsMargins(0, 0, 0, 0);
    playerLayout_->removeWidget(controlStrip_);
    setImmersiveSurfaceStyle(true);

    fullscreenButton_->setText(QStringLiteral("退出"));
    fullscreenButton_->setToolTip(QStringLiteral("退出沉浸播放 (Esc)"));
    if (immersiveWindowMode_ == ImmersiveWindowMode::SystemFullScreen) {
        enterSystemFullscreenImmersive();
    }
    showImmersiveControls();
    updateWindowControlGeometry();
    updateWindowControlVisibility();
    QTimer::singleShot(0, this, &MainWindow::positionImmersiveControls);
}

void MainWindow::exitImmersivePlayback()
{
    if (!immersiveMode_) {
        return;
    }

    const bool wasSystemFullscreen = immersiveWindowMode_ == ImmersiveWindowMode::SystemFullScreen;
    immersiveMode_ = false;
    if (immersiveControlsTimer_) {
        immersiveControlsTimer_->stop();
    }
    if (centralWidget()) {
        centralWidget()->unsetCursor();
    }
    if (wasSystemFullscreen) {
        leaveSystemFullscreenImmersive();
    }

    controlStrip_->show();
    if (playerLayout_->indexOf(controlStrip_) < 0) {
        playerLayout_->addWidget(controlStrip_);
    }
    rootLayout_->setContentsMargins(kRootLayoutMargin, kRootLayoutMargin, kRootLayoutMargin, kRootLayoutMargin);
    rootLayout_->setSpacing(kRootLayoutSpacing);
    playerLayout_->setSpacing(kPlayerLayoutSpacing);
    videoShellLayout_->setContentsMargins(kVideoShellMargin, kVideoShellMargin, kVideoShellMargin, kVideoShellMargin);
    toolbarStrip_->show();
    subtitlePane_->show();
    setImmersiveSurfaceStyle(false);

    fullscreenButton_->setText(QStringLiteral("全屏"));
    fullscreenButton_->setToolTip(QStringLiteral("沉浸播放；外层窗口最大化时进入系统全屏"));
    immersiveWindowMode_ = ImmersiveWindowMode::InWindow;
    updateMaximizeButton();
    updateWindowControlGeometry();
    updateWindowControlVisibility();
}

void MainWindow::showImmersiveControls()
{
    if (!immersiveMode_ || !controlStrip_) {
        return;
    }
    QWidget *immersiveSurface = immersiveFullscreenWindow_
        ? immersiveFullscreenWindow_
        : centralWidget();
    if (immersiveSurface) {
        immersiveSurface->unsetCursor();
    }
    controlStrip_->show();
    positionImmersiveControls();
    controlStrip_->raise();
    if (immersiveControlsTimer_) {
        immersiveControlsTimer_->start();
    }
}

void MainWindow::hideImmersiveControls()
{
    if (!immersiveMode_ || !controlStrip_) {
        return;
    }
    if (QApplication::mouseButtons() != Qt::NoButton) {
        immersiveControlsTimer_->start();
        return;
    }
    controlStrip_->hide();
    QWidget *immersiveSurface = immersiveFullscreenWindow_
        ? immersiveFullscreenWindow_
        : centralWidget();
    if (immersiveSurface) {
        immersiveSurface->setCursor(Qt::BlankCursor);
    }
}

void MainWindow::positionImmersiveControls()
{
    if (!immersiveMode_ || !playerPane_ || !controlStrip_) {
        return;
    }
    const QRect available = playerPane_->rect();
    const int margin = std::min(kImmersiveControlsMargin, std::max(0, available.width() / 4));
    const int preferredHeight = std::max(1, controlStrip_->sizeHint().height());
    const int height = std::min(preferredHeight, std::max(1, available.height() - (2 * margin)));
    const int width = std::max(1, available.width() - (2 * margin));
    controlStrip_->setGeometry(margin, std::max(0, available.height() - height - margin), width, height);
}

void MainWindow::setImmersiveSurfaceStyle(bool immersive)
{
    const QVariant value(immersive);
    if (centralWidget()) {
        centralWidget()->setProperty("immersive", value);
        refreshWidgetStyle(centralWidget());
    }
    for (QWidget *widget : {videoShell_, videoHost_, controlStrip_}) {
        widget->setProperty("immersive", value);
        refreshWidgetStyle(widget);
    }
    updateVideoHostMask();
}

void MainWindow::updateVideoHostMask()
{
    if (!videoHost_) {
        return;
    }
    if (immersiveMode_ || videoHost_->width() <= 0 || videoHost_->height() <= 0) {
        videoHost_->clearMask();
        return;
    }

    const qreal radius = std::min(
        kVideoHostCornerRadius,
        static_cast<qreal>(std::min(videoHost_->width(), videoHost_->height())) / 2.0);
    QPainterPath path;
    path.addRoundedRect(
        QRectF(0.0, 0.0, videoHost_->width(), videoHost_->height()),
        radius,
        radius);
    const QRegion roundedMask(path.toFillPolygon().toPolygon(), Qt::WindingFill);
    videoHost_->setMask(roundedMask.intersected(QRegion(videoHost_->rect())));
}

bool MainWindow::isEventFromThisWindow(QObject *object) const
{
    const auto *widget = qobject_cast<const QWidget *>(object);
    return widget
        && (widget == this
            || isAncestorOf(widget)
            || widget == immersiveFullscreenWindow_
            || (immersiveFullscreenWindow_ && immersiveFullscreenWindow_->isAncestorOf(widget)));
}

void MainWindow::enterSystemFullscreenImmersive()
{
    if (!immersiveMode_ || immersiveFullscreenWindow_ || !playerPane_ || !rootLayout_) {
        return;
    }

    immersiveWindowMode_ = ImmersiveWindowMode::SystemFullScreen;
    immersiveFullscreenWindow_ = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    immersiveFullscreenWindow_->setObjectName(QStringLiteral("immersiveFullscreenWindow"));
    immersiveFullscreenWindow_->setAttribute(Qt::WA_DeleteOnClose, false);
    immersiveFullscreenWindow_->setStyleSheet(
        styleSheet() + QStringLiteral("\nQWidget#immersiveFullscreenWindow { background:#01040a; }"));
    auto *fullscreenLayout = new QVBoxLayout(immersiveFullscreenWindow_);
    fullscreenLayout->setContentsMargins(0, 0, 0, 0);
    fullscreenLayout->setSpacing(0);

    rootLayout_->removeWidget(playerPane_);
    playerPane_->setParent(immersiveFullscreenWindow_);
    fullscreenLayout->addWidget(playerPane_);
    playerPane_->show();
    enableMouseTrackingForTree(immersiveFullscreenWindow_);
    immersiveFullscreenWindow_->showFullScreen();
    immersiveFullscreenWindow_->raise();
    immersiveFullscreenWindow_->activateWindow();
    QTimer::singleShot(0, this, &MainWindow::positionImmersiveControls);
}

void MainWindow::leaveSystemFullscreenImmersive()
{
    if (!immersiveFullscreenWindow_ || !playerPane_ || !rootLayout_) {
        return;
    }

    QWidget *fullscreenWindow = immersiveFullscreenWindow_;
    immersiveFullscreenWindow_ = nullptr;
    fullscreenWindow->hide();
    if (fullscreenWindow->layout()) {
        fullscreenWindow->layout()->removeWidget(playerPane_);
    }
    playerPane_->setParent(centralWidget());
    rootLayout_->insertWidget(0, playerPane_, 1);
    playerPane_->show();
    delete fullscreenWindow;
}

void MainWindow::toggleMaximized()
{
    isMaximized() ? showNormal() : showMaximized();
}

void MainWindow::updateWindowControlGeometry()
{
    if (!windowControls_) {
        return;
    }
    windowControls_->move(width() - kWindowControlsWidth - kWindowControlsMargin, kWindowControlsMargin);
    windowControls_->raise();
}

void MainWindow::updateWindowControlVisibility()
{
    if (!windowControls_) {
        return;
    }
    if (immersiveWindowMode_ == ImmersiveWindowMode::SystemFullScreen || isFullScreen()) {
        windowControls_->hide();
        return;
    }
    const QWidget *focusWidget = QApplication::focusWidget();
    const bool controlsFocused = focusWidget && windowControls_->isAncestorOf(focusWidget);
    const QPoint localCursor = mapFromGlobal(QCursor::pos());
    const QRect revealZone = windowControls_->geometry().adjusted(-kWindowControlsRevealPadding, -kWindowControlsRevealPadding, kWindowControlsRevealPadding, kWindowControlsRevealPadding);
    windowControls_->setVisible(isActiveWindow() && (controlsFocused || revealZone.contains(localCursor)));
}

void MainWindow::updateMaximizeButton()
{
    if (maximizeButton_) {
        maximizeButton_->setText(isMaximized() ? QStringLiteral("❐") : QStringLiteral("□"));
    }
}

Qt::Edges MainWindow::resizeEdgesAt(const QPoint &position) const
{
    if (isMaximized() || isFullScreen()) {
        return {};
    }

    Qt::Edges edges;
    if (position.x() >= 0 && position.x() < kResizeBorderWidth) {
        edges |= Qt::LeftEdge;
    } else if (position.x() < width() && position.x() >= width() - kResizeBorderWidth) {
        edges |= Qt::RightEdge;
    }
    if (position.y() >= 0 && position.y() < kTopResizeBorderWidth) {
        edges |= Qt::TopEdge;
    } else if (position.y() < height() && position.y() >= height() - kResizeBorderWidth) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

bool MainWindow::isMoveZoneAt(const QPoint &position) const
{
    return !isFullScreen()
        && position.y() >= kTopResizeBorderWidth
        && position.y() <= kMoveZoneHeight
        && !isInteractiveAt(position);
}

bool MainWindow::isInteractiveAt(const QPoint &position) const
{
    QWidget *widget = childAt(position);
    while (widget && widget != this) {
        if (widget == windowControls_
            || qobject_cast<QAbstractButton *>(widget)
            || qobject_cast<QComboBox *>(widget)
            || qobject_cast<QSlider *>(widget)
            || qobject_cast<QAbstractItemView *>(widget)) {
            return true;
        }
        widget = widget->parentWidget();
    }
    return false;
}

void MainWindow::restoreSettings()
{
    overlayToggle_->setChecked(settings_.value("subtitle-overlay", true).toBool());
    timingLogToggle_->setChecked(settings_.value("timing-log", false).toBool());
    lastState_.volume = settings_.value("volume", 100).toDouble();
    lastState_.muted = settings_.value("muted", false).toBool();
    updateVolumeUi(lastState_.volume, lastState_.muted);
    addComboItemWithTooltip(pictureSubtitleSelect_, QStringLiteral("无媒体"), kPictureSubtitleNone);
    pictureSubtitleSelect_->setEnabled(false);
    updatePictureSubtitleSelectVisibility();
    addComboItemWithTooltip(embeddedSelect_, QStringLiteral("无内嵌字幕"));
    embeddedSelect_->setEnabled(false);
}

void MainWindow::saveSettings()
{
    settings_.setValue("subtitle-overlay", overlayToggle_->isChecked());
    settings_.setValue("timing-log", timingLogToggle_->isChecked());
    settings_.setValue("volume", volumeSlider_->value());
    settings_.setValue("muted", lastState_.muted);
}

bool MainWindow::isVideoPath(const QString &path)
{
    return QRegularExpression(R"(\.(mp4|mkv|avi|mov|wmv|flv|webm|m4v|ts|m2ts|mpg|mpeg|3gp|ogv)$)", QRegularExpression::CaseInsensitiveOption).match(path).hasMatch();
}

bool MainWindow::isSubtitlePath(const QString &path)
{
    return QRegularExpression(R"(\.(srt|ass|ssa|vtt|lrc)$)", QRegularExpression::CaseInsensitiveOption).match(path).hasMatch();
}

QString MainWindow::subtitleTrackLabel(const SubtitleTrack &track)
{
    QStringList labelParts = {QString("#%1").arg(track.index), track.language.isEmpty() ? "und" : track.language};
    if (!track.title.isEmpty()) labelParts << track.title;
    if (!track.codec.isEmpty()) labelParts << track.codec;
    return labelParts.join(QStringLiteral(" · "));
}

QString MainWindow::basename(const QString &path)
{
    return QFileInfo(path).fileName();
}

QString MainWindow::formatTime(double seconds)
{
    if (!std::isfinite(seconds)) {
        return "00:00";
    }
    const int total = std::max(0, static_cast<int>(seconds));
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int rest = total % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(rest, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0')).arg(rest, 2, 10, QLatin1Char('0'));
}

QString MainWindow::formatClock(double seconds)
{
    if (!std::isfinite(seconds)) {
        return "00:00:00";
    }
    const int total = std::max(0, static_cast<int>(seconds));
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        handleDroppedPath(urls.first().toLocalFile());
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (immersiveMode_ && event->modifiers() == Qt::NoModifier && event->key() == Qt::Key_Escape) {
        exitImmersivePlayback();
        event->accept();
        return;
    }
    if (handlePlaybackKey(event)) {
        return;
    }
    if (event->modifiers() != Qt::NoModifier) {
        QMainWindow::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Space: handleSpacePlaybackAction(); break;
    case Qt::Key_K: togglePlayPause(true); break;
    default: QMainWindow::keyPressEvent(event); return;
    }
    event->accept();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const Qt::Edges edges = resizeEdgesAt(event->position().toPoint());
        if (edges != Qt::Edges{} && windowHandle() && windowHandle()->startSystemResize(edges)) {
            event->accept();
            return;
        }
        if (isMoveZoneAt(event->position().toPoint()) && windowHandle() && windowHandle()->startSystemMove()) {
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isMoveZoneAt(event->position().toPoint())) {
        toggleMaximized();
        event->accept();
        return;
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

bool MainWindow::handlePlaybackKey(QKeyEvent *event)
{
    if (event->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier)) {
        return false;
    }
    switch (event->key()) {
    case Qt::Key_Left: seekToAdjacentSubtitle(SubtitleNavigationDirection::Previous); break;
    case Qt::Key_Right: seekToAdjacentSubtitle(SubtitleNavigationDirection::Next); break;
    case Qt::Key_Up: applyVolume(qRound(lastState_.volume) + 5, true); break;
    case Qt::Key_Down: applyVolume(qRound(lastState_.volume) - 5, true); break;
    default: return false;
    }
    event->accept();
    return true;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateWindowControlGeometry();
    positionImmersiveControls();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateMaximizeButton();
        updateWindowControlGeometry();
        positionImmersiveControls();
        if (immersiveMode_
            && immersiveWindowMode_ == ImmersiveWindowMode::InWindow
            && isMaximized()) {
            immersiveWindowMode_ = ImmersiveWindowMode::SystemFullScreen;
            QTimer::singleShot(0, this, [this]() {
                if (immersiveMode_
                    && immersiveWindowMode_ == ImmersiveWindowMode::SystemFullScreen
                    && !immersiveFullscreenWindow_) {
                    enterSystemFullscreenImmersive();
                    showImmersiveControls();
                }
            });
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const auto isSubtitleListObject = [this](QObject *object) {
        return subtitleList_
            && (object == subtitleList_
                || object == subtitleListViewport_
                || object == subtitleListScrollBar_);
    };

    if (watched == videoHost_ && event->type() == QEvent::Resize) {
        updateVideoHostMask();
    }

    if (watched == immersiveFullscreenWindow_ && event->type() == QEvent::Close) {
        event->ignore();
        QTimer::singleShot(0, this, [this]() {
            if (immersiveMode_ && immersiveFullscreenWindow_) {
                exitImmersivePlayback();
            }
        });
        return true;
    }

    if (immersiveMode_ && isEventFromThisWindow(watched)) {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::HoverMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::Wheel:
        case QEvent::KeyPress:
            showImmersiveControls();
            break;
        default:
            break;
        }
    }

    const QWidget *activeWindow = QApplication::activeWindow();
    const bool playbackWindowActive = activeWindow == this || activeWindow == immersiveFullscreenWindow_;
    if (event->type() == QEvent::KeyPress && playbackWindowActive) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (immersiveMode_ && keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_Escape) {
            exitImmersivePlayback();
            keyEvent->accept();
            return true;
        }
        const bool subtitleListHasFocus = subtitleList_
            && (subtitleList_->hasFocus() || (subtitleListViewport_ && subtitleListViewport_->hasFocus()));
        const bool subtitleListNavigationKey = keyEvent->key() == Qt::Key_Up
            || keyEvent->key() == Qt::Key_Down
            || keyEvent->key() == Qt::Key_PageUp
            || keyEvent->key() == Qt::Key_PageDown
            || keyEvent->key() == Qt::Key_Home
            || keyEvent->key() == Qt::Key_End;
        if (subtitleListHasFocus && isSubtitleListObject(watched) && subtitleListNavigationKey) {
            suppressSubtitleAutoScrollForManualNavigation();
            return false;
        }
        if (keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_Space) {
            handleSpacePlaybackAction();
            keyEvent->accept();
            return true;
        }
        if (keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_K) {
            togglePlayPause(true);
            keyEvent->accept();
            return true;
        }
        if (handlePlaybackKey(keyEvent)) {
            return true;
        }
    }
    if (isSubtitleListObject(watched)) {
        if (watched == subtitleListViewport_ && event->type() == QEvent::Resize) {
            refreshSubtitleListLayoutForWidthChange();
        } else if (event->type() == QEvent::Wheel) {
            suppressSubtitleAutoScrollForManualNavigation();
        } else if (watched == subtitleListScrollBar_) {
            if (event->type() == QEvent::MouseButtonPress
                || (event->type() == QEvent::MouseMove && static_cast<QMouseEvent *>(event)->buttons().testFlag(Qt::LeftButton))) {
                suppressSubtitleAutoScrollForManualNavigation();
            }
        }
    }
    if (event->type() == QEvent::ToolTip) {
        QComboBox *tooltipCombo = nullptr;
        if (watched == pictureSubtitleSelectPopupViewport_) {
            tooltipCombo = pictureSubtitleSelect_;
        } else if (watched == embeddedSelectPopupViewport_) {
            tooltipCombo = embeddedSelect_;
        }
        if (!tooltipCombo) {
            return QMainWindow::eventFilter(watched, event);
        }
        QAbstractItemView *tooltipView = tooltipCombo->view();
        if (!tooltipView) {
            return QMainWindow::eventFilter(watched, event);
        }
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        const QModelIndex index = tooltipView->indexAt(helpEvent->pos());
        if (index.isValid()) {
            const QString tooltip = index.data(Qt::ToolTipRole).toString();
            if (!tooltip.isEmpty()) {
                QToolTip::showText(helpEvent->globalPos() + QPoint(12, 18), tooltip, tooltipView);
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    if (watched == videoHost_ && event->type() == QEvent::MouseButtonPress && mediaLoaded_) {
        togglePlayPause(true);
        return true;
    }
    if (watched == progressSlider_ && event->type() == QEvent::MouseButtonPress && mediaLoaded_ && lastState_.duration > 0.0) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const double ratio = std::clamp(mouse->position().x() / progressSlider_->width(), 0.0, 1.0);
        suppressAutoScrollUntil_ = QDateTime::currentMSecsSinceEpoch() + 800;
        seekTo(ratio * lastState_.duration, true);
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType)
#ifdef Q_OS_WIN
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCHITTEST) {
        if (isFullScreen()) {
            *result = HTCLIENT;
            return true;
        }
        const QPoint position = mapFromGlobal(QPoint(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)));
        if (isInteractiveAt(position)) {
            *result = HTCLIENT;
            return true;
        }

        const Qt::Edges edges = resizeEdgesAt(position);
        if (edges == (Qt::TopEdge | Qt::LeftEdge)) {
            *result = HTTOPLEFT;
        } else if (edges == (Qt::TopEdge | Qt::RightEdge)) {
            *result = HTTOPRIGHT;
        } else if (edges == (Qt::BottomEdge | Qt::LeftEdge)) {
            *result = HTBOTTOMLEFT;
        } else if (edges == (Qt::BottomEdge | Qt::RightEdge)) {
            *result = HTBOTTOMRIGHT;
        } else if (edges == Qt::LeftEdge) {
            *result = HTLEFT;
        } else if (edges == Qt::RightEdge) {
            *result = HTRIGHT;
        } else if (edges == Qt::TopEdge) {
            *result = HTTOP;
        } else if (edges == Qt::BottomEdge) {
            *result = HTBOTTOM;
        } else if (isMoveZoneAt(position)) {
            *result = HTCAPTION;
        } else {
            *result = HTCLIENT;
        }
        return true;
    }
    if (msg->message == WM_NCLBUTTONDBLCLK && msg->wParam == HTCAPTION) {
        toggleMaximized();
        *result = 0;
        return true;
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}
