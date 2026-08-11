#include "ui/StatsPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace h264tool {

StatsPanel::StatsPanel(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto* box = new QGroupBox(QStringLiteral("全局统计"), this);
    auto* form = new QFormLayout(box);
    form->setContentsMargins(8, 8, 8, 8);
    form->setHorizontalSpacing(10);
    // 默认垂直间距，不再强制压成 1px

    auto make = [] {
        auto* l = new QLabel(QStringLiteral("-"));
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    };
    total_ = make();
    ipb_ = make();
    bitrate_ = make();
    resolution_ = make();
    profile_ = make();
    duration_ = make();
    form->addRow(QStringLiteral("总帧数:"), total_);
    form->addRow(QStringLiteral("I / P / B:"), ipb_);
    form->addRow(QStringLiteral("平均码率:"), bitrate_);
    form->addRow(QStringLiteral("分辨率:"), resolution_);
    form->addRow(QStringLiteral("Profile/Level:"), profile_);
    form->addRow(QStringLiteral("时长:"), duration_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(box);
}

void StatsPanel::clear() {
    setStats(StreamStats{});
}

void StatsPanel::setStats(const StreamStats& stats) {
    total_->setText(QString::number(stats.totalFrames));
    ipb_->setText(QStringLiteral("%1 / %2 / %3")
                      .arg(stats.iFrames)
                      .arg(stats.pFrames)
                      .arg(stats.bFrames));
    bitrate_->setText(QStringLiteral("%1 kbps").arg(stats.avgBitrateKbps, 0, 'f', 1));
    resolution_->setText(stats.width > 0
                             ? QStringLiteral("%1 x %2").arg(stats.width).arg(stats.height)
                             : QStringLiteral("-"));
    profile_->setText(stats.profile.isEmpty()
                          ? QStringLiteral("-")
                          : QStringLiteral("%1 @ L%2").arg(stats.profile, stats.level));
    duration_->setText(QStringLiteral("%1 s (≈%2 fps)")
                           .arg(stats.durationSec, 0, 'f', 3)
                           .arg(stats.fps, 0, 'f', 2));
}

} // namespace h264tool
