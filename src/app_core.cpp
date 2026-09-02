// src/app_core.cpp
// SPDX-License-Identifier: LGPL-2.1-or-later
// 应用核心实现

#include "app_core.h"

#include "core/fcitx5_monitor.h"
#include "engine/statistics_engine.h"
#include "engine/database_storage.h"
#include "ui/detail_panel.h"
#include "ui/insight_panel.h"
#include "ui/tray_icon_manager.h"
#include "ui/settings_dialog.h"
#include "ui/theme_helper.h"
#include "ui/config_manager.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QDebug>


AppCore& AppCore::instance()
{
    static AppCore core;
    return core;
}

bool AppCore::acquireSingleInstance()
{
    // 尝试创建共享内存，如果已存在则说明已有实例在运行
    m_sharedMem.setKey(QString::fromUtf8(kSharedMemKey));
    
    if (m_sharedMem.attach()) {
        // 共享内存已存在，说明已有实例
        qWarning() << "已有程序实例在运行，退出";
        return false;
    }

    // 创建共享内存（至少1字节）
    if (!m_sharedMem.create(1)) {
        // 创建失败，可能权限问题，尝试附加已有
        if (m_sharedMem.attach()) {
            qWarning() << "已有程序实例在运行，退出";
            return false;
        }
        // 实在无法创建，继续运行但记录警告
        qWarning() << "无法创建共享内存，但继续运行（可能无法防止多实例）";
    }

    return true;
}

void AppCore::releaseSingleInstance()
{
    if (m_sharedMem.isAttached()) {
        m_sharedMem.detach();
    }
}

bool AppCore::init()
{
    // 单例检测（必须最先执行）
    if (!acquireSingleInstance()) {
        return false;
    }

    auto& config = ConfigManager::instance();

    // 1. 数据库
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + "/Fcitx5WordCount";
    QDir().mkpath(configDir);
    QString dbPath = configDir + "/stats.db";

    m_db = new wordcount::DatabaseStorage(dbPath, this);
    if (!m_db->init()) {
        qWarning() << "数据库初始化失败";
    }

    // 2. 统计引擎
    m_engine = new wordcount::StatisticsEngine(m_db, this);

    // 3. 应用配置到引擎
    m_engine->setCountPunctuation(config.countPunctuation());
    m_engine->setCountEmoji(config.countEmoji());

    // 4. 输入法监听
    m_monitor = new wordcount::Fcitx5Monitor(this);
    connect(m_monitor, &wordcount::Fcitx5Monitor::textCommitted,
            m_engine, &wordcount::StatisticsEngine::onTextCommitted);
    m_monitor->start();

    // 5. 托盘管理器（传入引擎，内部自动订阅 statsChanged）
    m_trayManager = new TrayIconManager(m_engine, this);
    connect(m_trayManager, &TrayIconManager::showPanelRequested,
            &wordcount::DetailPanel::togglePanel);
    connect(m_trayManager, &TrayIconManager::showInsightRequested,
            &wordcount::InsightPanel::togglePanel);
    connect(m_trayManager, &TrayIconManager::resetTodayRequested,
            m_engine, &wordcount::StatisticsEngine::resetToday);
    connect(m_trayManager, &TrayIconManager::resetAllRequested,
            this, &AppCore::onResetAllRequested);
    connect(m_trayManager, &TrayIconManager::settingsRequested,
            this, &AppCore::onSettingsRequested);
    connect(m_trayManager, &TrayIconManager::aboutRequested,
            this, &AppCore::onAboutRequested);
    connect(m_trayManager, &TrayIconManager::quitRequested,
            this, &AppCore::onQuitRequested);

    m_trayManager->init();

    // 6. 面板（传入引擎，内部自动订阅 statsChanged，单例自注册）
    new wordcount::DetailPanel(m_engine);   // 单例自注册到 s_instance
    new wordcount::InsightPanel(m_engine);  // 单例自注册到 s_instance

    // 7. 主题
    wordcount::ThemeHelper::instance().setThemeMode(
        static_cast<wordcount::ThemeMode>(config.themeMode()));

    // 8. 配置变化 → 同步到引擎
    connect(&config, &ConfigManager::countPunctuationChanged,
            m_engine, &wordcount::StatisticsEngine::setCountPunctuation);
    connect(&config, &ConfigManager::countEmojiChanged,
            m_engine, &wordcount::StatisticsEngine::setCountEmoji);

    // 9. 加载数据（触发首次 statsChanged）
    m_engine->load();

    return true;
}

void AppCore::onResetAllRequested()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr, tr("确认重置"),
        tr("确定要永久删除所有统计数据吗？此操作不可撤销！"),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_engine->resetAll();
    }
}

void AppCore::onSettingsRequested()
{
    SettingsDialog dlg(nullptr);
    dlg.setTrayManager(m_trayManager);
    dlg.exec();
    // 配置变更由 ConfigManager 信号驱动，无需额外操作
}

void AppCore::onAboutRequested()
{
    QMessageBox::about(nullptr, tr("关于打字统计"),
                       tr("<b>打字统计 v1.7</b><br>"
                       "统计 Fcitx 5 输入法输入的汉字字数，采用了文人段位系统。<br>"
                       "基于Qt，完全本地运行。"));
}

void AppCore::shutdown()
{
    qDebug() << "AppCore::shutdown() 开始";

    // 1. 先断开信号，防止在清理过程中触发新的操作
    if (m_monitor) {
        disconnect(m_monitor, nullptr, this, nullptr);
    }
    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
    }

    // 2. 停止输入法监听
    if (m_monitor) {
        qDebug() << "停止输入法监听...";
        m_monitor->stop();
        // stop() 已经等待子进程退出，不再需要额外等待
        m_monitor->deleteLater();
        m_monitor = nullptr;
    }

    // 3. 刷新数据到数据库
    if (m_engine) {
        qDebug() << "刷新数据到数据库...";
        m_engine->flushToDatabase();
        m_engine->deleteLater();
        m_engine = nullptr;
    }

    // 4. 托盘图标
    if (m_trayManager) {
        qDebug() << "隐藏托盘图标...";
        m_trayManager->deleteLater();
        m_trayManager = nullptr;
    }

    // 5. 数据库
    if (m_db) {
        qDebug() << "关闭数据库...";
        m_db->close();
        m_db->deleteLater();
        m_db = nullptr;
    }

    // 6. 配置
    ConfigManager::instance().sync();

    // 7. 共享内存
    releaseSingleInstance();

    qDebug() << "AppCore::shutdown() 完成";
}

void AppCore::onQuitRequested()
{
    qDebug() << "收到退出请求";
    // 使用 QTimer::singleShot 避免在信号处理中直接删除
    QTimer::singleShot(0, qApp, &QApplication::quit);
}