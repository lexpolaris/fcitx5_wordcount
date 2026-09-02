// src/ui/detail_panel.cpp
// SPDX-License-Identifier: LGPL-2.1-or-later
// 详情面板实现 — 自管理数据订阅和显示

#include "detail_panel.h"
#include "engine/statistics_engine.h"
#include "core/level_system.h"
#include "core/word_equivalence.h"
#include "theme_helper.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QShowEvent>
#include <QFont>
#include <QFontMetrics>
#include <QDate>
#include <QLinearGradient>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <cmath>

namespace wordcount {

    // ========== 静态实例管理 ==========
    QPointer<DetailPanel> DetailPanel::s_instance;

    DetailPanel* DetailPanel::instance()
    {
        return s_instance;
    }

    bool DetailPanel::isVisible()
    {
        return s_instance && s_instance->QWidget::isVisible();
    }

    void DetailPanel::showPanel()
    {
        if (!s_instance) return;

        // 计算鼠标位置附近的位置
        QPoint cursorPos = QCursor::pos();
        QScreen* screen = QGuiApplication::screenAt(cursorPos);
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (!screen) {
            s_instance->move(100, 100);
            s_instance->show();
            return;
        }

        QRect screenRect = screen->availableGeometry();
        int panelW = s_instance->width();
        int panelH = s_instance->height();

        QPoint pos = cursorPos + QPoint(10, 10);
        if (pos.x() + panelW > screenRect.right())
            pos.setX(screenRect.right() - panelW);
        if (pos.y() + panelH > screenRect.bottom())
            pos.setY(screenRect.bottom() - panelH);
        if (pos.x() < screenRect.left()) pos.setX(screenRect.left());
        if (pos.y() < screenRect.top()) pos.setY(screenRect.top());

        s_instance->move(pos);
        s_instance->show();
        s_instance->raise();
        s_instance->activateWindow();
    }

    void DetailPanel::togglePanel()
    {
        if (!s_instance) return;
        if (s_instance->isVisible()) {
            s_instance->hide();
        } else {
            showPanel();
        }
    }

    void DetailPanel::hidePanel()
    {
        if (s_instance) s_instance->hide();
    }

    // ========== 构造 / 析构 ==========
    DetailPanel::DetailPanel(StatisticsEngine* engine, QWidget *parent)
        : QWidget(parent)
        , m_engine(engine)
    {
        setFixedSize(kPanelWidth, kPanelHeight);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setFocusPolicy(Qt::NoFocus);

        // 获取系统字体缩放因子
        qreal dpr = devicePixelRatioF();
        int basePointSize = 10;

        // 使用点大小自动适配 DPI
        m_fontToday   = QFont("", basePointSize + 4, QFont::Bold);   // 14pt
        m_fontT4      = QFont("", basePointSize + 2, QFont::Bold);   // 12pt
        m_fontT5      = QFont("", basePointSize + 1, QFont::Bold);   // 11pt
        m_fontT7      = QFont("", basePointSize - 1);                // 9pt
        m_fontT8      = QFont("", basePointSize - 2);                // 8pt
        m_fontT10     = QFont("", basePointSize - 3);                // 7pt

        m_fontToday.setStyleStrategy(QFont::PreferMatch);
        m_fontT4.setStyleStrategy(QFont::PreferMatch);
        m_fontT5.setStyleStrategy(QFont::PreferMatch);
        m_fontT7.setStyleStrategy(QFont::PreferMatch);
        m_fontT8.setStyleStrategy(QFont::PreferMatch);
        m_fontT10.setStyleStrategy(QFont::PreferMatch);

        m_fmToday = QFontMetrics(m_fontToday);
        m_fmT4 = QFontMetrics(m_fontT4);
        m_fmT5 = QFontMetrics(m_fontT5);
        m_fmT7 = QFontMetrics(m_fontT7);
        m_fmT8 = QFontMetrics(m_fontT8);
        m_fmT10 = QFontMetrics(m_fontT10);

        // ★ 订阅引擎数据更新
        if (m_engine) {
            connect(m_engine, &StatisticsEngine::statsChanged,
                    this, &DetailPanel::onStatsChanged);
            // 首次加载数据
            updateData();
        }

        // 主题变化时刷新
        connect(&ThemeHelper::instance(), &ThemeHelper::themeChanged,
                this, QOverload<>::of(&QWidget::update));

        // 注册单例
        s_instance = this;
    }

