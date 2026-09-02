// app_core.h
// SPDX-License-Identifier: LGPL-2.1-or-later
// 创建模块，连接必要的信号路由

#pragma once

#include <QObject>
#include <QSharedMemory>

namespace wordcount {
    class Fcitx5Monitor;
    class StatisticsEngine;
    class DatabaseStorage;
}

class TrayIconManager;
class ConfigManager;

class AppCore : public QObject
{
    Q_OBJECT
public:
    static AppCore& instance();

    bool init();
    void shutdown();

private slots:
    void onResetAllRequested();
    void onSettingsRequested();
    void onAboutRequested();

private:
    AppCore() = default;
    ~AppCore() = default;
    AppCore(const AppCore&) = delete;
    AppCore& operator=(const AppCore&) = delete;

    bool acquireSingleInstance();
    void releaseSingleInstance();

    wordcount::DatabaseStorage* m_db = nullptr;
    wordcount::StatisticsEngine* m_engine = nullptr;
    wordcount::Fcitx5Monitor* m_monitor = nullptr;
    TrayIconManager* m_trayManager = nullptr;

    QSharedMemory m_sharedMem;
    static constexpr const char* kSharedMemKey = "Fcitx5WordCount_SingleInstance";
};