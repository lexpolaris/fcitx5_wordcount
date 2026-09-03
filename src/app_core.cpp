// src/app_core.cpp
// SPDX-License-Identifier: LGPL-2.1-or-later
// 应用核心实现

#include "app_core.h"

#include "core/fcitx5_monitor.h"
#include "core/level_system.h"
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
    // 使用 QLockFile 实现更可靠的单例检测
    // QLockFile 在进程异常退出时会自动释放锁（通过文件锁的机制）
    QString lockDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (lockDir.isEmpty()) {
        lockDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    QString lockPath = lockDir + "/" + kLockFileName;
    
    // 确保目录存在
    QDir().mkpath(lockDir);
    
    m_lockFile = new QLockFile(lockPath);  // 修正：只传路径，不传 parent
    
    // 设置锁的过期时间，防止僵尸锁
    m_lockFile->setStaleLockTime(5000);  // 5秒后认为锁已过期
    
    if (m_lockFile->tryLock()) {
        qDebug() << "单例锁获取成功:" << lockPath;
        return true;
    } else {
        // 获取锁失败，检查是否是僵尸锁
        qint64 pid;
        QString hostname, appname;
        if (m_lockFile->getLockInfo(&pid, &hostname, &appname)) {
            qWarning() << "已有程序实例在运行 (PID:" << pid << ", Host:" << hostname << ")";
        } else {
            qWarning() << "已有程序实例在运行";
        }
        
        delete m_lockFile;
        m_lockFile = nullptr;
        return false;
    }
}

void AppCore::releaseSingleInstance()
{
    if (m_lockFile) {
        m_lockFile->unlock();
        delete m_lockFile;
        m_lockFile = nullptr;
        qDebug() << "单例锁已释放";
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

    // 8. 应用段位方案
    int schemeIndex = config.tierScheme();
    wordcount::LevelSystem::setScheme(schemeIndex);

    // 9. 配置变化 → 同步到引擎
    connect(&config, &ConfigManager::countPunctuationChanged,
            m_engine, &wordcount::StatisticsEngine::setCountPunctuation);
    connect(&config, &ConfigManager::countEmojiChanged,
            m_engine, &wordcount::StatisticsEngine::setCountEmoji);

    // 10. 加载数据（触发首次 statsChanged）
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
    dlg.setEngine(m_engine);
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

    // 6. 释放单例锁
    releaseSingleInstance();

    // 7. 配置
    ConfigManager::instance().sync();

    qDebug() << "AppCore::shutdown() 完成";
}

void AppCore::onQuitRequested()
{
    qDebug() << "收到退出请求";
    // 使用 QTimer::singleShot 避免在信号处理中直接删除
    QTimer::singleShot(0, qApp, &QApplication::quit);
}