#include "ui/FrameInfoPanel.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace h264tool {

FrameInfoPanel::FrameInfoPanel(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto* box = new QGroupBox(QStringLiteral("当前帧信息"), this);
    auto* grid = new QGridLayout(box);
    // 使用默认行距，略收边距即可
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(-1); // 交给样式默认

    streamOrder_ = makeValueLabel();
    displayOrder_ = makeValueLabel();
    gap_ = makeValueLabel();
    size_ = makeValueLabel();
    time_ = makeValueLabel();
    offset_ = makeValueLabel();
    type_ = makeValueLabel();
    qp_ = makeValueLabel();
    poc_ = makeValueLabel();
    refs_ = makeValueLabel();

    // 3 列 × 若干行：每项 = 标签 + 值
    auto addItem = [&](int row, int col, const QString& title, QLabel* value) {
        const int base = col * 2;
        auto* name = new QLabel(title);
        name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(name, row, base);
        grid->addWidget(value, row, base + 1);
        grid->setColumnStretch(base + 1, 1);
    };

    // 列0 / 列1 / 列2
    addItem(0, 0, QStringLiteral("Stream order:"), streamOrder_);
    addItem(0, 1, QStringLiteral("Display order:"), displayOrder_);
    addItem(0, 2, QStringLiteral("Gap:"), gap_);

    addItem(1, 0, QStringLiteral("Size:"), size_);
    addItem(1, 1, QStringLiteral("Time:"), time_);
    addItem(1, 2, QStringLiteral("Offset:"), offset_);

    addItem(2, 0, QStringLiteral("Type:"), type_);
    addItem(2, 1, QStringLiteral("QP:"), qp_);
    addItem(2, 2, QStringLiteral("POC:"), poc_);

    auto* refsName = new QLabel(QStringLiteral("References:"));
    refsName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(refsName, 3, 0);
    grid->addWidget(refs_, 3, 1, 1, 5);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(box);
}

QLabel* FrameInfoPanel::makeValueLabel() {
    auto* l = new QLabel(QStringLiteral("-"));
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

void FrameInfoPanel::setKV(QLabel* label, const QString& text) {
    label->setText(text);
}

void FrameInfoPanel::clear() {
    FrameInfo empty;
    setFrame(empty, 0.0);
}

void FrameInfoPanel::setFrame(const FrameInfo& frame, double durationSec) {
    setKV(streamOrder_, QString::number(frame.streamOrder));
    setKV(displayOrder_, QString::number(frame.displayOrder));
    setKV(gap_, QString::number(frame.gap));
    setKV(size_, QStringLiteral("%1 bytes").arg(frame.sizeBytes));

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
    setKV(time_, QStringLiteral("%1 / %2").arg(fmtTs(frame.timeSec), fmtTs(durationSec)));
    setKV(offset_, QStringLiteral("0x%1").arg(frame.fileOffset, 16, 16, QChar('0')));
    setKV(type_, frame.typeLabel());
    setKV(qp_, frame.qp >= 0 ? QString::number(frame.qp) : QStringLiteral("-"));
    setKV(poc_, QString::number(frame.poc));

    QStringList r0, r1;
    for (int v : frame.refList0) r0 << QString::number(v);
    for (int v : frame.refList1) r1 << QString::number(v);
    setKV(refs_, QStringLiteral("L0:[%1]  L1:[%2]")
                     .arg(r0.join(','), r1.join(',')));
}

} // namespace h264tool
