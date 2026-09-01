// app_core.cpp
#include "app_core.h"

#include "core/fcitx5_monitor.h"
#include "ui/detail_panel.h"
#include "ui/insight_panel.h"
#include "engine/statistics_engine.h"
#include "engine/database_storage.h" 
#include "ui/tray_icon_manager.h"
#include "ui/settings_dialog.h"
#include "ui/theme_helper.h"
#include "ui/config_manager.h"
#include "core/level_system.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>

// ========== 单例 ==========
AppCore& AppCore::instance()
{
    static AppCore core;
    return core;
}

// ========== 初始化 ==========
void AppCore::init()
{
    // 1. 配置存储
    auto& config = ConfigManager::instance();
    
    // ---- 创建数据库存储 ----
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/Fcitx5WordCount";
    QDir().mkpath(configDir);
    QString dbPath = configDir + "/stats.db";

    m_db = new wordcount::DatabaseStorage(dbPath, this);
    if (!m_db->init()) {
        qWarning() << "数据库初始化失败，应用将无法统计";
        // 仍继续运行，但所有查询返回0
    }

    // ---- 创建统计引擎 ----
    m_engine = new wordcount::StatisticsEngine(m_db, this);

    // 2. 加载应用设置并应用到引擎
    loadSettings();
    applySettings();

    // 3. 输入法监听
    m_monitor = new wordcount::Fcitx5Monitor(this);
    connect(m_monitor, &wordcount::Fcitx5Monitor::textCommitted,
            m_engine, &wordcount::StatisticsEngine::onTextCommitted);
    m_monitor->start();

    // 4. 托盘管理器
    m_trayManager = new TrayIconManager(m_engine, this);
    connect(m_trayManager, &TrayIconManager::showPanelRequested,
            this, &AppCore::toggleDetailPanel);
    connect(m_trayManager, &TrayIconManager::showInsightRequested,
            this, &AppCore::showInsightPanel);
    connect(m_trayManager, &TrayIconManager::resetTodayRequested,
            m_engine, &wordcount::StatisticsEngine::resetToday);
    connect(m_trayManager, &TrayIconManager::resetAllRequested,
            this, &AppCore::onResetAllRequested);
    connect(m_trayManager, &TrayIconManager::settingsRequested,
            this, &AppCore::onSettingsRequested);
    connect(m_trayManager, &TrayIconManager::aboutRequested,
            this, &AppCore::onAboutRequested);
    connect(m_trayManager, &TrayIconManager::quitRequested,
            qApp, &QApplication::quit);

    m_trayManager->init();

    // 5. 详情面板（纯UI，无业务逻辑）
    m_detailPanel = new wordcount::DetailPanel(nullptr);
    m_detailPanel->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // 6. 统计变化信号
    connect(m_engine, &wordcount::StatisticsEngine::statsChanged,
            this, &AppCore::onStatsChanged);

    // 7. 主题变化
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    connect(qApp, &QApplication::paletteChanged, this, &AppCore::onThemeChanged);
    #pragma GCC diagnostic pop

    // 连接主题变化
    connect(&wordcount::ThemeHelper::instance(),
            &wordcount::ThemeHelper::themeChanged,
            this, &AppCore::onThemeChanged);

    // 应用主题
    wordcount::ThemeHelper::instance().setThemeMode(
        static_cast<wordcount::ThemeMode>(config.themeMode()));

    // 8. 连接配置变化信号
    connect(&config, &ConfigManager::anySettingChanged,
            this, &AppCore::onConfigChanged);
    connect(&config, &ConfigManager::countPunctuationChanged,
            this, &AppCore::onConfigChanged);
    connect(&config, &ConfigManager::countEmojiChanged,
            this, &AppCore::onConfigChanged);
    connect(&config, &ConfigManager::trayDisplayModeChanged,
            this, &AppCore::onConfigChanged);
    connect(&config, &ConfigManager::themeModeChanged,
            this, &AppCore::onConfigChanged);

    // 9. 统计仪表盘
    m_insightPanel = new wordcount::InsightPanel(nullptr);

    // 10. 首次刷新
    updateUI();
}

// ========== 配置加载与应用 ==========
void AppCore::loadSettings()
{
    // 读取设置（使用默认值），实际值在 applySettings 中应用
}

void AppCore::applySettings()
{
    if (!m_engine) return;

    auto& config = ConfigManager::instance();

    // 应用统计设置
    m_engine->setCountPunctuation(config.countPunctuation());
    m_engine->setCountEmoji(config.countEmoji());

    // 应用托盘显示模式
    if (m_trayManager) {
        m_trayManager->setDisplayMode(
            static_cast<TrayIconManager::DisplayMode>(config.trayDisplayMode()));
    }
}

void AppCore::onSettingsApplied()
{
    // 设置对话框确认后重新加载并应用
    // 由于 ConfigManager 已经自动同步，只需重新应用
    applySettings();
}

