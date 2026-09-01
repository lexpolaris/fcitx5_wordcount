// app_core.h
#pragma once

#include <QObject>
#include <QPointer>

namespace wordcount {
    class Fcitx5Monitor;
    class DetailPanel;
    class StatisticsEngine;
    class DatabaseStorage;
    class InsightPanel;
    struct InsightData;
}

class TrayIconManager;
class ConfigManager;

class AppCore : public QObject
{
    Q_OBJECT
public:
    static AppCore& instance();

    void init();
    void shutdown();

private slots:
    void onStatsChanged(qint64 total, qint64 today, double wpm);
    void onThemeChanged();
    void toggleDetailPanel();
    void showInsightPanel();
    void onSettingsApplied();

    // 托盘菜单触发的业务操作
    void onResetAllRequested();
    void onSettingsRequested();
    void onAboutRequested();
    // 配置响应
    void onConfigChanged();

private:
    AppCore() = default;
    ~AppCore() = default;
    AppCore(const AppCore&) = delete;
    AppCore& operator=(const AppCore&) = delete;

    void loadSettings();
    void applySettings();
    void updateUI();
    void updateInsightData();

    wordcount::DatabaseStorage* m_db = nullptr;
    wordcount::StatisticsEngine* m_engine = nullptr;
    wordcount::Fcitx5Monitor* m_monitor = nullptr;
    wordcount::DetailPanel* m_detailPanel = nullptr;
    TrayIconManager* m_trayManager = nullptr;
    wordcount::InsightPanel* m_insightPanel = nullptr;
};