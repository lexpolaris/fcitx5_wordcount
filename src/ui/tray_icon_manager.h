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

    explicit TrayIconManager(wordcount::StatisticsEngine* engine, QObject* parent = nullptr);
    ~TrayIconManager() override = default;

    void init();
    void updateIcon();
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
