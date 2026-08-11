#pragma once

#include <QObject>

namespace h264tool {

/**
 * @brief 控制窗口与预览窗口的帧选中 / 播放状态同步中枢。
 */
class SyncController : public QObject {
    Q_OBJECT
public:
    explicit SyncController(QObject* parent = nullptr);

    int currentFrame() const { return current_; }
    int frameCount() const { return frameCount_; }
    bool isPlaying() const { return playing_; }

public slots:
    void setFrameCount(int count);
    void setCurrentFrame(int index);
    void nextFrame();
    void prevFrame();
    void firstFrame();
    void lastFrame();
    void setPlaying(bool on);
    void togglePlay();

signals:
    void currentFrameChanged(int index);
    void playingChanged(bool playing);
    void frameCountChanged(int count);

private:
    int current_ = 0;
    int frameCount_ = 0;
    bool playing_ = false;
};

} // namespace h264tool
