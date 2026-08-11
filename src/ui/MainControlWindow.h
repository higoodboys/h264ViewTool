#pragma once

#include "core/FileLoader.h"

#include <QMainWindow>
#include <memory>
#include <vector>

class QTimer;
class QLabel;

namespace h264tool {

class SyncController;
class PreviewWindow;
class FrameBarChart;
class FrameInfoPanel;
class StatsPanel;
class NavigationBar;
class IDecoder;

/**
 * @brief 主控制/信息窗口：打开文件、条带图、帧信息、统计与导航。
 */
class MainControlWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainControlWindow(QWidget* parent = nullptr);
    ~MainControlWindow() override;

public slots:
    void openFile(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onOpenAction();
    void onFrameClicked(int index);
    void onCurrentFrameChanged(int index);
    void onPlayTick();
    void onFindFrame();

private:
    void buildUi();
    void buildMenus();
    void applyAnalysis(const FileLoader::LoadResult& result);
    void refreshFrameUi(int index);

    SyncController* sync_ = nullptr;
    PreviewWindow* preview_ = nullptr;

    FrameBarChart* chart_ = nullptr;
    FrameInfoPanel* infoPanel_ = nullptr;
    StatsPanel* statsPanel_ = nullptr;
    NavigationBar* nav_ = nullptr;
    QLabel* statusPath_ = nullptr;
    QTimer* playTimer_ = nullptr;

    FileLoader::LoadResult loaded_;
    double durationSec_ = 0.0;
};

} // namespace h264tool
