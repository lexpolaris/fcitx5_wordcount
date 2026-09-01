#include <QApplication>
#include "app_core.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Fcitx5WordCount");
    app.setOrganizationName("Fcitx5WordCount");
    app.setQuitOnLastWindowClosed(false);

    // 初始化核心
    AppCore& core = AppCore::instance();
    core.init();

    // 退出
    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        core.shutdown();
    });

    return app.exec();
}
