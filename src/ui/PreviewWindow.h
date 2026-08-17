#pragma once

#include <QMainWindow>
#include <QImage>
#include <memory>

class QLabel;
class QScrollArea;

namespace h264tool {

class SyncController;
class IDecoder;

/**
 * @brief 独立预览窗口：显示当前帧解码图像，支持拖拽打开。
 */
class PreviewWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit PreviewWindow(SyncController* sync, QWidget* parent = nullptr);
    ~PreviewWindow() override;

    void setDecoder(std::unique_ptr<IDecoder> decoder);
    void openFile(const QString& path);

signals:
    void fileDropped(const QString& path);

public slots:
    void onCurrentFrameChanged(int index);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void showImage(const QImage& img);
    void updateScaledPixmap();

    SyncController* sync_ = nullptr;
    std::unique_ptr<IDecoder> decoder_;
    QScrollArea* scrollArea_ = nullptr;
    QLabel* imageLabel_ = nullptr;
    QImage currentImage_;
    QString path_;
};

} // namespace h264tool
