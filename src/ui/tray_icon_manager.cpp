// src/ui/tray_icon_manager.cpp
// SPDX-License-Identifier: LGPL-2.1-or-later
// 托盘图标管理实现

#include "tray_icon_manager.h"
#include "engine/statistics_engine.h"
#include "core/level_system.h"
#include "theme_helper.h"
#include "config_manager.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>

TrayIconManager::TrayIconManager(wordcount::StatisticsEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
    // 主题变化时刷新图标
    connect(&wordcount::ThemeHelper::instance(),
            &wordcount::ThemeHelper::themeChanged,
            this, &TrayIconManager::updateIcon);

    // 配置变化时切换显示模式
    connect(&ConfigManager::instance(), &ConfigManager::trayDisplayModeChanged,
            this, [this](int mode) {
                m_mode = static_cast<DisplayMode>(mode);
                updateIcon();
            });

    // 引擎数据变化时自动刷新图标
    if (m_engine) {
        connect(m_engine, &wordcount::StatisticsEngine::statsChanged,
                this, &TrayIconManager::updateIcon);
    }
}

void TrayIconManager::init()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "系统托盘不可用";
        return;
    }

    m_mode = static_cast<DisplayMode>(ConfigManager::instance().trayDisplayMode());

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(generateIcon()));
    m_trayIcon->show();

    setupContextMenu();
    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &TrayIconManager::onActivated);

    // 首次刷新
    updateIcon();
}

void TrayIconManager::updateIcon()
{
    if (!m_trayIcon) {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->show();
        setupContextMenu();
        m_trayIcon->setContextMenu(m_trayMenu);
        connect(m_trayIcon, &QSystemTrayIcon::activated,
                this, &TrayIconManager::onActivated);
    }

    m_trayIcon->setIcon(QIcon(generateIcon()));

    if (m_engine) {
        QString tip = tr("今日: %1 字\n总计: %2 字")
            .arg(m_engine->todayChars())
            .arg(m_engine->totalChars());
        m_trayIcon->setToolTip(tip);
    }
}

void TrayIconManager::setDisplayMode(DisplayMode mode)
{
    // 更新菜单状态
    if (m_actionToday) m_actionToday->setChecked(mode == ModeToday);
    if (m_actionLevel) m_actionLevel->setChecked(mode == ModeLevel);

    if (m_mode == mode) {
        // 即使相同也强制刷新图标
        updateIcon();
        return;
    }

    m_mode = mode;
    ConfigManager::instance().setTrayDisplayMode(static_cast<int>(mode));

    // 使用 QTimer 延迟刷新，确保界面更新完成
    QTimer::singleShot(50, this, &TrayIconManager::updateIcon);
}

void TrayIconManager::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        emit showPanelRequested();
    }
}

void TrayIconManager::setupContextMenu()
{
    m_trayMenu = new QMenu();

    // 显示面板
    QAction* showAction = new QAction(tr("显示面板"), this);
    connect(showAction, &QAction::triggered, this, &TrayIconManager::showPanelRequested);
    m_trayMenu->addAction(showAction);

    m_trayMenu->addSeparator();

    // 显示模式子菜单
    QMenu* displayMenu = m_trayMenu->addMenu(tr("托盘图标显示"));
    QActionGroup* group = new QActionGroup(this);
    group->setExclusive(true);

    m_actionToday = new QAction(tr("今日字数"), this);
    m_actionToday->setCheckable(true);
    m_actionToday->setChecked(m_mode == ModeToday);
    connect(m_actionToday, &QAction::triggered, this, [this]() {
        setDisplayMode(ModeToday);
    });
    group->addAction(m_actionToday);
    displayMenu->addAction(m_actionToday);

    m_actionLevel = new QAction(tr("段位进度"), this);
    m_actionLevel->setCheckable(true);
    m_actionLevel->setChecked(m_mode == ModeLevel);
    connect(m_actionLevel, &QAction::triggered, this, [this]() {
        setDisplayMode(ModeLevel);
    });
    group->addAction(m_actionLevel);
    displayMenu->addAction(m_actionLevel);

    m_trayMenu->addSeparator();

    // 效率仪表盘
    QAction* insightAction = new QAction(tr("效率仪表盘"), this);
    connect(insightAction, &QAction::triggered, this, &TrayIconManager::showInsightRequested);
    m_trayMenu->addAction(insightAction);

    m_trayMenu->addSeparator();

    // 重置今日
    QAction* resetTodayAction = new QAction(tr("重置今日"), this);
    connect(resetTodayAction, &QAction::triggered, this, &TrayIconManager::resetTodayRequested);
    m_trayMenu->addAction(resetTodayAction);

    // 重置全部
    QAction* resetAllAction = new QAction(tr("重置全部"), this);
    connect(resetAllAction, &QAction::triggered, this, &TrayIconManager::resetAllRequested);
    m_trayMenu->addAction(resetAllAction);

    m_trayMenu->addSeparator();

    // 设置
    QAction* settingsAction = new QAction(tr("设置..."), this);
    connect(settingsAction, &QAction::triggered, this, &TrayIconManager::settingsRequested);
    m_trayMenu->addAction(settingsAction);

    m_trayMenu->addSeparator();

    // 关于
    QAction* aboutAction = new QAction(tr("关于"), this);
    connect(aboutAction, &QAction::triggered, this, &TrayIconManager::aboutRequested);
    m_trayMenu->addAction(aboutAction);

    m_trayMenu->addSeparator();

    // 退出
    QAction* quitAction = new QAction(tr("退出"), this);
    connect(quitAction, &QAction::triggered, this, &TrayIconManager::quitRequested);
    m_trayMenu->addAction(quitAction);
}

