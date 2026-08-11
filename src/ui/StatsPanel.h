#pragma once

#include "core/FrameInfo.h"

#include <QWidget>

class QLabel;

namespace h264tool {

/** 全局统计面板 */
class StatsPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatsPanel(QWidget* parent = nullptr);
    void setStats(const StreamStats& stats);
    void clear();

private:
    QLabel* total_ = nullptr;
    QLabel* ipb_ = nullptr;
    QLabel* bitrate_ = nullptr;
    QLabel* resolution_ = nullptr;
    QLabel* profile_ = nullptr;
    QLabel* duration_ = nullptr;
};

} // namespace h264tool
