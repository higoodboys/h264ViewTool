#pragma once

#include <QWidget>

class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;

namespace h264tool {

/** 底部导航：逐帧、播放、跳转 */
class NavigationBar : public QWidget {
    Q_OBJECT
public:
    explicit NavigationBar(QWidget* parent = nullptr);
    void setPlaying(bool playing);
    void setFrameIndex(int index, int total);

signals:
    void firstClicked();
    void prevClicked();
    void playClicked();
    void nextClicked();
    void lastClicked();
    void jumpRequested(int index);
    void findClicked();

private:
    QPushButton* firstBtn_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* playBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QPushButton* lastBtn_ = nullptr;
    QLineEdit* jumpEdit_ = nullptr;
    QLabel* posLabel_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
};

} // namespace h264tool
