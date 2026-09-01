#include "detail_panel.h"
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
#include <QPalette>
#include <cmath>

namespace wordcount {

    // ---------- 内部布局常量 ----------
    namespace {
        constexpr int kHeroHeight = 100;
        constexpr int kCardsHeight = 82;
        constexpr int kDaily7Height = 104;
        constexpr int kHourlyHeight = 78;
        constexpr int kEquivalenceHeight = 56;
        constexpr double kPi = 3.14159265358979323846;

        QColor adjustedColor(const QColor &c, bool dark) {
            return dark ? c.lighter(140) : c.lighter(108);
        }
    }

    // ---------- 构造 ----------
    DetailPanel::DetailPanel(QWidget *parent)
    : QWidget(parent)
    {
        setFixedSize(kPanelWidth, kPanelHeight);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setFocusPolicy(Qt::NoFocus);

        // 获取系统字体缩放因子
        qreal dpr = devicePixelRatioF();
        int basePointSize = 10;

        // 使用点大小（Point Size）替代像素大小，自动适配 DPI
        m_fontToday   = QFont("", basePointSize + 4, QFont::Bold);   // 14pt
        m_fontT4      = QFont("", basePointSize + 2, QFont::Bold);   // 12pt
        m_fontT5      = QFont("", basePointSize + 1, QFont::Bold);   // 11pt
        m_fontT7      = QFont("", basePointSize - 1);                // 9pt
        m_fontT8      = QFont("", basePointSize - 2);                // 8pt
        m_fontT10     = QFont("", basePointSize - 3);                // 7pt

        // 设置字体策略为优先使用点大小
        m_fontToday.setStyleStrategy(QFont::PreferMatch);
        m_fontT4.setStyleStrategy(QFont::PreferMatch);
        m_fontT5.setStyleStrategy(QFont::PreferMatch);
        m_fontT7.setStyleStrategy(QFont::PreferMatch);
        m_fontT8.setStyleStrategy(QFont::PreferMatch);
        m_fontT10.setStyleStrategy(QFont::PreferMatch);

        // 重新创建 QFontMetrics
        m_fmToday = QFontMetrics(m_fontToday);
        m_fmT4 = QFontMetrics(m_fontT4);
        m_fmT5 = QFontMetrics(m_fontT5);
        m_fmT7 = QFontMetrics(m_fontT7);
        m_fmT8 = QFontMetrics(m_fontT8);
        m_fmT10 = QFontMetrics(m_fontT10);

        connect(&ThemeHelper::instance(), &ThemeHelper::themeChanged,
                this, QOverload<>::of(&QWidget::update));
    }

    // ---------- 主题颜色辅助 ----------
    bool DetailPanel::isDarkTheme() const
    {
        return wordcount::ThemeHelper::isDarkTheme();
    }

    QColor DetailPanel::textColor() const
    {
        return wordcount::ThemeHelper::textColor();
    }

    QColor DetailPanel::subTextColor() const
    {
        return wordcount::ThemeHelper::subTextColor();
    }

    QColor DetailPanel::bgColor() const
    {
        return wordcount::ThemeHelper::bgColor();
    }

    QColor DetailPanel::cardBgColor() const
    {
        return wordcount::ThemeHelper::cardBgColor();
    }

    QColor DetailPanel::separatorColor() const
    {
        return wordcount::ThemeHelper::separatorColor();
    }

    QColor DetailPanel::accentColor() const
    {
        // 品牌色/强调色 - 翠绿
        return QColor(0x00, 0xA4, 0x8A);
    }

    QColor DetailPanel::heatColor() const
    {
        // 热力图颜色使用品牌色
        return accentColor();
    }

    QColor DetailPanel::adjustedTierColor() const
    {
        return wordcount::ThemeHelper::adjustedColor(m_tierColor);
    }

    // ---------- showEvent ----------
    void DetailPanel::showEvent(QShowEvent *event)
    {
        QWidget::showEvent(event);
    }

    // ---------- 刷新数据 ----------
    void DetailPanel::refresh(qint64 total, qint64 today, double wpm,
                              const QJsonArray &hourly, const QJsonArray &daily7)
    {
        m_total = total;
        m_today = today;
        m_wpm = wpm;
        m_hourly = hourly;
        m_daily7 = daily7;

        LevelInfo lv = LevelSystem::levelForTotal(total);
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
            const qint64 remain = qMax<qint64>(0, lv.nextMin - total);
            m_progressLine = QStringLiteral("%1% · 距%2还差%3字")
            .arg(static_cast<int>(m_levelProgress * 100))
            .arg(nextRank)
            .arg(formatNumber(remain));
        }

        m_cardToday = formatNumber(m_today);
        m_cardSpeed = QStringLiteral("%1 字/分").arg(static_cast<int>(m_wpm));
        m_cardTotal = formatNumber(m_total);

