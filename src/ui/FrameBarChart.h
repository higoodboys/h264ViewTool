#pragma once

#include "core/FrameInfo.h"

#include <QWidget>
#include <vector>

namespace h264tool {

/**
 * @brief 帧大小 / 类型彩色条带图（StreamEye 风格）。
 *
 * 仅绘制可见区间，点击发射 frameClicked。
 */
class FrameBarChart : public QWidget {
    Q_OBJECT
public:
    explicit FrameBarChart(QWidget* parent = nullptr);

    void setFrames(const std::vector<FrameInfo>& frames);
    void setCurrentFrame(int index);
    int currentFrame() const { return current_; }

signals:
    void frameClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int indexAtX(int x) const;
    void ensureVisible(int index);

    std::vector<FrameInfo> frames_;
    int current_ = 0;
    int firstVisible_ = 0;
    double pixelsPerFrame_ = 6.0;
    uint64_t maxSize_ = 1;
};

} // namespace h264tool
