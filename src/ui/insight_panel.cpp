// src/ui/insight_panel.cpp
#include "insight_panel.h"
#include "theme_helper.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QDateTime>
#include <QCloseEvent>
#include <QDebug>
#include <cmath>

namespace wordcount {

// ---------- 辅助工具 ----------
static QString formatNumber(qint64 n)
{
    QString s = QString::number(n);
    int len = s.length();
    for (int i = len - 3; i > 0; i -= 3) {
        s.insert(i, QLatin1Char(','));
    }
    return s;
}

// ---------- 今日页 ----------
class InsightPanel::TodayPage : public QWidget {
public:
    explicit TodayPage(QWidget* parent = nullptr) : QWidget(parent) {}

    void setData(const InsightData& data) {
        m_data = data;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        bool dark = ThemeHelper::isDarkTheme();
        QColor textColor = ThemeHelper::textColor();
        QColor subTextColor = ThemeHelper::subTextColor();
        QColor bgColor = ThemeHelper::cardBgColor();

        int w = width(), h = height();
        int margin = 16;
        int cardH = 70;
        int gap = 10;

        // ---- 第一行：今日大字 + 段位缩略 ----
        QRect topRect(margin, margin, w - 2*margin, 60);
        p.fillRect(topRect, bgColor);
        p.setPen(textColor);
        QFont bigFont = p.font();
        bigFont.setPointSize(24);
        bigFont.setBold(true);
        p.setFont(bigFont);
        QString todayStr = formatNumber(m_data.todayChars);
        p.drawText(topRect.adjusted(10, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter, todayStr);

        // 段位缩略（右对齐）
        p.setFont(QFont("", 12, QFont::Bold));
        p.setPen(m_data.tierColor);
        QString tierBrief = m_data.tierName + " · " + m_data.tierRank;
        p.drawText(topRect.adjusted(10, 0, -10, 0), Qt::AlignRight | Qt::AlignVCenter, tierBrief);

        // ---- 第二行：三个小卡片（提交数 / 均速 / 峰速） ----
        int y = topRect.bottom() + gap;
        int cardW = (w - 2*margin - 2*gap) / 3;
        QRect cardRects[3] = {
            QRect(margin, y, cardW, cardH),
            QRect(margin + cardW + gap, y, cardW, cardH),
            QRect(margin + 2*(cardW + gap), y, cardW, cardH)
        };
        QString labels[3] = {"提交", "均速", "峰速"};
        QString values[3] = {
            QString::number(m_data.todayCommits),
            QString::number((int)m_data.avgSpeed) + "/分",
            QString::number(m_data.peakSpeed) + "/分"
        };
        for (int i = 0; i < 3; ++i) {
            p.fillRect(cardRects[i], bgColor);
            p.setPen(subTextColor);
            p.setFont(QFont("", 9));
            p.drawText(cardRects[i].adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, labels[i]);
            p.setPen(textColor);
            p.setFont(QFont("", 14, QFont::Bold));
            p.drawText(cardRects[i].adjusted(8, 6, -8, -6), Qt::AlignBottom | Qt::AlignLeft, values[i]);
        }

        // ---- 第三行：迷你热力图（24小时条） ----
        int barY = cardRects[2].bottom() + gap + 4;
        int barH = 30;
        QRect barArea(margin, barY, w - 2*margin, barH);
        p.fillRect(barArea, bgColor);

        // 从 InsightData 扩展获取小时数据（暂时显示占位）
        p.setPen(subTextColor);
        p.setFont(QFont("", 9));
        p.drawText(barArea, Qt::AlignCenter, "今日时段分布 (开发中)");
    }

private:
    InsightData m_data;
};

// ---------- 效率页 ----------
class InsightPanel::EfficiencyPage : public QWidget {
public:
    explicit EfficiencyPage(QWidget* parent = nullptr) : QWidget(parent) {}

