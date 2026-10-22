#include "ui/MainControlWindow.h"

#include "decode/DecoderFactory.h"
#include "ui/FrameBarChart.h"
#include "ui/FrameInfoPanel.h"
#include "ui/NavigationBar.h"
#include "ui/PreviewWindow.h"
#include "ui/StatsPanel.h"
#include "ui/SyncController.h"

#include <QAction>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace h264tool {

MainControlWindow::MainControlWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("H264Tool - ESEye"));
    setAcceptDrops(true);
    resize(1000, 520);

    sync_ = new SyncController(this);
    playTimer_ = new QTimer(this);
    playTimer_->setInterval(40); // ~25fps
    connect(playTimer_, &QTimer::timeout, this, &MainControlWindow::onPlayTick);

    buildMenus();
    buildUi();

    preview_ = new PreviewWindow(sync_);
    preview_->setDecoder(DecoderFactory::create());
    preview_->show();

    connect(preview_, &PreviewWindow::fileDropped, this, &MainControlWindow::openFile);
    connect(sync_, &SyncController::currentFrameChanged, this, &MainControlWindow::onCurrentFrameChanged);
    connect(sync_, &SyncController::playingChanged, this, [this](bool on) {
        nav_->setPlaying(on);
        if (on) playTimer_->start();
        else playTimer_->stop();
    });
}

MainControlWindow::~MainControlWindow() {
    if (preview_) {
        preview_->close();
        delete preview_;
        preview_ = nullptr;
    }
}

void MainControlWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    auto* openAct = fileMenu->addAction(QStringLiteral("打开(&O)..."));
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainControlWindow::onOpenAction);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出(&X)"), QKeySequence::Quit, this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("查看(&V)"));
    viewMenu->addAction(QStringLiteral("显示预览窗口"), this, [this] {
        if (preview_) {
            preview_->show();
            preview_->raise();
        }
    });

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于"), this, [this] {
        QMessageBox::about(this, QStringLiteral("关于 H264Tool"),
                           QStringLiteral(
                               "H264Tool — Windows H.264 码流分析与预览\n"
                               "交互参考 Elecard StreamEye Tools 2.0\n"
                               "解析自研；解码可选 FFmpeg（LGPL/GPL）。"));
    });

    auto* tb = addToolBar(QStringLiteral("主工具栏"));
    tb->addAction(openAct);
}

void MainControlWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 2);
    layout->setSpacing(4);

    chart_ = new FrameBarChart;
    chart_->setMinimumHeight(140);
    layout->addWidget(chart_, 1);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    infoPanel_ = new FrameInfoPanel;
    statsPanel_ = new StatsPanel;
    splitter->addWidget(infoPanel_);
    splitter->addWidget(statsPanel_);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    // 信息区按内容高度，不纵向拉伸占空
    splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    layout->addWidget(splitter, 0);

    nav_ = new NavigationBar;
    layout->addWidget(nav_, 0);

    setCentralWidget(central);
    statusPath_ = new QLabel(QStringLiteral("未打开文件"));
    statusBar()->addWidget(statusPath_, 1);

    connect(chart_, &FrameBarChart::frameClicked, this, &MainControlWindow::onFrameClicked);
    connect(nav_, &NavigationBar::firstClicked, sync_, &SyncController::firstFrame);
    connect(nav_, &NavigationBar::prevClicked, sync_, &SyncController::prevFrame);
    connect(nav_, &NavigationBar::nextClicked, sync_, &SyncController::nextFrame);
    connect(nav_, &NavigationBar::lastClicked, sync_, &SyncController::lastFrame);
    connect(nav_, &NavigationBar::playClicked, sync_, &SyncController::togglePlay);
    connect(nav_, &NavigationBar::jumpRequested, sync_, &SyncController::setCurrentFrame);
    connect(nav_, &NavigationBar::findClicked, this, &MainControlWindow::onFindFrame);
}

