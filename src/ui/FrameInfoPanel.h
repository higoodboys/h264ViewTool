#pragma once

#include "core/FrameInfo.h"

#include <QWidget>

class QLabel;
class QFormLayout;

namespace h264tool {

/** 当前帧详情面板（对照 StreamEye 左下信息区） */
class FrameInfoPanel : public QWidget {
    Q_OBJECT
public:
    explicit FrameInfoPanel(QWidget* parent = nullptr);
    void setFrame(const FrameInfo& frame, double durationSec);
    void clear();

private:
    QLabel* makeValueLabel();
    void setKV(QLabel* label, const QString& text);

    QLabel* streamOrder_ = nullptr;
    QLabel* displayOrder_ = nullptr;
    QLabel* gap_ = nullptr;
    QLabel* size_ = nullptr;
    QLabel* time_ = nullptr;
    QLabel* offset_ = nullptr;
    QLabel* type_ = nullptr;
    QLabel* qp_ = nullptr;
    QLabel* poc_ = nullptr;
    QLabel* refs_ = nullptr;
};

} // namespace h264tool