        qint64 yesterday = 0;
        if (daily7.size() >= 6) {
            yesterday = daily7.at(5).toVariant().toLongLong();
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

        qint64 base = 0, last = 0;
        if (daily7.size() >= 7) {
            base = daily7.at(0).toVariant().toLongLong();
            last = daily7.at(6).toVariant().toLongLong();
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

        m_peakLine.clear();
        m_peakIsNewRecord = false;
        m_rhythmTitle.clear();
        m_rhythmSubtitle.clear();
        m_rhythmProgress = 0.0;

        update();
    }

    // ---------- paintEvent ----------
    void DetailPanel::paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setPen(Qt::NoPen);
        p.setBrush(bgColor());
        p.drawRoundedRect(rect(), 12, 12);

        p.setPen(QPen(separatorColor(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

        int y = 0;
        QRect heroArea(0, y, kPanelWidth, kHeroHeight); y += kHeroHeight;
        QRect cardsArea(0, y, kPanelWidth, kCardsHeight); y += kCardsHeight;
        QRect daily7Area(0, y, kPanelWidth, kDaily7Height); y += kDaily7Height;
        QRect hourlyArea(0, y, kPanelWidth, kHourlyHeight); y += kHourlyHeight;
        QRect equivalenceArea(0, y, kPanelWidth, kEquivalenceHeight);

        p.setPen(QPen(separatorColor(), 1));
        p.drawLine(0, heroArea.bottom(), kPanelWidth, heroArea.bottom());

        drawHero(p, heroArea);
        drawNumberCards(p, cardsArea);
        drawDaily7(p, daily7Area);
        drawHourlyHeatmap(p, hourlyArea);
        drawEquivalence(p, equivalenceArea);
    }

    // ---------- ★ Hero 区绘制 ----------
    void DetailPanel::drawHero(QPainter &p, const QRect &area)
    {
        // ---- 右侧今日数据区域（垂直布局） ----
        const int rightW = 110;
        const int rightX = area.x() + area.width() - 16 - rightW;
        const QRect rightRect(rightX, area.y() + 8, rightW, area.height() - 16);

        // 第一行：“今日”标签（顶部）
        const int row1Y = rightRect.y();
        const int row1H = 16;
        p.setFont(m_fontT10);
        p.setPen(subTextColor());
        p.drawText(QRect(rightRect.x(), row1Y, rightRect.width(), row1H),
                   Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("今日已输入"));

        // 第二行：今日数字（居中，大字体）
        const int row2Y = row1Y + row1H + 2;
        const int row2H = 30;
        QFont numFont = m_fontToday;
        int numWidth = QFontMetrics(numFont).horizontalAdvance(m_cardToday);
        // if (numWidth > rightRect.width()) {
        //     numFont = QFont("", 16, QFont::Bold);
        //     numWidth = QFontMetrics(numFont).horizontalAdvance(m_cardToday);
        // }
        p.setFont(numFont);
        p.setPen(textColor());
        p.drawText(QRect(rightRect.x(), row2Y, rightRect.width(), row2H),
                   Qt::AlignRight | Qt::AlignVCenter, m_cardToday);

        // // 第三行：差值徽章（底部）
        // const int row3Y = row2Y + row2H + 2;
        // const int row3H = 18;
        // p.setFont(m_fontT10);
        // p.setPen(m_badgeColor);
        // p.drawText(QRect(rightRect.x(), row3Y, rightRect.width(), row3H),
        //            Qt::AlignRight | Qt::AlignVCenter, m_badgeText);

        // ---- 左侧等级信息  ----
        const int leftW = rightX - area.x() - 24;

        p.setFont(m_fontT4);
        p.setPen(adjustedTierColor());
        const QString tierLine = m_fmT4.horizontalAdvance(m_tierLine) > leftW ?
        m_fmT4.elidedText(m_tierLine, Qt::ElideRight, leftW) :
        m_tierLine;
        p.drawText(QRect(area.x() + 16, area.y() + 12, leftW, 26),
                   Qt::AlignLeft | Qt::AlignVCenter, tierLine);

        p.setFont(m_fontT8);
        p.setPen(subTextColor());
        p.drawText(QRect(area.x() + 16, area.y() + 36, leftW, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, m_tierSubtitle);

        // ---- 进度条 ----
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

        // 进度文案（在进度条下方）
        p.setFont(m_fontT8);
        p.setPen(subTextColor());
        const QString progressLine = m_fmT8.horizontalAdvance(m_progressLine) > barW ?
        m_fmT8.elidedText(m_progressLine, Qt::ElideRight, barW) :
        m_progressLine;
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

    // ---------- 工具函数 ----------
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