void MainControlWindow::onOpenAction() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开媒体/码流"),
        QString(),
        QStringLiteral("媒体与码流 (*.mp4 *.avi *.h264 *.264);;所有文件 (*.*)"));
    if (!path.isEmpty()) openFile(path);
}

void MainControlWindow::openFile(const QString& path) {
    sync_->setPlaying(false);
    const auto result = FileLoader::load(path);
    if (!result.ok()) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), result.error);
        return;
    }
    // 先换解码器；注入索引前断开预览切帧，避免无索引时从头重解
    preview_->setDecoder(DecoderFactory::create());
    preview_->openFile(path);
    disconnect(sync_, &SyncController::currentFrameChanged, preview_,
               &PreviewWindow::onCurrentFrameChanged);
    applyAnalysis(result);
    preview_->setStreamIndex(&loaded_.nals, loaded_.analysis.frames);
    connect(sync_, &SyncController::currentFrameChanged, preview_,
            &PreviewWindow::onCurrentFrameChanged);
    preview_->onCurrentFrameChanged(sync_->currentFrame());
    preview_->show();
    preview_->raise();
}

void MainControlWindow::applyAnalysis(const FileLoader::LoadResult& result) {
    loaded_ = result;
    durationSec_ = result.analysis.stats.durationSec;
    chart_->setFrames(result.analysis.frames);
    statsPanel_->setStats(result.analysis.stats);
    sync_->setFrameCount(static_cast<int>(result.analysis.frames.size()));
    sync_->setCurrentFrame(0);
    refreshFrameUi(0);

    const double fps = result.analysis.stats.fps > 0 ? result.analysis.stats.fps : 25.0;
    playTimer_->setInterval(std::max(1, static_cast<int>(1000.0 / fps)));

    setWindowTitle(QStringLiteral("H264Tool - [%1] (00:00:00.000)").arg(result.path));
    statusPath_->setText(result.path);
    if (!result.analysis.warning.isEmpty())
        statusBar()->showMessage(result.analysis.warning, 8000);
}

void MainControlWindow::onFrameClicked(int index) {
    sync_->setCurrentFrame(index);
}

void MainControlWindow::onCurrentFrameChanged(int index) {
    chart_->setCurrentFrame(index);
    refreshFrameUi(index);
}

void MainControlWindow::refreshFrameUi(int index) {
    nav_->setFrameIndex(index, sync_->frameCount());
    if (index >= 0 && index < static_cast<int>(loaded_.analysis.frames.size())) {
        const auto& f = loaded_.analysis.frames[static_cast<size_t>(index)];
        infoPanel_->setFrame(f, durationSec_);
        auto fmtTs = [](double sec) {
            const int ms = static_cast<int>(sec * 1000.0 + 0.5);
            const int h = ms / 3600000;
            const int m = (ms / 60000) % 60;
            const int s = (ms / 1000) % 60;
            const int milli = ms % 1000;
            return QStringLiteral("%1:%2:%3.%4")
                .arg(h, 2, 10, QChar('0'))
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0'))
                .arg(milli, 3, 10, QChar('0'));
        };
        setWindowTitle(QStringLiteral("H264Tool - [%1] (%2)")
                           .arg(loaded_.path, fmtTs(f.timeSec)));
    }
}

void MainControlWindow::onPlayTick() {
    if (sync_->currentFrame() >= sync_->frameCount() - 1) {
        sync_->setPlaying(false);
        return;
    }
    sync_->nextFrame();
}

void MainControlWindow::onFindFrame() {
    bool ok = false;
    const int v = QInputDialog::getInt(this, QStringLiteral("Find"),
                                       QStringLiteral("跳转到帧号:"),
                                       sync_->currentFrame(), 0,
                                       std::max(0, sync_->frameCount() - 1), 1, &ok);
    if (ok) sync_->setCurrentFrame(v);
}

void MainControlWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainControlWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty()) openFile(urls.front().toLocalFile());
}

void MainControlWindow::closeEvent(QCloseEvent* event) {
    if (preview_) preview_->close();
    QMainWindow::closeEvent(event);
}

} // namespace h264tool
