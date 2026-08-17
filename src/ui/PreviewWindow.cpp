#include "ui/PreviewWindow.h"

#include "decode/IDecoder.h"
#include "decode/NullDecoder.h"
#include "ui/SyncController.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStatusBar>
#include <memory>

namespace h264tool {

PreviewWindow::PreviewWindow(SyncController* sync, QWidget* parent)
    : QMainWindow(parent), sync_(sync) {
    setWindowTitle(QStringLiteral("H264Tool - Preview"));
    setAcceptDrops(true);
    resize(960, 540);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setAlignment(Qt::AlignCenter);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setStyleSheet(QStringLiteral("QScrollArea { background:#1e1e22; border: none; }"));
    imageLabel_ = new QLabel;
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel_->setMinimumSize(1, 1);
    imageLabel_->setStyleSheet(QStringLiteral("background:#1e1e22; color:#ddd;"));
    imageLabel_->setText(QStringLiteral("预览窗口\n拖入或通过控制窗口打开文件"));
    scrollArea_->setWidget(imageLabel_);
    setCentralWidget(scrollArea_);
    statusBar()->showMessage(QStringLiteral("就绪"));

    connect(sync_, &SyncController::currentFrameChanged, this, &PreviewWindow::onCurrentFrameChanged);
}

PreviewWindow::~PreviewWindow() = default;

void PreviewWindow::setDecoder(std::unique_ptr<IDecoder> decoder) {
    decoder_ = std::move(decoder);
}

void PreviewWindow::openFile(const QString& path) {
    path_ = path;
    if (!decoder_)
        decoder_ = std::make_unique<NullDecoder>();

    if (!decoder_->open(path)) {
        statusBar()->showMessage(decoder_->lastError());
        auto fallback = std::make_unique<NullDecoder>();
        fallback->setFrameCount(sync_->frameCount());
        fallback->open(path);
        decoder_ = std::move(fallback);
    } else if (auto* nullDec = dynamic_cast<NullDecoder*>(decoder_.get())) {
        nullDec->setFrameCount(sync_->frameCount());
    }

    setWindowTitle(QStringLiteral("H264Tool - Preview [%1]").arg(path));
    statusBar()->showMessage(QStringLiteral("解码后端: %1").arg(decoder_->backendName()));
    onCurrentFrameChanged(sync_->currentFrame());
}

void PreviewWindow::onCurrentFrameChanged(int index) {
    if (!decoder_) return;
    QImage img = decoder_->decodeFrame(index);
    if (img.isNull()) {
        // 仍显示占位
        QImage placeholder(640, 360, QImage::Format_RGB32);
        placeholder.fill(QColor(30, 30, 34));
        showImage(placeholder);
        statusBar()->showMessage(decoder_->lastError());
        return;
    }
    showImage(img);
    statusBar()->showMessage(QStringLiteral("帧 #%1  %2x%3")
                                 .arg(index)
                                 .arg(img.width())
                                 .arg(img.height()));
}

void PreviewWindow::showImage(const QImage& img) {
    currentImage_ = img;
    if (img.isNull()) {
        imageLabel_->setPixmap(QPixmap());
        imageLabel_->setText(QStringLiteral("无图像"));
        return;
    }
    imageLabel_->setText(QString());
    updateScaledPixmap();
}

void PreviewWindow::updateScaledPixmap() {
    if (currentImage_.isNull() || !scrollArea_ || !imageLabel_)
        return;

    const QSize logical = scrollArea_->viewport()->size();
    if (logical.width() < 1 || logical.height() < 1)
        return;

    const qreal dpr = imageLabel_->devicePixelRatioF();
    const QSize physical(qMax(1, qRound(logical.width() * dpr)),
                         qMax(1, qRound(logical.height() * dpr)));
    QPixmap scaled = QPixmap::fromImage(currentImage_).scaled(
        physical, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    imageLabel_->setPixmap(scaled);
}

void PreviewWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateScaledPixmap();
}

void PreviewWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void PreviewWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;
    emit fileDropped(urls.front().toLocalFile());
}

void PreviewWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left: sync_->prevFrame(); break;
    case Qt::Key_Right: sync_->nextFrame(); break;
    case Qt::Key_Home: sync_->firstFrame(); break;
    case Qt::Key_End: sync_->lastFrame(); break;
    case Qt::Key_Space: sync_->togglePlay(); break;
    default: QMainWindow::keyPressEvent(event); break;
    }
}

} // namespace h264tool