    DetailPanel::~DetailPanel()
    {
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    // ========== 数据显示事件 ==========
    void DetailPanel::showEvent(QShowEvent *event)
    {
        QWidget::showEvent(event);
        // 显示时确保数据最新
        if (m_engine) updateData();
    }

    // ========== 数据更新 ==========
    void DetailPanel::onStatsChanged(qint64 total, qint64 today, double wpm)
    {
        Q_UNUSED(total);
        Q_UNUSED(today);
        Q_UNUSED(wpm);
        updateData();
        if (isVisible()) update();
    }

    void DetailPanel::updateData()
    {
        if (!m_engine) return;

        m_total = m_engine->totalChars();
        m_today = m_engine->todayChars();
        m_wpm = m_engine->currentWpm();
        m_hourly = m_engine->hourlyToday();
        m_daily7 = m_engine->daily7();

        // ---- 等级信息 ----
        LevelInfo lv = LevelSystem::levelForTotal(m_total);
        m_tierName = lv.tierName;
        m_tierRank = lv.tierRank;
        m_tierColor = lv.tierColor;
        m_levelProgress = lv.progress;
        m_isMax = lv.isMax;

        m_tierLine = QStringLiteral("%1 · %2").arg(m_tierName).arg(m_tierRank);

        if (m_isMax) {
            m_tierSubtitle = QStringLiteral("已达%1").arg(QString::fromUtf8(
                LevelSystem::tierAt(lv.tierIndex).fullName));
            m_progressLine = QStringLiteral("文坛巅峰");
        } else {
            m_tierSubtitle = QString::fromUtf8(LevelSystem::tierAt(lv.tierIndex).fullName);
            const QString nextRank = QString::fromUtf8(
                LevelSystem::rankName(lv.levelInTier + 1));
            const qint64 remain = qMax<qint64>(0, lv.nextMin - m_total);
            m_progressLine = QStringLiteral("%1% · 距%2还差%3字")
                .arg(static_cast<int>(m_levelProgress * 100))
                .arg(nextRank)
                .arg(formatNumber(remain));
        }

        // ---- 卡片数据 ----
        m_cardToday = formatNumber(m_today);
        m_cardSpeed = QStringLiteral("%1 字/分").arg(static_cast<int>(m_wpm));
        m_cardTotal = formatNumber(m_total);

        // ---- 徽章（与昨日对比） ----
        qint64 yesterday = 0;
        if (m_daily7.size() >= 6) {
            yesterday = m_daily7.at(5).toVariant().toLongLong();
        }
        const qint64 diff = m_today - yesterday;
        if (yesterday <= 0 && diff <= 0) {
            m_badgeText = m_cardToday;
            m_badgeColor = adjustedTierColor();
        } else if (diff >= 0) {
            m_badgeText = QStringLiteral("+%1").arg(formatNumber(diff));
            m_badgeColor = adjustedTierColor();
        } else {
            m_badgeText = QStringLiteral("-%1").arg(formatNumber(-diff));
            m_badgeColor = QColor(QStringLiteral("#E64545"));
        }

        // ---- 7日趋势 ----
        qint64 base = 0, last = 0;
        if (m_daily7.size() >= 7) {
            base = m_daily7.at(0).toVariant().toLongLong();
            last = m_daily7.at(6).toVariant().toLongLong();
        }
        if (base <= 0) {
            m_daily7Delta = last > 0 ? QStringLiteral("▲ 今日 %1 字").arg(formatNumber(last))
                                     : QStringLiteral("暂无数据");
        } else {
            const int rawPct = static_cast<int>(static_cast<double>(last - base) / base * 100);
            const int pct = qBound(-999, rawPct, 999);
            m_daily7Delta = pct >= 0 ? QStringLiteral("▲ %1%").arg(pct)
                                     : QStringLiteral("▼ %1%").arg(-pct);
        }

        // 清空扩展字段
        m_peakLine.clear();
        m_peakIsNewRecord = false;
        m_rhythmTitle.clear();
        m_rhythmSubtitle.clear();
        m_rhythmProgress = 0.0;
    }

    // ========== 绘制 ==========
    void DetailPanel::paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // 背景
        p.setPen(Qt::NoPen);
        p.setBrush(bgColor());
        p.drawRoundedRect(rect(), 12, 12);

        // 边框
        p.setPen(QPen(separatorColor(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

        // 各区域
        int y = 0;
        QRect heroArea(0, y, kPanelWidth, 100); y += 100;
        QRect cardsArea(0, y, kPanelWidth, 82); y += 82;
        QRect daily7Area(0, y, kPanelWidth, 104); y += 104;
        QRect hourlyArea(0, y, kPanelWidth, 78); y += 78;
        QRect equivalenceArea(0, y, kPanelWidth, 56);

        // 分隔线
        p.setPen(QPen(separatorColor(), 1));
        p.drawLine(0, heroArea.bottom(), kPanelWidth, heroArea.bottom());

        drawHero(p, heroArea);
        drawNumberCards(p, cardsArea);
        drawDaily7(p, daily7Area);
        drawHourlyHeatmap(p, hourlyArea);
        drawEquivalence(p, equivalenceArea);
    }

    // ========== 绘制各区域（与原来完全相同，只是从成员变量读取数据） ==========

    void DetailPanel::drawHero(QPainter &p, const QRect &area)
    {
        // 右侧今日数据
        const int rightW = 110;
        const int rightX = area.x() + area.width() - 16 - rightW;
        const QRect rightRect(rightX, area.y() + 8, rightW, area.height() - 16);

        // 第一行："今日"标签
        const int row1Y = rightRect.y();
        const int row1H = 16;
        p.setFont(m_fontT10);
        p.setPen(subTextColor());
        p.drawText(QRect(rightRect.x(), row1Y, rightRect.width(), row1H),
                   Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("今日已输入"));

        // 第二行：今日数字
        const int row2Y = row1Y + row1H + 2;
        const int row2H = 30;
        p.setFont(m_fontToday);
        p.setPen(textColor());
        p.drawText(QRect(rightRect.x(), row2Y, rightRect.width(), row2H),
                   Qt::AlignRight | Qt::AlignVCenter, m_cardToday);

        // 左侧等级信息
        const int leftW = rightX - area.x() - 24;

        p.setFont(m_fontT4);
        p.setPen(adjustedTierColor());
        const QString tierLine = m_fmT4.horizontalAdvance(m_tierLine) > leftW ?
            m_fmT4.elidedText(m_tierLine, Qt::ElideRight, leftW) : m_tierLine;
        p.drawText(QRect(area.x() + 16, area.y() + 12, leftW, 26),
                   Qt::AlignLeft | Qt::AlignVCenter, tierLine);

        p.setFont(m_fontT8);
        p.setPen(subTextColor());
        p.drawText(QRect(area.x() + 16, area.y() + 36, leftW, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, m_tierSubtitle);

        // 进度条
        const int barX = area.x() + 16;
        const int barY = area.y() + area.height() - 34;
        const int barH = 8;
        const int barW = area.width() - 32;

        QColor trackColor = adjustedTierColor();
        trackColor.setAlpha(70);
        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        p.drawRoundedRect(barX, barY, barW, barH, barH/2, barH/2);

        int filledW = static_cast<int>(barW * m_levelProgress);
        if (filledW > 0) {
            QRect filledRect(barX, barY, filledW, barH);
            p.setBrush(adjustedTierColor());
            p.drawRoundedRect(filledRect, barH/2, barH/2);
        }

        p.setFont(m_fontT8);
        p.setPen(subTextColor());
        const QString progressLine = m_fmT8.horizontalAdvance(m_progressLine) > barW ?
            m_fmT8.elidedText(m_progressLine, Qt::ElideRight, barW) : m_progressLine;
        p.drawText(QRect(barX, barY + barH + 4, barW, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, progressLine);
    }

    void DetailPanel::drawNumberCards(QPainter &p, const QRect &area)
    {
        struct Card { QString label; const QString *value; };
        const Card cards[2] = {
            { QStringLiteral("速度"), &m_cardSpeed },
            { QStringLiteral("总计"), &m_cardTotal },
        };
        const int gap = 10;
        const int cardW = (area.width() - 16*2 - gap) / 2;
        const int cardH = area.height() - 20;
        const int cardY = area.y() + 10;

        for (int i = 0; i < 2; ++i) {
            QRect cardRect(area.x() + 16 + i * (cardW + gap), cardY, cardW, cardH);
            p.setPen(Qt::NoPen);
            p.setBrush(cardBgColor());
            p.drawRoundedRect(cardRect, 10, 10);

            p.setFont(m_fontT10);
            p.setPen(subTextColor());
            p.drawText(QRect(cardRect.x() + 12, cardRect.y() + 6,
                             cardRect.width() - 12, 16),
                       Qt::AlignLeft | Qt::AlignVCenter, cards[i].label);

            p.setFont(m_fontT5);
            p.setPen(textColor());
            p.drawText(QRect(cardRect.x() + 12, cardRect.y() + 30,
                             cardRect.width() - 12, 24),
                       Qt::AlignLeft | Qt::AlignVCenter, *cards[i].value);
        }
    }

    void DetailPanel::drawHourlyHeatmap(QPainter &p, const QRect &area)
    {
        p.setFont(m_fontT7);
        p.setPen(textColor());
        p.drawText(QRect(area.x() + 16, area.y() + 4, area.width() - 32, 20),
                   Qt::AlignLeft, QStringLiteral("今日时段分布"));

        const int barH = 20;
        const int barY = area.y() + 30;
        const int chartX = area.x() + 16;
        const int chartW = area.width() - 32;
        const qreal cellW = static_cast<qreal>(chartW) / 24.0;
        const int gap = 1;

        qint64 maxVal = 0;
        for (int h = 0; h < 24; ++h) {
            qint64 v = m_hourly.size() > h ? m_hourly.at(h).toVariant().toLongLong() : 0;
            if (v > maxVal) maxVal = v;
        }
        if (maxVal == 0) maxVal = 1;

        for (int h = 0; h < 24; ++h) {
            qint64 v = m_hourly.size() > h ? m_hourly.at(h).toVariant().toLongLong() : 0;
            if (v <= 0) continue;
            double intensity = static_cast<double>(v) / maxVal;
            QColor c = heatColor();
            c.setAlpha(static_cast<int>(100 + intensity * 155));
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            QRectF cell(chartX + h * cellW + gap * 0.5, barY, cellW - gap, barH);
            p.drawRoundedRect(cell, 3, 3);
        }

        p.setFont(m_fontT10);
        p.setPen(subTextColor());
        int labelY = barY + barH + 4;
        for (int h : {0, 6, 12, 18, 23}) {
            int x = static_cast<int>(chartX + h * cellW + cellW / 2.0);
            p.drawText(QRect(x - 10, labelY, 20, 12),
                       Qt::AlignCenter, QString::number(h));
        }
    }

    void DetailPanel::drawDaily7(QPainter &p, const QRect &area)
    {
        const QString title = QStringLiteral("近 7 日");
        p.setFont(m_fontT7);
        p.setPen(textColor());
        p.drawText(QRect(area.x() + 16, area.y() + 4, area.width() - 32, 20),
                   Qt::AlignLeft, title);

        p.setFont(m_fontT10);
        p.setPen(heatColor());
        const int titleW = m_fmT7.horizontalAdvance(title);
        const int deltaW = m_fmT10.horizontalAdvance(m_daily7Delta);
        p.drawText(QRect(area.x() + 16 + titleW + 8, area.y() + 6, deltaW + 4, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, m_daily7Delta);

        int chartX = area.x() + 16;
        int chartY = area.y() + 26;
        int chartW = area.width() - 32;
        int chartH = area.height() - 50;

        qint64 vals[7] = {0};
        qint64 maxVal = 0;
        for (int d = 0; d < 7; ++d) {
            vals[d] = m_daily7.size() > d ? m_daily7.at(d).toVariant().toLongLong() : 0;
            if (vals[d] > maxVal) maxVal = vals[d];
        }
        if (maxVal == 0) maxVal = 1;

        p.setPen(QPen(separatorColor(), 1));
        p.drawLine(chartX, chartY + chartH - 2, chartX + chartW, chartY + chartH - 2);

        int barW = chartW / 7;
        QPointF pts[7];
        for (int d = 0; d < 7; ++d) {
            double h = static_cast<double>(vals[d]) / maxVal * (chartH - 8);
            double x = chartX + d * barW + barW / 2.0;
            double y = chartY + chartH - 2 - h;
            pts[d] = QPointF(x, y);
        }

        QPainterPath linePath;
        linePath.moveTo(pts[0]);
        for (int d = 0; d < 6; ++d) {
            const QPointF &p0 = pts[qMax(d - 1, 0)];
            const QPointF &p1 = pts[d];
            const QPointF &p2 = pts[d + 1];
            const QPointF &p3 = pts[qMin(d + 2, 6)];
            const QPointF c1 = p1 + (p2 - p0) / 6.0;
            const QPointF c2 = p2 - (p3 - p1) / 6.0;
            linePath.cubicTo(c1, c2, p2);
        }

        QPainterPath fillPath = linePath;
        fillPath.lineTo(QPointF(chartX + chartW, chartY + chartH - 2));
        fillPath.lineTo(QPointF(chartX, chartY + chartH - 2));
        fillPath.closeSubpath();
        QLinearGradient grad(QPointF(0, chartY), QPointF(0, chartY + chartH));
        QColor hc = heatColor();
        grad.setColorAt(0.0, QColor(hc.red(), hc.green(), hc.blue(), 140));
        grad.setColorAt(1.0, QColor(hc.red(), hc.green(), hc.blue(), 35));
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawPath(fillPath);

        p.setPen(QPen(heatColor(), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(linePath);

        for (int d = 0; d < 7; ++d) {
            double r = (d == 6) ? 4.0 : 2.0;
            QColor c = (d == 6) ? heatColor() : subTextColor();
            p.setPen(QPen(c, 1));
            p.setBrush(c);
            p.drawEllipse(pts[d], r, r);
        }

        p.setFont(m_fontT10);
        p.setPen(subTextColor());
        p.drawText(QRect(static_cast<int>(pts[6].x()) - 24,
                         static_cast<int>(pts[6].y()) - 18, 48, 14),
                   Qt::AlignCenter, m_cardToday);

        p.setFont(m_fontT10);
        p.setPen(subTextColor());
        int labelY = chartY + chartH + 4;
        QDate today = QDate::currentDate();
        for (int d = 0; d < 7; ++d) {
            QDate date = today.addDays(d - 6);
            QString label = date.toString("MM-dd");
            int x = chartX + d * barW + barW / 2;
            p.drawText(QRect(x - 18, labelY, 36, 12),
                       Qt::AlignCenter, label);
        }
    }

    void DetailPanel::drawEquivalence(QPainter &p, const QRect &area)
    {
        const QString text = WordEquivalence::describe(m_total);
        if (text.isEmpty()) return;

        QFont eqFont = m_fontT8;
        eqFont.setItalic(true);
        p.setFont(eqFont);
        p.setPen(subTextColor());
        p.drawText(QRect(area.x() + 16, area.y() + 8,
                         area.width() - 32, area.height() - 16),
                   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text);
    }

    // ========== 颜色辅助 ==========
    bool DetailPanel::isDarkTheme() const
    {
        return ThemeHelper::isDarkTheme();
    }

    QColor DetailPanel::textColor() const
    {
        return ThemeHelper::textColor();
    }

    QColor DetailPanel::subTextColor() const
    {
        return ThemeHelper::subTextColor();
    }

    QColor DetailPanel::bgColor() const
    {
        return ThemeHelper::bgColor();
    }

    QColor DetailPanel::cardBgColor() const
    {
        return ThemeHelper::cardBgColor();
    }

    QColor DetailPanel::separatorColor() const
    {
        return ThemeHelper::separatorColor();
    }

    QColor DetailPanel::accentColor() const
    {
        return QColor(0x00, 0xA4, 0x8A);
    }

    QColor DetailPanel::heatColor() const
    {
        return accentColor();
    }

    QColor DetailPanel::adjustedTierColor() const
    {
        return ThemeHelper::adjustedColor(m_tierColor);
    }

    // ========== 工具函数 ==========
    QString DetailPanel::formatNumber(qint64 n)
    {
        QString s = QString::number(n);
        int len = s.length();
        for (int i = len - 3; i > 0; i -= 3) {
            s.insert(i, QLatin1Char(','));
        }
        return s;
    }

} // namespace wordcount