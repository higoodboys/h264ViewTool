#include "ui/NavigationBar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>

namespace h264tool {
namespace {

/** 绘制清晰的矢量风格导航图标 */
QIcon makeNavIcon(const QString& kind, int size = 22) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor fg(40, 40, 45);
    p.setPen(Qt::NoPen);
    p.setBrush(fg);

    const qreal s = size;
    const qreal m = s * 0.18;

    if (kind == QLatin1String("first")) {
        p.drawRect(QRectF(m, m, s * 0.12, s - 2 * m));
        QPolygonF tri;
        tri << QPointF(s - m, m) << QPointF(s * 0.38, s * 0.5) << QPointF(s - m, s - m);
        p.drawPolygon(tri);
    } else if (kind == QLatin1String("prev")) {
        QPolygonF tri;
        tri << QPointF(s - m, m) << QPointF(m, s * 0.5) << QPointF(s - m, s - m);
        p.drawPolygon(tri);
    } else if (kind == QLatin1String("next")) {
        QPolygonF tri;
        tri << QPointF(m, m) << QPointF(s - m, s * 0.5) << QPointF(m, s - m);
        p.drawPolygon(tri);
    } else if (kind == QLatin1String("last")) {
        QPolygonF tri;
        tri << QPointF(m, m) << QPointF(s * 0.62, s * 0.5) << QPointF(m, s - m);
        p.drawPolygon(tri);
        p.drawRect(QRectF(s - m - s * 0.12, m, s * 0.12, s - 2 * m));
    } else if (kind == QLatin1String("play")) {
        QPolygonF tri;
        tri << QPointF(m + s * 0.08, m) << QPointF(s - m, s * 0.5) << QPointF(m + s * 0.08, s - m);
        p.drawPolygon(tri);
    } else if (kind == QLatin1String("pause")) {
        const qreal w = s * 0.18;
        const qreal gap = s * 0.12;
        const qreal x0 = s * 0.5 - gap * 0.5 - w;
        p.drawRect(QRectF(x0, m, w, s - 2 * m));
        p.drawRect(QRectF(x0 + w + gap, m, w, s - 2 * m));
    }
    return QIcon(pm);
}

QPushButton* makeIconButton(const QIcon& icon, const QString& tip, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setIcon(icon);
    b->setIconSize(QSize(22, 22));
    b->setFixedSize(40, 36);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #f3f3f5;"
        "  border: 1px solid #b0b0b8;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "}"
        "QPushButton:hover { background: #e4e8f0; border-color: #5b7cba; }"
        "QPushButton:pressed { background: #d0d7e6; }"
    ));
    return b;
}

} // namespace

NavigationBar::NavigationBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    firstBtn_ = makeIconButton(makeNavIcon(QStringLiteral("first")), QStringLiteral("首帧"), this);
    prevBtn_ = makeIconButton(makeNavIcon(QStringLiteral("prev")), QStringLiteral("上一帧"), this);
    playBtn_ = makeIconButton(makeNavIcon(QStringLiteral("play")), QStringLiteral("播放/暂停"), this);
    playBtn_->setFixedSize(52, 36);
    nextBtn_ = makeIconButton(makeNavIcon(QStringLiteral("next")), QStringLiteral("下一帧"), this);
    lastBtn_ = makeIconButton(makeNavIcon(QStringLiteral("last")), QStringLiteral("末帧"), this);

    layout->addWidget(firstBtn_);
    layout->addWidget(prevBtn_);
    layout->addWidget(playBtn_);
    layout->addWidget(nextBtn_);
    layout->addWidget(lastBtn_);

    layout->addSpacing(14);
    layout->addWidget(new QLabel(QStringLiteral("跳转:")));
    jumpEdit_ = new QLineEdit;
    jumpEdit_->setFixedWidth(72);
    jumpEdit_->setPlaceholderText(QStringLiteral("帧号"));
    layout->addWidget(jumpEdit_);

    auto* jumpBtn = new QPushButton(QStringLiteral("Go"));
    jumpBtn->setFixedHeight(36);
    layout->addWidget(jumpBtn);

    auto* findBtn = new QPushButton(QStringLiteral("Find..."));
    findBtn->setFixedHeight(36);
    layout->addWidget(findBtn);

    layout->addSpacing(12);
    layout->addWidget(new QLabel(QStringLiteral("Mode Display")));
    modeCombo_ = new QComboBox;
    modeCombo_->addItems({QStringLiteral("Frame size"), QStringLiteral("QP"), QStringLiteral("Type only")});
    modeCombo_->setFixedHeight(36);
    layout->addWidget(modeCombo_);

    layout->addStretch();
    posLabel_ = new QLabel(QStringLiteral("0 / 0"));
    posLabel_->setStyleSheet(QStringLiteral("font-weight:600;"));
    layout->addWidget(posLabel_);

    connect(firstBtn_, &QPushButton::clicked, this, &NavigationBar::firstClicked);
    connect(prevBtn_, &QPushButton::clicked, this, &NavigationBar::prevClicked);
    connect(playBtn_, &QPushButton::clicked, this, &NavigationBar::playClicked);
    connect(nextBtn_, &QPushButton::clicked, this, &NavigationBar::nextClicked);
    connect(lastBtn_, &QPushButton::clicked, this, &NavigationBar::lastClicked);
    connect(findBtn, &QPushButton::clicked, this, &NavigationBar::findClicked);
    connect(jumpBtn, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const int v = jumpEdit_->text().trimmed().toInt(&ok);
        if (ok) emit jumpRequested(v);
    });
    connect(jumpEdit_, &QLineEdit::returnPressed, jumpBtn, &QPushButton::click);
}

void NavigationBar::setPlaying(bool playing) {
    playBtn_->setIcon(makeNavIcon(playing ? QStringLiteral("pause") : QStringLiteral("play")));
    playBtn_->setToolTip(playing ? QStringLiteral("暂停") : QStringLiteral("播放"));
}

void NavigationBar::setFrameIndex(int index, int total) {
    posLabel_->setText(QStringLiteral("%1 / %2").arg(index).arg(total));
}

} // namespace h264tool