bool TrayIconManager::isDarkTheme() const
{
    return wordcount::ThemeHelper::instance().isDarkTheme();
}

QPixmap TrayIconManager::generateIcon() const
{
    // 获取系统托盘图标的标准尺寸
    int baseSize = 32;

    #ifdef Q_OS_WIN
    baseSize = 16;
    #elif defined(Q_OS_MAC)
    baseSize = 22;
    #else
    baseSize = 24;
    #endif

    // 如果有托盘图标且 geometry 可用，使用实际尺寸
    if (m_trayIcon) {
        QRect trayRect = m_trayIcon->geometry();
        if (trayRect.isValid() && trayRect.width() > 0 && trayRect.height() > 0) {
            int size = qMin(trayRect.width(), trayRect.height());
            if (size >= 16 && size <= 64) {
                baseSize = size;
            }
        }
    }

    return generateIconForSize(baseSize);
}

QPixmap TrayIconManager::generateIconForSize(int baseSize) const
{
    const qreal dpr = qApp->devicePixelRatio();
    const int size = static_cast<int>(baseSize * dpr);
    QPixmap pixmap(size, size);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 使用静态方法调用
    QColor bgColor = wordcount::ThemeHelper::iconBgColor();
    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, size*0.2, size*0.2);
    painter.fillPath(path, bgColor);

    if (m_mode == ModeToday) {
        // 今日字数
        QString displayText = QString::number(m_engine->todayChars());
        bool ok;
        qint64 num = displayText.toLongLong(&ok);
        if (ok && num >= 0) {
            if (num >= 100000000)
                displayText = QString::number(num / 10000000 / 10.0) + "亿";
            else if (num >= 10000)
                displayText = QString::number(num / 1000 / 10.0) + "万";
            else if (num >= 1000)
                displayText = QString::number(num / 100 / 10.0) + "千";
        }

        QColor textColor = wordcount::ThemeHelper::iconTextColor();
        QFont font = painter.font();
        font.setBold(true);

        // 根据实际大小调整字体
        int fontSize = static_cast<int>(baseSize * 0.6);
        font.setPixelSize(fontSize);
        painter.setFont(font);
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(displayText);

        // 如果文字太宽，缩小字体
        while (textWidth > baseSize * 0.85 && fontSize > 8) {
            fontSize--;
            font.setPixelSize(fontSize);
            painter.setFont(font);
            fm = QFontMetrics(font);
            textWidth = fm.horizontalAdvance(displayText);
        }
        painter.setPen(textColor);
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, displayText);
    } else {
        // 段位进度环
        wordcount::LevelInfo info = wordcount::LevelSystem::levelForTotal(m_engine->totalChars());
        QString shortName = info.tierShort;
        double progress = info.progress;
        QColor tierColor = info.tierColor;

        QColor color = wordcount::ThemeHelper::adjustedColor(tierColor);

        int margin = size * 0.1;
        int penWidth = size * 0.1;
        QRectF rect(margin, margin, size - 2*margin, size - 2*margin);

        // 背景圆环
        QColor bgRing = wordcount::ThemeHelper::iconRingBgColor();
        painter.setPen(QPen(bgRing, penWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(rect);

        // 前景弧（进度）
        if (progress > 0.0) {
            painter.setPen(QPen(color, penWidth));
            int startAngle = -90 * 16;
            int spanAngle = static_cast<int>(progress * 360 * 16);
            painter.drawArc(rect, startAngle, spanAngle);
        }

        // 段位缩写
        QColor textColor = wordcount::ThemeHelper::iconTextColor();
        QFont font = painter.font();
        font.setBold(true);
        int fontSize = static_cast<int>(size * 0.4);
        font.setPixelSize(fontSize);
        painter.setFont(font);
        painter.setPen(textColor);
        painter.drawText(rect, Qt::AlignCenter, shortName);
    }

    return pixmap;
}