    void setData(const InsightData& data) {
        m_data = data;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        bool dark = ThemeHelper::isDarkTheme();
        QColor textColor = ThemeHelper::textColor();
        QColor subTextColor = ThemeHelper::subTextColor();
        QColor bgColor = ThemeHelper::cardBgColor();
        QColor accent = QColor(0x00, 0xA4, 0x8A);

        int w = width(), h = height();
        int margin = 16;
        int y = margin;

        // ---- 标题 ----
        p.setPen(textColor);
        p.setFont(QFont("", 12, QFont::Bold));
        p.drawText(QRect(margin, y, w - 2*margin, 24), Qt::AlignLeft, "⚡ 字词分布");

        // ---- 条形图 ----
        y += 30;
        int barH = 20;
        int barGap = 6;
        int labelW = 40;
        int maxBarW = w - 2*margin - labelW - 20;

        struct DistItem { QString label; int count; int total; };
        int total = m_data.dist1 + m_data.dist2 + m_data.dist3 + m_data.dist4 + m_data.dist5plus;
        if (total == 0) total = 1;

        DistItem items[5] = {
            {"1字", m_data.dist1, total},
            {"2字", m_data.dist2, total},
            {"3字", m_data.dist3, total},
            {"4字", m_data.dist4, total},
            {"5+字", m_data.dist5plus, total}
        };

        QColor colors[5] = {
            QColor(0x6C, 0x8E, 0xA0), QColor(0x4A, 0x9D, 0x8E),
            QColor(0x7D, 0x4A, 0x9D), QColor(0xC9, 0xA2, 0x27),
            QColor(0xB3, 0x3A, 0x3A)
        };

        for (int i = 0; i < 5; ++i) {
            double ratio = (double)items[i].count / total;
            int barLen = static_cast<int>(ratio * maxBarW);
            if (barLen < 4 && items[i].count > 0) barLen = 4;

            QRect labelRect(margin, y, labelW, barH);
            QRect barRect(margin + labelW, y, barLen, barH);

            p.setPen(subTextColor);
            p.setFont(QFont("", 9));
            p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, items[i].label);

            if (items[i].count > 0) {
                QColor fillColor = colors[i];
                if (dark) fillColor = fillColor.lighter(130);
                p.fillRect(barRect, fillColor);

                // 百分比文字
                QString pct = QString::number((int)(ratio * 100)) + "%";
                p.setPen(textColor);
                p.setFont(QFont("", 8));
                int txtX = barRect.x() + 4;
                if (barLen < 30) txtX = barRect.x() + barLen + 4;
                p.drawText(QRect(txtX, y, 40, barH), Qt::AlignLeft | Qt::AlignVCenter, pct);

                // 次数（右侧）
                QString cnt = QString::number(items[i].count);
                p.setPen(subTextColor);
                p.setFont(QFont("", 8));
                p.drawText(QRect(margin + labelW + maxBarW + 4, y, 50, barH), Qt::AlignLeft | Qt::AlignVCenter, cnt);
            }
            y += barH + barGap;
        }

        // ---- 词组连打率 ----
        y += 10;
        int phraseCommits = total - m_data.dist1;
        double phraseRate = total > 0 ? (double)phraseCommits / total * 100 : 0.0;

        p.setPen(textColor);
        p.setFont(QFont("", 12, QFont::Bold));
        p.drawText(QRect(margin, y, w - 2*margin, 24), Qt::AlignLeft, "⚡ 词组连打率");

        y += 28;
        QFont bigFont = p.font();
        bigFont.setPointSize(22);
        bigFont.setBold(true);
        p.setFont(bigFont);
        p.setPen(accent);
        QString rateStr = QString::number(phraseRate, 'f', 1) + "%";
        p.drawText(QRect(margin, y, 120, 40), Qt::AlignLeft | Qt::AlignVCenter, rateStr);

        p.setPen(subTextColor);
        p.setFont(QFont("", 9));
        p.drawText(QRect(margin + 130, y, w - 2*margin - 130, 40), Qt::AlignLeft | Qt::AlignVCenter,
                   "≥2字提交占比");

        // 平均提交长度
        double avgLen = total > 0 ? (double)(m_data.dist1*1 + m_data.dist2*2 + m_data.dist3*3 + m_data.dist4*4 + m_data.dist5plus*5) / total : 0.0;
        y += 48;
        p.setPen(subTextColor);
        p.setFont(QFont("", 10));
        p.drawText(QRect(margin, y, w - 2*margin, 20), Qt::AlignLeft,
                   QString("平均提交长度: %1 字").arg(avgLen, 0, 'f', 1));
    }

private:
    InsightData m_data;
};

// ---------- 趋势页 ----------
class InsightPanel::TrendPage : public QWidget {
public:
    explicit TrendPage(QWidget* parent = nullptr) : QWidget(parent) {}

