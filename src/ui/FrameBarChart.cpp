#include "ui/FrameBarChart.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <chrono>

namespace h264tool {

FrameBarChart::FrameBarChart(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(180);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void FrameBarChart::setFrames(const std::vector<FrameInfo>& frames) {
    frames_ = frames;
    maxSize_ = 1;
    for (const auto& f : frames_)
        maxSize_ = std::max(maxSize_, f.sizeBytes);
    firstVisible_ = 0;
    current_ = 0;
    update();
}

void FrameBarChart::setCurrentFrame(int index) {
    if (frames_.empty()) return;
    current_ = std::clamp(index, 0, static_cast<int>(frames_.size()) - 1);
    ensureVisible(current_);
    update();
}

void FrameBarChart::ensureVisible(int index) {
    const int visibleCount = std::max(1, static_cast<int>(width() / pixelsPerFrame_));
    if (index < firstVisible_)
        firstVisible_ = index;
    else if (index >= firstVisible_ + visibleCount)
        firstVisible_ = index - visibleCount + 1;
    firstVisible_ = std::max(0, firstVisible_);
}

int FrameBarChart::indexAtX(int x) const {
    if (frames_.empty() || pixelsPerFrame_ <= 0) return -1;
    const int idx = firstVisible_ + static_cast<int>(x / pixelsPerFrame_);
    if (idx < 0 || idx >= static_cast<int>(frames_.size())) return -1;
    return idx;
}

void FrameBarChart::paintEvent(QPaintEvent*) {
    // 目标：可见区重绘 < 50ms
    const auto t0 = std::chrono::steady_clock::now();

    QPainter p(this);
    p.fillRect(rect(), QColor(245, 245, 248));

    const int leftAxis = 56;
    const int bottomAxis = 28;
    const int topPad = 8;
    const QRect plot(leftAxis, topPad, width() - leftAxis - 8, height() - bottomAxis - topPad);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // 坐标轴
    p.setPen(QColor(120, 120, 120));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
    p.drawLine(plot.bottomLeft(), plot.topLeft());

    // Y 刻度（帧大小）
    p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
    for (int i = 0; i <= 4; ++i) {
        const int y = plot.bottom() - plot.height() * i / 4;
        const uint64_t val = maxSize_ * static_cast<uint64_t>(i) / 4;
        p.drawText(QRect(0, y - 8, leftAxis - 4, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(val));
        p.setPen(QColor(220, 220, 225));
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(QColor(120, 120, 120));
    }

    if (frames_.empty()) {
        p.drawText(plot, Qt::AlignCenter, QStringLiteral("打开文件以显示帧分布"));
        return;
    }

    const int visibleCount = std::max(1, static_cast<int>(plot.width() / pixelsPerFrame_));
    const int last = std::min(static_cast<int>(frames_.size()), firstVisible_ + visibleCount + 1);

    for (int i = firstVisible_; i < last; ++i) {
        const auto& f = frames_[static_cast<size_t>(i)];
        const double x = plot.left() + (i - firstVisible_) * pixelsPerFrame_;
        // 条宽比步进少 1px，帧与帧之间留 1 像素空隙，避免糊成一片
        const double barW = std::max(1.0, pixelsPerFrame_ - 1.0);
        const double hRatio = maxSize_ > 0 ? static_cast<double>(f.sizeBytes) / static_cast<double>(maxSize_) : 0;
        const int h = std::max(1, static_cast<int>(plot.height() * hRatio));
        const QRectF bar(x, plot.bottom() - h, barW, h);
        p.fillRect(bar, frameTypeColor(f.type));
        if (i == current_) {
            p.setPen(QPen(Qt::black, 2));
            p.drawRect(bar);
            p.setPen(QColor(120, 120, 120));
        }
    }

    // X 刻度
    const int step = std::max(1, visibleCount / 8);
    for (int i = firstVisible_; i < last; i += step) {
        const int x = plot.left() + static_cast<int>((i - firstVisible_) * pixelsPerFrame_);
        p.drawText(x, height() - 8, QString::number(i));
    }

    // 图例
    const int legendY = 4;
    auto legend = [&](int x, const QColor& c, const QString& t) {
        p.fillRect(x, legendY, 10, 10, c);
        p.drawText(x + 14, legendY + 10, t);
    };
    legend(leftAxis + 8, frameTypeColor(FrameType::I), QStringLiteral("I"));
    legend(leftAxis + 48, frameTypeColor(FrameType::P), QStringLiteral("P"));
    legend(leftAxis + 88, frameTypeColor(FrameType::B), QStringLiteral("B"));

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    Q_UNUSED(ms);
}

void FrameBarChart::mousePressEvent(QMouseEvent* event) {
    const int leftAxis = 56;
    const int idx = indexAtX(event->pos().x() - leftAxis);
    if (idx >= 0) {
        current_ = idx;
        update();
        emit frameClicked(idx);
    }
}

void FrameBarChart::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? 1.2 : 1.0 / 1.2;
        pixelsPerFrame_ = std::clamp(pixelsPerFrame_ * factor, 2.0, 40.0);
        ensureVisible(current_);
        update();
    } else {
        const int delta = event->angleDelta().y() > 0 ? -8 : 8;
        firstVisible_ = std::clamp(firstVisible_ + delta, 0,
                                   std::max(0, static_cast<int>(frames_.size()) - 1));
        update();
    }
    event->accept();
}

void FrameBarChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ensureVisible(current_);
}

} // namespace h264tool
