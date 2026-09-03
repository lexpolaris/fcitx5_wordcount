// src/ui/insight_panel.cpp
// SPDX-License-Identifier: LGPL-2.1-or-later
// 统计仪表盘实现

#include "insight_panel.h"
#include "engine/statistics_engine.h"
#include "core/level_system.h"
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

    // ========== 辅助工具 ==========
    static QString formatNumber(qint64 n)
    {
        QString s = QString::number(n);
        int len = s.length();
        for (int i = len - 3; i > 0; i -= 3) {
            s.insert(i, QLatin1Char(','));
        }
        return s;
    }

    // ========== 静态实例管理 ==========
    QPointer<InsightPanel> InsightPanel::s_instance;

    InsightPanel* InsightPanel::instance()
    {
        return s_instance;
    }

    bool InsightPanel::isVisible()
    {
        return s_instance && s_instance->QWidget::isVisible();
    }

    void InsightPanel::showPanel()
    {
        if (!s_instance) return;
        if (s_instance->isMinimized()) {
            s_instance->showNormal();
        } else {
            s_instance->show();
        }
        s_instance->raise();
        s_instance->activateWindow();
    }

    void InsightPanel::togglePanel()
    {
        if (!s_instance) return;
        if (s_instance->isVisible()) {
            s_instance->hide();
        } else {
            showPanel();
        }
    }

    void InsightPanel::hidePanel()
    {
        if (s_instance) s_instance->hide();
    }

    // ============================================================
    // 内部页面类定义（必须在 setupUI 之前完整定义）
    // ============================================================

    // ---------- 今日页 ----------
    class InsightPanel::TodayPage : public QWidget {
    public:
        explicit TodayPage(InsightPanel* parent) : QWidget(parent), m_panel(parent) {}

    protected:
        void paintEvent(QPaintEvent*) override {
            if (!m_panel) return;
            const auto& d = m_panel->m_cache;

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            QColor textColor = ThemeHelper::textColor();
            QColor subTextColor = ThemeHelper::subTextColor();
            QColor bgColor = ThemeHelper::cardBgColor();

            int w = width(), h = height();
            int margin = 16;
            int cardH = 70;
            int gap = 10;

            // 第一行：今日大字
            QRect topRect(margin, margin, w - 2*margin, 60);
            p.fillRect(topRect, bgColor);
            p.setPen(textColor);
            QFont bigFont = p.font();
            bigFont.setPointSize(24);
            bigFont.setBold(true);
            p.setFont(bigFont);
            QString todayStr = formatNumber(d.todayChars);
            p.drawText(topRect.adjusted(10, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter, todayStr);

            // 第二行：三个小卡片
            int y = topRect.bottom() + gap;
            int cardW = (w - 2*margin - 2*gap) / 3;
            QRect cardRects[3] = {
                QRect(margin, y, cardW, cardH),
                QRect(margin + cardW + gap, y, cardW, cardH),
                QRect(margin + 2*(cardW + gap), y, cardW, cardH)
            };
            QString labels[3] = {"提交", "均速", "峰速"};
            QString values[3] = {
                QString::number(d.todayCommits),
                QString::number((int)d.avgSpeed) + "/分",
                QString::number(d.peakSpeed) + "/分"
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
        }

    private:
        InsightPanel* m_panel = nullptr;
    };

    // ---------- 效率页 ----------
    class InsightPanel::EfficiencyPage : public QWidget {
    public:
        explicit EfficiencyPage(InsightPanel* parent) : QWidget(parent), m_panel(parent) {}

    protected:
        void paintEvent(QPaintEvent*) override {
            if (!m_panel) return;
            const auto& d = m_panel->m_cache;

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            QColor textColor = ThemeHelper::textColor();
            QColor subTextColor = ThemeHelper::subTextColor();
            QColor bgColor = ThemeHelper::cardBgColor();
            QColor accent = QColor(0x00, 0xA4, 0x8A);

            int w = width(), h = height();
            int margin = 16;
            int y = margin;

            // 标题
            p.setPen(textColor);
            p.setFont(QFont("", 12, QFont::Bold));
            p.drawText(QRect(margin, y, w - 2*margin, 24), Qt::AlignLeft, "⚡ 字词分布");

            // 条形图
            y += 30;
            int barH = 20;
            int barGap = 6;
            int labelW = 40;
            int maxBarW = w - 2*margin - labelW - 20;

            struct DistItem { QString label; int count; int total; };
            int total = d.dist1 + d.dist2 + d.dist3 + d.dist4 + d.dist5plus;
            if (total == 0) total = 1;

            DistItem items[5] = {
                {"1字", d.dist1, total},
                {"2字", d.dist2, total},
                {"3字", d.dist3, total},
                {"4字", d.dist4, total},
                {"5+字", d.dist5plus, total}
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
                    if (ThemeHelper::isDarkTheme()) fillColor = fillColor.lighter(130);
                    p.fillRect(barRect, fillColor);

                    QString pct = QString::number((int)(ratio * 100)) + "%";
                    p.setPen(textColor);
                    p.setFont(QFont("", 8));
                    int txtX = barRect.x() + 4;
                    if (barLen < 30) txtX = barRect.x() + barLen + 4;
                    p.drawText(QRect(txtX, y, 40, barH), Qt::AlignLeft | Qt::AlignVCenter, pct);

                    QString cnt = QString::number(items[i].count);
                    p.setPen(subTextColor);
                    p.setFont(QFont("", 8));
                    p.drawText(QRect(margin + labelW + maxBarW + 4, y, 50, barH), Qt::AlignLeft | Qt::AlignVCenter, cnt);
                }
                y += barH + barGap;
            }

            // 词组连打率
            y += 10;
            int phraseCommits = total - d.dist1;
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

            double avgLen = total > 0 ? (double)(d.dist1*1 + d.dist2*2 + d.dist3*3 + d.dist4*4 + d.dist5plus*5) / total : 0.0;
            y += 48;
            p.setPen(subTextColor);
            p.setFont(QFont("", 10));
            p.drawText(QRect(margin, y, w - 2*margin, 20), Qt::AlignLeft,
                       QString("平均提交长度: %1 字").arg(avgLen, 0, 'f', 1));
        }

    private:
        InsightPanel* m_panel = nullptr;
    };

    // ---------- 趋势页 ----------
    class InsightPanel::TrendPage : public QWidget {
    public:
        explicit TrendPage(InsightPanel* parent) : QWidget(parent), m_panel(parent) {}

    protected:
        void paintEvent(QPaintEvent*) override {
            if (!m_panel) return;
            const auto& d = m_panel->m_cache;

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            QColor textColor = ThemeHelper::textColor();
            QColor subTextColor = ThemeHelper::subTextColor();
            QColor accent = QColor(0x00, 0xA4, 0x8A);

            int w = width(), h = height();
            int margin = 16;
            int chartTop = margin + 30;
            int chartBottom = h - margin - 30;
            int chartLeft = margin + 40;
            int chartRight = w - margin - 10;
            int chartW = chartRight - chartLeft;
            int chartH = chartBottom - chartTop;

            // 标题
            p.setPen(textColor);
            p.setFont(QFont("", 12, QFont::Bold));
            p.drawText(QRect(margin, margin, w - 2*margin, 24), Qt::AlignLeft, "📈 近7日字数");

            // 数据准备
            qint64 maxVal = 0;
            for (int i = 0; i < 7; ++i) {
                if (d.daily7[i] > maxVal) maxVal = d.daily7[i];
            }
            if (maxVal == 0) maxVal = 1;
            qint64 minVal = maxVal;
            for (int i = 0; i < 7; ++i) {
                if (d.daily7[i] < minVal) minVal = d.daily7[i];
            }
            if (maxVal == minVal) minVal = 0;

            QPointF points[7];
            for (int i = 0; i < 7; ++i) {
                double x = chartLeft + (i / 6.0) * chartW;
                double y = chartBottom - (d.daily7[i] - minVal) / (double)(maxVal - minVal + 1) * chartH;
                points[i] = QPointF(x, y);
            }

            // 网格
            p.setPen(QPen(subTextColor, 1, Qt::DotLine));
            for (int i = 0; i <= 4; ++i) {
                double y = chartTop + i * chartH / 4.0;
                p.drawLine(chartLeft, y, chartRight, y);
            }

            // Y轴标签
            p.setPen(subTextColor);
            p.setFont(QFont("", 8));
            for (int i = 0; i <= 4; ++i) {
                double y = chartTop + i * chartH / 4.0;
                qint64 val = minVal + (maxVal - minVal) * (4 - i) / 4;
                p.drawText(QRect(margin, y - 8, chartLeft - margin, 16), Qt::AlignRight | Qt::AlignVCenter,
                           formatNumber(val));
            }

            // X轴标签
            QDate today = QDate::currentDate();
            for (int i = 0; i < 7; ++i) {
                QDate d = today.addDays(i - 6);
                QString label = d.toString("MM-dd");
                double x = chartLeft + (i / 6.0) * chartW;
                p.drawText(QRect(x - 20, chartBottom + 4, 40, 16), Qt::AlignHCenter, label);
            }

            // 填充区域
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

            // 折线
            QPainterPath linePath;
            linePath.moveTo(points[0]);
            for (int i = 1; i < 7; ++i) {
                linePath.lineTo(points[i]);
            }
            p.setPen(QPen(accent, 2));
            p.drawPath(linePath);

            // 数据点
            for (int i = 0; i < 7; ++i) {
                double r = (i == 6) ? 4.0 : 2.0;
                QColor c = (i == 6) ? accent : subTextColor;
                p.setBrush(c);
                p.setPen(Qt::NoPen);
                p.drawEllipse(points[i], r, r);
            }

            // 今日数字标注
            if (d.daily7[6] > 0) {
                p.setPen(accent);
                p.setFont(QFont("", 9, QFont::Bold));
                QString todayStr = formatNumber(d.daily7[6]);
                p.drawText(QRect(points[6].x() - 30, points[6].y() - 24, 60, 18), Qt::AlignCenter, todayStr);
            }
        }

    private:
        InsightPanel* m_panel = nullptr;
    };

    // ---------- 段位页 ----------
    class InsightPanel::LevelPage : public QWidget {
    public:
        explicit LevelPage(InsightPanel* parent) : QWidget(parent), m_panel(parent) {}

    protected:
        void paintEvent(QPaintEvent*) override {
            if (!m_panel) return;
            const auto& d = m_panel->m_cache;

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            QColor textColor = ThemeHelper::textColor();
            QColor subTextColor = ThemeHelper::subTextColor();
            QColor bgColor = ThemeHelper::cardBgColor();
            bool dark = ThemeHelper::isDarkTheme();

            int w = width(), h = height();
            int margin = 16;
            int y = margin;

            // 段位徽章
            QRect badgeRect(margin, y, w - 2*margin, 80);
            QColor tierBg = d.tierColor;
            tierBg.setAlpha(dark ? 80 : 40);
            p.fillRect(badgeRect, tierBg);

            p.setPen(d.tierColor);
            p.setFont(QFont("", 28, QFont::Bold));
            p.drawText(badgeRect.adjusted(16, 0, -16, 0), Qt::AlignLeft | Qt::AlignVCenter, d.tierName);

            p.setPen(subTextColor);
            p.setFont(QFont("", 12));
            QString rankLine = d.tierRank + " · " + d.tierSubtitle;
            p.drawText(badgeRect.adjusted(16, 0, -16, 0), Qt::AlignRight | Qt::AlignVCenter, rankLine);

            // 总字数
            y = badgeRect.bottom() + 12;
            p.setPen(textColor);
            p.setFont(QFont("", 12, QFont::Bold));
            p.drawText(QRect(margin, y, w - 2*margin, 24), Qt::AlignLeft, "生涯总字数");
            p.setFont(QFont("", 20, QFont::Bold));
            p.drawText(QRect(margin, y + 26, w - 2*margin, 32), Qt::AlignLeft, formatNumber(d.totalChars));

            // 进度条
            y += 70;
            int barX = margin, barW = w - 2*margin, barH = 14;
            QColor trackColor = d.tierColor;
            trackColor.setAlpha(70);
            p.fillRect(QRect(barX, y, barW, barH), trackColor);
            int filled = static_cast<int>(barW * d.progress);
            if (filled > 0) {
                QRect fillRect(barX, y, filled, barH);
                p.fillRect(fillRect, d.tierColor);
            }

            y += barH + 6;
            p.setPen(subTextColor);
            p.setFont(QFont("", 9));
            QString progText;
            if (d.progress >= 1.0 && d.nextThreshold == d.totalChars) {
                // 动态获取顶级段位名称
                const TierDef& topTier = LevelSystem::tierAt(LevelSystem::kTierCount - 1);
                progText = QString("🏆 %1").arg(QString::fromUtf8(topTier.tierName));
            } else {
                qint64 remain = d.nextThreshold - d.totalChars;
                if (remain < 0) remain = 0;
                progText = QString("进度 %1% · 距下一级还差 %2 字")
                            .arg((int)(d.progress * 100))
                            .arg(formatNumber(remain));
            }
            p.drawText(QRect(margin, y, w - 2*margin, 18), Qt::AlignCenter, progText);

            // 段位里程碑
            y += 30;
            p.setPen(subTextColor);
            p.setFont(QFont("", 10));
            p.drawText(QRect(margin, y, w - 2*margin, 20), Qt::AlignLeft, "段位里程碑");

            y += 24;

            // ★ 动态获取当前方案的所有段位名称
            const auto& scheme = LevelSystem::currentScheme();
            QStringList allTiers;
            for (int i = 0; i < LevelSystem::kTierCount; ++i) {
                allTiers.append(QString::fromUtf8(scheme.tiers[i].tierName));
            }

            int currentIdx = allTiers.indexOf(d.tierName);
            if (currentIdx < 0) currentIdx = 0;

            int dotR = 6;
            int gap = (w - 2*margin - 2*dotR) / (allTiers.size() - 1);
            int dotY = y + dotR;
            for (int i = 0; i < allTiers.size(); ++i) {
                int x = margin + dotR + i * gap;
                QColor dotColor = (i <= currentIdx) ? d.tierColor : subTextColor;
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
        InsightPanel* m_panel = nullptr;
    };

    // ============================================================
    // InsightPanel 成员函数实现
    // ============================================================

    // ---------- 构造 / 析构 ----------
    InsightPanel::InsightPanel(StatisticsEngine* engine, QWidget* parent)
        : QWidget(parent)
        , m_engine(engine)
    {
        setWindowTitle(tr("📊 统计仪表盘"));
        setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
        setMinimumSize(440, 540);
        resize(440, 540);

        setupUI();

        // ★ 订阅引擎数据更新
        if (m_engine) {
            connect(m_engine, &StatisticsEngine::statsChanged,
                    this, &InsightPanel::onStatsChanged);
            // 首次加载
            updateData();
        }

        s_instance = this;
    }

    InsightPanel::~InsightPanel()
    {
        if (s_instance == this) s_instance = nullptr;
    }

    // ---------- UI 构建 ----------
    void InsightPanel::setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setDocumentMode(false);
        m_tabWidget->setTabPosition(QTabWidget::North);
        m_tabWidget->setTabShape(QTabWidget::Rounded);

        // 创建各页面（传入 this 作为 parent）
        m_levelPage = new LevelPage(this);
        m_todayPage = new TodayPage(this);
        m_efficiencyPage = new EfficiencyPage(this);
        m_trendPage = new TrendPage(this);

        m_tabWidget->addTab(m_levelPage, "🏆 段位");
        m_tabWidget->addTab(m_todayPage, "📊 今日");
        m_tabWidget->addTab(m_efficiencyPage, "⚡ 效率");
        m_tabWidget->addTab(m_trendPage, "📈 趋势");

        mainLayout->addWidget(m_tabWidget);
        setLayout(mainLayout);
    }

    // ---------- 数据更新 ----------
    void InsightPanel::onStatsChanged(qint64 total, qint64 today, double wpm)
    {
        Q_UNUSED(total);
        Q_UNUSED(today);
        Q_UNUSED(wpm);
        updateData();
        // 刷新所有页面
        if (m_todayPage) m_todayPage->update();
        if (m_efficiencyPage) m_efficiencyPage->update();
        if (m_trendPage) m_trendPage->update();
        if (m_levelPage) m_levelPage->update();
    }

    void InsightPanel::updateData()
    {
        if (!m_engine) return;

        // 今日数据
        m_cache.todayChars = m_engine->todayChars();
        m_cache.todayCommits = m_engine->todayCommits();
        m_cache.avgSpeed = m_engine->currentWpm();
        m_cache.peakSpeed = m_engine->peakSpeed();

        // 字词分布
        auto dist = m_engine->distToday();
        m_cache.dist1 = dist[0];
        m_cache.dist2 = dist[1];
        m_cache.dist3 = dist[2];
        m_cache.dist4 = dist[3];
        m_cache.dist5plus = dist[4];

        // 段位信息
        auto levelInfo = LevelSystem::levelForTotal(m_engine->totalChars());
        m_cache.tierName = levelInfo.tierName;
        m_cache.tierRank = levelInfo.tierRank;
        m_cache.tierSubtitle = QString::fromUtf8(
            LevelSystem::tierAt(levelInfo.tierIndex).fullName);
        m_cache.tierColor = levelInfo.tierColor;
        m_cache.progress = levelInfo.progress;
        m_cache.nextThreshold = levelInfo.nextMin;
        m_cache.totalChars = m_engine->totalChars();

        // 近7天
        QJsonArray daily7 = m_engine->daily7();
        for (int i = 0; i < 7 && i < daily7.size(); ++i) {
            m_cache.daily7[i] = daily7.at(i).toVariant().toLongLong();
        }
    }

    // ---------- 窗口事件 ----------
    void InsightPanel::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        // 显示时刷新数据
        if (m_engine) updateData();
    }

    void InsightPanel::closeEvent(QCloseEvent* event)
    {
        // 只是隐藏窗口，不销毁
        hide();
        event->accept();
    }

} // namespace wordcount