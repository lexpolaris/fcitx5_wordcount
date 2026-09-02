// src/ui/tray_icon_manager.h
// SPDX-License-Identifier: LGPL-2.1-or-later
// 托盘图标管理 — 自更新：订阅 StatisticsEngine::statsChanged 自动刷新图标

#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QPixmap>

namespace wordcount {
    class StatisticsEngine;
}

class TrayIconManager : public QObject
{
    Q_OBJECT
public:
    enum DisplayMode { ModeToday, ModeLevel };
    Q_ENUM(DisplayMode)

    /// 构造时传入引擎，自动订阅数据变化
    explicit TrayIconManager(wordcount::StatisticsEngine* engine, QObject* parent = nullptr);
    ~TrayIconManager() override = default;

    void init();
    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_mode; }

signals:
    void showPanelRequested();
    void showInsightRequested();
    void resetTodayRequested();
    void resetAllRequested();
    void settingsRequested();
    void aboutRequested();
    void quitRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void updateIcon();  // 由引擎信号触发

private:
    void setupContextMenu();
    QPixmap generateIcon() const;
    QPixmap generateIconForSize(int baseSize) const;
    bool isDarkTheme() const;

    wordcount::StatisticsEngine* m_engine = nullptr;
    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    QAction* m_actionToday = nullptr;
    QAction* m_actionLevel = nullptr;
    DisplayMode m_mode = ModeToday;
};