    void setData(const InsightData& data) {
        m_data = data;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        bool dark = ThemeHelper::isDarkTheme();
        QColor textColor = ThemeHelper::textColor();
        QColor subTextColor = ThemeHelper::subTextColor();
        QColor bgColor = ThemeHelper::cardBgColor();
        QColor accent = QColor(0x00, 0xA4, 0x8A);

        int w = width(), h = height();
        int margin = 16;
        int chartTop = margin + 30;
        int chartBottom = h - margin - 30;
        int chartLeft = margin + 40;
        int chartRight = w - margin - 10;
        int chartW = chartRight - chartLeft;
        int chartH = chartBottom - chartTop;

        // ---- 标题 ----
        p.setPen(textColor);
        p.setFont(QFont("", 12, QFont::Bold));
        p.drawText(QRect(margin, margin, w - 2*margin, 24), Qt::AlignLeft, "📈 近7日字数");

        // ---- 数据准备 ----
        qint64 maxVal = 0;
        for (int i = 0; i < 7; ++i) {
            if (m_data.daily7[i] > maxVal) maxVal = m_data.daily7[i];
        }
        if (maxVal == 0) maxVal = 1;
        qint64 minVal = maxVal;
        for (int i = 0; i < 7; ++i) {
            if (m_data.daily7[i] < minVal) minVal = m_data.daily7[i];
        }
        if (maxVal == minVal) minVal = 0;

        QPointF points[7];
        for (int i = 0; i < 7; ++i) {
            double x = chartLeft + (i / 6.0) * chartW;
            double y = chartBottom - (m_data.daily7[i] - minVal) / (double)(maxVal - minVal + 1) * chartH;
            points[i] = QPointF(x, y);
        }

        // ---- 绘制网格 ----
        p.setPen(QPen(subTextColor, 1, Qt::DotLine));
        for (int i = 0; i <= 4; ++i) {
            double y = chartTop + i * chartH / 4.0;
            p.drawLine(chartLeft, y, chartRight, y);
        }

        // ---- Y轴标签 ----
        p.setPen(subTextColor);
        p.setFont(QFont("", 8));
        for (int i = 0; i <= 4; ++i) {
            double y = chartTop + i * chartH / 4.0;
            qint64 val = minVal + (maxVal - minVal) * (4 - i) / 4;
            p.drawText(QRect(margin, y - 8, chartLeft - margin, 16), Qt::AlignRight | Qt::AlignVCenter,
                       formatNumber(val));
        }

        // ---- X轴标签（日期） ----
        QDate today = QDate::currentDate();
        for (int i = 0; i < 7; ++i) {
            QDate d = today.addDays(i - 6);
            QString label = d.toString("MM-dd");
            double x = chartLeft + (i / 6.0) * chartW;
            p.drawText(QRect(x - 20, chartBottom + 4, 40, 16), Qt::AlignHCenter, label);
        }

        // ---- 折线填充区域 ----
        QPainterPath fillPath;
        fillPath.moveTo(points[0]);
        for (int i = 1; i < 7; ++i) {
            fillPath.lineTo(points[i]);
        }
        fillPath.lineTo(QPointF(points[6].x(), chartBottom));
        fillPath.lineTo(QPointF(points[0].x(), chartBottom));
        fillPath.closeSubpath();

        QColor fillColor = accent;
        fillColor.setAlpha(40);
        p.fillPath(fillPath, fillColor);

        // ---- 折线 ----
        QPainterPath linePath;
        linePath.moveTo(points[0]);
        for (int i = 1; i < 7; ++i) {
            linePath.lineTo(points[i]);
        }
        p.setPen(QPen(accent, 2));
        p.drawPath(linePath);

        // ---- 数据点 ----
        for (int i = 0; i < 7; ++i) {
            double r = (i == 6) ? 4.0 : 2.0;
            QColor c = (i == 6) ? accent : subTextColor;
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            p.drawEllipse(points[i], r, r);
        }

        // ---- 今日数字标注 ----
        if (m_data.daily7[6] > 0) {
            p.setPen(accent);
            p.setFont(QFont("", 9, QFont::Bold));
            QString todayStr = formatNumber(m_data.daily7[6]);
            p.drawText(QRect(points[6].x() - 30, points[6].y() - 24, 60, 18), Qt::AlignCenter, todayStr);
        }
    }

private:
    InsightData m_data;
};

// ---------- 段位页 ----------
class InsightPanel::LevelPage : public QWidget {
public:
    explicit LevelPage(QWidget* parent = nullptr) : QWidget(parent) {}