// ========== 配置变化响应 ==========
void AppCore::onConfigChanged()
{
    // 配置发生变化时，重新应用设置
    applySettings();
    // 更新 UI
    updateUI();
}

// ========== 业务操作槽 ==========
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
    connect(&dlg, &SettingsDialog::settingsApplied, this, &AppCore::onSettingsApplied);
    dlg.exec();
}

void AppCore::onAboutRequested()
{
    QMessageBox::about(nullptr, tr("关于打字统计"),
                       tr("<b>打字统计 v1.6</b><br>"
                       "统计 Fcitx 5 输入法输入的汉字字数，采用了文人段位系统。<br>"
                       "基于Qt，完全本地运行。"));
}

// ========== 面板显示 ==========
void AppCore::toggleDetailPanel()
{
    if (!m_detailPanel) return;

    if (m_detailPanel->isVisible()) {
        m_detailPanel->hide();
        return;
    }

    QPoint cursorPos = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursorPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) {
        m_detailPanel->move(100, 100);
        m_detailPanel->show();
        return;
    }

    QRect screenRect = screen->availableGeometry();
    int panelW = m_detailPanel->width();
    int panelH = m_detailPanel->height();

    QPoint pos = cursorPos + QPoint(10, 10);
    if (pos.x() + panelW > screenRect.right())
        pos.setX(screenRect.right() - panelW);
    if (pos.y() + panelH > screenRect.bottom())
        pos.setY(screenRect.bottom() - panelH);
    if (pos.x() < screenRect.left()) pos.setX(screenRect.left());
    if (pos.y() < screenRect.top()) pos.setY(screenRect.top());

    m_detailPanel->move(pos);
    m_detailPanel->show();
    m_detailPanel->raise();
    m_detailPanel->activateWindow();
}

// ========== 显示统计仪表盘 ==========
void AppCore::showInsightPanel()
{
    if (!m_insightPanel) return;

    // 先刷新数据
    updateInsightData();

    // 如果窗口已最小化，恢复它
    if (m_insightPanel->isMinimized()) {
        m_insightPanel->showNormal();
    } else {
        m_insightPanel->show();
    }
    m_insightPanel->raise();
    m_insightPanel->activateWindow();
}

// ========== 更新仪表盘数据 ==========
void AppCore::updateInsightData()
{
    if (!m_engine || !m_insightPanel) return;

    wordcount::InsightData data;
    
    // 今日数据
    data.todayChars = m_engine->todayChars();
    data.todayCommits = m_engine->todayCommits();
    data.avgSpeed = m_engine->currentWpm();
    data.peakSpeed = m_engine->peakSpeed();

    // 字词分布（需要在 StatisticsEngine 中实现）
    auto dist = m_engine->distToday();
    data.dist1 = dist[0];
    data.dist2 = dist[1];
    data.dist3 = dist[2];
    data.dist4 = dist[3];
    data.dist5plus = dist[4];

    // 段位信息
    auto levelInfo = wordcount::LevelSystem::levelForTotal(m_engine->totalChars());
    data.tierName = levelInfo.tierName;
    data.tierRank = levelInfo.tierRank;
    data.tierSubtitle = QString::fromUtf8(
        wordcount::LevelSystem::tierAt(levelInfo.tierIndex).fullName);
    data.tierColor = levelInfo.tierColor;
    data.progress = levelInfo.progress;
    data.nextThreshold = levelInfo.nextMin;
    data.totalChars = m_engine->totalChars();

    // 近7天
    QJsonArray daily7 = m_engine->daily7();
    for (int i = 0; i < 7 && i < daily7.size(); ++i) {
        data.daily7[i] = daily7.at(i).toVariant().toLongLong();
    }

    m_insightPanel->refresh(data);
}

// ========== 统计变化 ==========
void AppCore::onStatsChanged(qint64 total, qint64 today, double wpm)
{
    Q_UNUSED(total);
    Q_UNUSED(today);
    Q_UNUSED(wpm);
    
    // 更新详情面板
    updateUI();
    
    // 如果仪表盘可见，更新它
    if (m_insightPanel && m_insightPanel->isVisible()) {
        updateInsightData();
    }
}

void AppCore::updateUI()
{
    if (!m_engine || !m_detailPanel) return;

    m_detailPanel->refresh(
        m_engine->totalChars(),
        m_engine->todayChars(),
        m_engine->currentWpm(),
        m_engine->hourlyToday(),
        m_engine->daily7()
    );

    if (m_trayManager) m_trayManager->updateIcon();
}

// ========== 主题变化 ==========
void AppCore::onThemeChanged()
{
    if (m_trayManager) m_trayManager->updateIcon();
}

// ========== 关闭 ==========
void AppCore::shutdown()
{
    if (m_engine) {
        m_engine->flushToDatabase();
    }

    if (m_monitor) m_monitor->stop();

    ConfigManager::instance().sync();
}