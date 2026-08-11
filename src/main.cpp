#include "ui/MainControlWindow.h"

#include <QApplication>
#include <QIcon>

/**
 * H264Tool 入口。
 *
 * 启动后创建独立的控制窗口与预览窗口（由 MainControlWindow 管理）。
 */
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("H264Tool"));
    QApplication::setOrganizationName(QStringLiteral("H264Tool"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/h264tool.png")));

    h264tool::MainControlWindow win;
    win.show();

    if (argc >= 2) {
        win.openFile(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