    void setData(const InsightData& data) {
        m_data = data;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        bool dark = ThemeHelper::isDarkTheme();
        QColor textColor = ThemeHelper::textColor();
        QColor subTextColor = ThemeHelper::subTextColor();
        QColor bgColor = ThemeHelper::cardBgColor();

        int w = width(), h = height();
        int margin = 16;
        int y = margin;

        // ---- 段位徽章（大号） ----
        QRect badgeRect(margin, y, w - 2*margin, 80);
        QColor tierBg = m_data.tierColor;
        tierBg.setAlpha(dark ? 80 : 40);
        p.fillRect(badgeRect, tierBg);

        p.setPen(m_data.tierColor);
        p.setFont(QFont("", 28, QFont::Bold));
        p.drawText(badgeRect.adjusted(16, 0, -16, 0), Qt::AlignLeft | Qt::AlignVCenter, m_data.tierName);

        p.setPen(subTextColor);
        p.setFont(QFont("", 12));
        QString rankLine = m_data.tierRank + " · " + m_data.tierSubtitle;
        p.drawText(badgeRect.adjusted(16, 0, -16, 0), Qt::AlignRight | Qt::AlignVCenter, rankLine);

        // ---- 总字数 ----
        y = badgeRect.bottom() + 12;
        p.setPen(textColor);
        p.setFont(QFont("", 12, QFont::Bold));
        p.drawText(QRect(margin, y, w - 2*margin, 24), Qt::AlignLeft, "生涯总字数");
        p.setFont(QFont("", 20, QFont::Bold));
        p.drawText(QRect(margin, y + 26, w - 2*margin, 32), Qt::AlignLeft, formatNumber(m_data.totalChars));

        // ---- 进度条 ----
        y += 70;
        int barX = margin, barW = w - 2*margin, barH = 14;
        QColor trackColor = m_data.tierColor;
        trackColor.setAlpha(70);
        p.fillRect(QRect(barX, y, barW, barH), trackColor);
        int filled = static_cast<int>(barW * m_data.progress);
        if (filled > 0) {
            QRect fillRect(barX, y, filled, barH);
            p.fillRect(fillRect, m_data.tierColor);
        }

        // 进度文字
        y += barH + 6;
        p.setPen(subTextColor);
        p.setFont(QFont("", 9));
        QString progText;
        if (m_data.progress >= 1.0 && m_data.nextThreshold == m_data.totalChars) {
            progText = "已达巅峰";
        } else {
            qint64 remain = m_data.nextThreshold - m_data.totalChars;
            if (remain < 0) remain = 0;
            progText = QString("进度 %1% · 距下一级还差 %2 字")
                        .arg((int)(m_data.progress * 100))
                        .arg(formatNumber(remain));
        }
        p.drawText(QRect(margin, y, w - 2*margin, 18), Qt::AlignCenter, progText);

        // ---- 里程碑（段位列表） ----
        y += 30;
        p.setPen(subTextColor);
        p.setFont(QFont("", 10));
        p.drawText(QRect(margin, y, w - 2*margin, 20), Qt::AlignLeft, "段位里程碑");

        y += 24;
        QStringList allTiers = {"蒙童", "布衣", "青衿", "文士", "墨客", "雅士", "鸿儒", "文宗", "文圣"};
        int currentIdx = -1;
        for (int i = 0; i < allTiers.size(); ++i) {
            if (allTiers[i] == m_data.tierName) {
                currentIdx = i;
                break;
            }
        }
        if (currentIdx < 0) currentIdx = 0;

        int dotR = 6;
        int gap = (w - 2*margin - 2*dotR) / (allTiers.size() - 1);
        int dotY = y + dotR;
        for (int i = 0; i < allTiers.size(); ++i) {
            int x = margin + dotR + i * gap;
            QColor dotColor = (i <= currentIdx) ? m_data.tierColor : subTextColor;
            dotColor.setAlpha(i == currentIdx ? 255 : 80);
            p.setBrush(dotColor);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPoint(x, dotY), dotR, dotR);

            p.setPen(i == currentIdx ? textColor : subTextColor);
            p.setFont(QFont("", 8, i == currentIdx ? QFont::Bold : QFont::Normal));
            p.drawText(QRect(x - 16, dotY + dotR + 4, 32, 14), Qt::AlignHCenter, allTiers[i]);
        }
    }

private:
    InsightData m_data;
};

// ---------- InsightPanel 实现 ----------
InsightPanel::InsightPanel(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(tr("📊 统计仪表盘"));
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(440, 540);
    resize(440, 540);
    setupUI();
}

void InsightPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(false);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setTabShape(QTabWidget::Rounded);

    // 创建各页面
    m_todayPage = new TodayPage(this);
    m_efficiencyPage = new EfficiencyPage(this);
    m_trendPage = new TrendPage(this);
    m_levelPage = new LevelPage(this);

    m_tabWidget->addTab(m_levelPage, "🏆 段位");
    m_tabWidget->addTab(m_todayPage, "📊 今日");
    m_tabWidget->addTab(m_efficiencyPage, "⚡ 效率");
    m_tabWidget->addTab(m_trendPage, "📈 趋势");
    

    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
}

void InsightPanel::refresh(const InsightData& data)
{
    m_data = data;
    m_todayPage->setData(data);
    m_efficiencyPage->setData(data);
    m_trendPage->setData(data);
    m_levelPage->setData(data);
}

void InsightPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

void InsightPanel::closeEvent(QCloseEvent* event)
{
    // 只是隐藏窗口，不销毁
    hide();
    event->accept();
}

} // namespace wordcount