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

    // 初始化，如果返回 false 说明已有实例在运行
    if (!core.init()) {
        // 已有实例，向用户提示（可选）
        // 静默退出，不弹出窗口（避免打扰）
        return 0;
    }

    // 退出
    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        core.shutdown();
    });

    return app.exec();
}
