#include <QApplication>
#include <QTimer>
#include "app_core.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Fcitx5WordCount");
    app.setOrganizationName("Fcitx5WordCount");
    app.setQuitOnLastWindowClosed(false);

    AppCore& core = AppCore::instance();
    
    if (!core.init()) {
        return 0;
    }

    // 使用 aboutToQuit 而不是 applicationStateChanged
    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        qDebug() << "aboutToQuit 信号触发";
        core.shutdown();
    });

    // 优雅退出处理
    QObject::connect(&app, &QApplication::lastWindowClosed, [&]() {
        // 如果所有窗口都关闭了（托盘还在），不退出
        // 但如果有窗口被关闭，不做特殊处理
    });

    int result = app.exec();
    
    qDebug() << "应用程序退出，返回值:" << result;
    return result;
}