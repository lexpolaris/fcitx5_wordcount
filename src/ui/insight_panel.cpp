// src/ui/insight_panel.cpp
// SPDX-License-Identifier: LGPL-2.1-or-later
// 统计仪表盘实现

#include "insight_panel.h"
#include "engine/statistics_engine.h"
#include "core/level_system.h"
#include "theme_helper.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QDateTime>
#include <QCloseEvent>
#include <QDebug>
#include <cmath>
#include <QMouseEvent>
#include <QDate>
#include <QDialog>
#include <QDateEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMap>
#include <QMouseEvent>


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

            // 动态获取当前方案的所有段位名称
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

    // ---------- 分析页 ----------
    class InsightPanel::AnalysisPage : public QWidget {
    public:
        explicit AnalysisPage(InsightPanel* parent) : QWidget(parent), m_panel(parent) {
            setMouseTracking(true);
            updateRangeDates();
        }

    protected:
        void paintEvent(QPaintEvent*) override {
            if (!m_panel || !m_panel->m_engine) {
                QPainter p(this);
                p.fillRect(rect(), ThemeHelper::bgColor());
                return;
            }
            const auto& d = m_panel->m_cache;

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            QColor textColor = ThemeHelper::textColor();
            QColor subTextColor = ThemeHelper::subTextColor();
            QColor bgColor = ThemeHelper::cardBgColor();
            QColor accent = QColor(0x00, 0xA4, 0x8A);

            int w = width(), h = height();
            int margin = 16;
            int y = 10;

            // ============================================================
            // 1. 标题
            // ============================================================
            p.setPen(textColor);
            p.setFont(QFont("", 12, QFont::Bold));
            p.drawText(QRect(margin, y, w - 2*margin, 28), Qt::AlignLeft, "📈 趋势分析");

            y += 34;

            // ============================================================
            // 2. 日期范围（标题下方）
            // ============================================================
            // 模式按钮：本周、本月、自定义 居左侧
            QStringList modes = {"本周", "本月", "自定义"};
            int btnW = 48, btnH = 24;
            int btnX = margin;
            int btnY = y;

            for (int i = 0; i < modes.size(); ++i) {
                QRect btnRect(btnX + i * (btnW + 6), btnY, btnW, btnH);
                bool active = (i == m_selectedMode);
                QColor btnBg = active ? accent : subTextColor;
                btnBg.setAlpha(active ? 200 : 60);
                p.fillRect(btnRect, btnBg);
                p.setPen(active ? Qt::white : subTextColor);
                p.setFont(QFont("", 9));
                p.drawText(btnRect, Qt::AlignCenter, modes[i]);
                m_modeRects[i] = btnRect;
            }

            // 日期范围 居右侧
            p.setPen(subTextColor);
            p.setFont(QFont("", 9));

            if (m_selectedMode == 2) { // 自定义模式
                QString dateLabel = "📅 " + m_dateFrom.toString("yyyy-MM-dd") + " ~ " + m_dateTo.toString("yyyy-MM-dd");
                p.setPen(accent);
                p.setFont(QFont("", 9));
                
                QFontMetrics fm(p.font());
                int textW = fm.horizontalAdvance(dateLabel);
                int dateX = w - margin - textW;
                QRect dateRect(dateX, y, textW, 24);
                p.drawText(dateRect, Qt::AlignLeft | Qt::AlignVCenter, dateLabel);
                m_dateRect = dateRect;
            } else {
                QString dateLabel = "📅 " + m_dateFrom.toString("MM-dd") + " ~ " + m_dateTo.toString("MM-dd");
                p.setPen(subTextColor);
                p.setFont(QFont("", 9));
                
                QFontMetrics fm(p.font());
                int textW = fm.horizontalAdvance(dateLabel);
                int dateX = w - margin - textW;
                p.drawText(QRect(dateX, y, textW, 24), Qt::AlignLeft | Qt::AlignVCenter, dateLabel);
                m_dateRect = QRect();
            }

            y += 34;

            // ============================================================
            // 3. 趋势图
            // ============================================================
            int chartTop = y;
            int chartBottom = y + 140;
            int chartLeft = margin + 40;
            int chartRight = w - margin - 10;
            int chartW = chartRight - chartLeft;
            int chartH = chartBottom - chartTop;

            QVector<qint64> data = getRangeData();
            QStringList labels = getRangeLabels();

            if (data.isEmpty() || data.size() < 2) {
                p.setPen(subTextColor);
                p.setFont(QFont("", 10));
                p.drawText(QRect(chartLeft, chartTop + chartH/2 - 8, chartW, 16),
                        Qt::AlignCenter, "暂无数据");
                y += chartH + 20;
            } else {
                qint64 maxVal = 0;
                for (qint64 v : data) if (v > maxVal) maxVal = v;
                if (maxVal == 0) maxVal = 1;

                // 网格线
                p.setPen(QPen(subTextColor, 1, Qt::DotLine));
                for (int i = 0; i <= 4; ++i) {
                    double yy = chartTop + i * chartH / 4.0;
                    p.drawLine(chartLeft, yy, chartRight, yy);
                }

                // Y轴标签
                p.setPen(subTextColor);
                p.setFont(QFont("", 8));
                for (int i = 0; i <= 4; ++i) {
                    double yy = chartTop + i * chartH / 4.0;
                    qint64 val = maxVal * (4 - i) / 4;
                    p.drawText(QRect(margin, yy - 8, chartLeft - margin, 16),
                            Qt::AlignRight | Qt::AlignVCenter, formatNumber(val));
                }

                // X轴标签
                for (int i = 0; i < labels.size() && i < data.size(); ++i) {
                    double x = chartLeft + (i / (double)(data.size() - 1)) * chartW;
                    p.drawText(QRect(x - 20, chartBottom + 4, 40, 16),
                            Qt::AlignHCenter, labels[i]);
                }

                // 计算点位置
                QVector<QPointF> points;
                for (int i = 0; i < data.size(); ++i) {
                    double x = chartLeft + (i / (double)(data.size() - 1)) * chartW;
                    double yp = chartBottom - (data[i] / (double)maxVal) * chartH;
                    points.append(QPointF(x, yp));
                }

                if (points.size() >= 2) {
                    // 填充区域
                    QPainterPath fillPath;
                    fillPath.moveTo(points[0]);
                    for (int i = 1; i < points.size(); ++i) {
                        fillPath.lineTo(points[i]);
                    }
                    fillPath.lineTo(QPointF(points.last().x(), chartBottom));
                    fillPath.lineTo(QPointF(points[0].x(), chartBottom));
                    fillPath.closeSubpath();
                    QColor fillColor = accent;
                    fillColor.setAlpha(40);
                    p.fillPath(fillPath, fillColor);

                    // 折线
                    QPainterPath linePath;
                    linePath.moveTo(points[0]);
                    for (int i = 1; i < points.size(); ++i) {
                        linePath.lineTo(points[i]);
                    }
                    p.setPen(QPen(accent, 2));
                    p.drawPath(linePath);

                    // 数据点
                    for (int i = 0; i < points.size(); ++i) {
                        if (data[i] == 0) continue;
                        double r = (i == points.size() - 1) ? 4.0 : 2.0;
                        QColor c = (i == points.size() - 1) ? accent : subTextColor;
                        p.setBrush(c);
                        p.setPen(Qt::NoPen);
                        p.drawEllipse(points[i], r, r);
                    }

                    if (data.last() > 0) {
                        p.setPen(accent);
                        p.setFont(QFont("", 9, QFont::Bold));
                        p.drawText(QRect(points.last().x() - 30, points.last().y() - 24, 60, 18),
                                Qt::AlignCenter, formatNumber(data.last()));
                    }
                }

                y += chartH + 30;
            }

            // ============================================================
            // 4. 字词分布
            // ============================================================
            p.setPen(textColor);
            p.setFont(QFont("", 12, QFont::Bold));
            p.drawText(QRect(margin, y, w - 2*margin, 24), Qt::AlignLeft, "📊 字词分布");

            y += 30;
            int barH = 20;
            int barGap = 6;
            int labelW = 40;
            int maxBarW = w - 2*margin - labelW - 20;

            auto dist = getRangeDist();
            int total = dist[0] + dist[1] + dist[2] + dist[3] + dist[4];
            if (total == 0) total = 1;

            struct DistItem { QString label; int count; int total; };
            DistItem items[5] = {
                {"1字", dist[0], total},
                {"2字", dist[1], total},
                {"3字", dist[2], total},
                {"4字", dist[3], total},
                {"5+字", dist[4], total}
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
                    p.drawText(QRect(margin + labelW + maxBarW + 4, y, 50, barH),
                            Qt::AlignLeft | Qt::AlignVCenter, cnt);
                }
                y += barH + barGap;
            }

            y += 10;

            // ============================================================
            // 5. 统计摘要
            // ============================================================
            int phraseCommits = total - dist[0];
            double phraseRate = total > 0 ? (double)phraseCommits / total * 100 : 0.0;
            double avgLen = total > 0 ? (double)(dist[0]*1 + dist[1]*2 + dist[2]*3 + dist[3]*4 + dist[4]*5) / total : 0.0;

            QRect summaryRect(margin, y, w - 2*margin, 55);
            p.fillRect(summaryRect, bgColor);

            p.setPen(accent);
            p.setFont(QFont("", 14, QFont::Bold));
            QString rateStr = QString::number(phraseRate, 'f', 1) + "%";
            p.drawText(QRect(margin + 16, y + 6, 80, 24), Qt::AlignLeft | Qt::AlignVCenter, rateStr);

            p.setPen(subTextColor);
            p.setFont(QFont("", 9));
            p.drawText(QRect(margin + 16 + 90, y + 6, 80, 24), Qt::AlignLeft | Qt::AlignVCenter,
                    "词组连打率");

            p.setPen(subTextColor);
            p.setFont(QFont("", 9));
            QString stats = QString("提交: %1  字数: %2  平均: %3字/次")
                .arg(total)
                .arg(dist[0]*1 + dist[1]*2 + dist[2]*3 + dist[3]*4 + dist[4]*5)
                .arg(avgLen, 0, 'f', 1);
            p.drawText(QRect(margin + 16, y + 34, w - 2*margin - 32, 18),
                    Qt::AlignLeft | Qt::AlignVCenter, stats);
        }

        void mousePressEvent(QMouseEvent* event) override {
            QPoint pos = event->pos();
            bool changed = false;

            // 检查模式按钮点击
            for (int i = 0; i < 3; ++i) {
                if (m_modeRects[i].contains(pos)) {
                    m_selectedMode = i;
                    changed = true;
                    break;
                }
            }

            // 检查日期区域点击（自定义模式下弹出日期选择）
            if (m_selectedMode == 2 && m_dateRect.contains(pos)) {
                showDatePickerDialog();
                changed = true;
            }

            if (changed) {
                updateRangeDates();
                update();
            }
        }

    private:
        void showDatePickerDialog() {
            // 使用简单的对话框选择日期范围
            QDialog* dialog = new QDialog(this);
            dialog->setWindowTitle("选择日期范围");
            dialog->setModal(true);
            dialog->resize(280, 140);

            QVBoxLayout* layout = new QVBoxLayout(dialog);

            QHBoxLayout* fromLayout = new QHBoxLayout();
            QLabel* fromLabel = new QLabel("从:", dialog);
            QDateEdit* fromEdit = new QDateEdit(dialog);
            fromEdit->setCalendarPopup(true);
            fromEdit->setDate(m_dateFrom);
            fromLayout->addWidget(fromLabel);
            fromLayout->addWidget(fromEdit);

            QHBoxLayout* toLayout = new QHBoxLayout();
            QLabel* toLabel = new QLabel("到:", dialog);
            QDateEdit* toEdit = new QDateEdit(dialog);
            toEdit->setCalendarPopup(true);
            toEdit->setDate(m_dateTo);
            toLayout->addWidget(toLabel);
            toLayout->addWidget(toEdit);

            QHBoxLayout* btnLayout = new QHBoxLayout();
            QPushButton* okBtn = new QPushButton("确定", dialog);
            QPushButton* cancelBtn = new QPushButton("取消", dialog);
            btnLayout->addStretch();
            btnLayout->addWidget(okBtn);
            btnLayout->addWidget(cancelBtn);

            layout->addLayout(fromLayout);
            layout->addLayout(toLayout);
            layout->addLayout(btnLayout);

            connect(okBtn, &QPushButton::clicked, [=]() {
                m_dateFrom = fromEdit->date();
                m_dateTo = toEdit->date();
                if (m_dateFrom > m_dateTo) {
                    std::swap(m_dateFrom, m_dateTo);
                }
                dialog->accept();
                updateRangeDates();
                update();
            });
            connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);

            dialog->exec();
            delete dialog;
        }

        void updateRangeDates() {
            QDate today = QDate::currentDate();
            switch (m_selectedMode) {
                case 0: // 本周
                    m_dateFrom = today.addDays(-6);
                    m_dateTo = today;
                    break;
                case 1: // 本月
                    m_dateFrom = today.addDays(-29);
                    m_dateTo = today;
                    break;
                case 2: // 自定义
                    if (!m_dateFrom.isValid() || !m_dateTo.isValid()) {
                        m_dateFrom = today.addDays(-7);
                        m_dateTo = today;
                    }
                    break;
            }
            if (!m_dateFrom.isValid()) m_dateFrom = today.addDays(-6);
            if (!m_dateTo.isValid()) m_dateTo = today;
            if (m_dateFrom > m_dateTo) {
                std::swap(m_dateFrom, m_dateTo);
            }
        }

        QVector<qint64> getRangeData() const {
            QVector<qint64> result;
            if (!m_panel || !m_panel->m_engine) return result;
            if (!m_dateFrom.isValid() || !m_dateTo.isValid()) return result;

            QString from = m_dateFrom.toString("yyyy-MM-dd");
            QString to = m_dateTo.toString("yyyy-MM-dd");

            auto data = m_panel->m_engine->dailyRange(from, to);
            
            QMap<QString, qint64> dataMap;
            for (const auto& [date, chars] : data) {
                dataMap[date] = chars;
            }
            
            QDate current = m_dateFrom;
            QDate end = m_dateTo;
            while (current <= end) {
                QString dateStr = current.toString("yyyy-MM-dd");
                result.append(dataMap.value(dateStr, 0));
                current = current.addDays(1);
            }
            return result;
        }

        QStringList getRangeLabels() const {
            QStringList labels;
            if (!m_dateFrom.isValid() || !m_dateTo.isValid()) return labels;
            QDate current = m_dateFrom;
            QDate end = m_dateTo;
            while (current <= end) {
                labels.append(current.toString("MM-dd"));
                current = current.addDays(1);
            }
            return labels;
        }

        std::array<int, 5> getRangeDist() const {
            std::array<int, 5> result{0, 0, 0, 0, 0};
            if (!m_panel || !m_panel->m_engine) return result;
            if (!m_dateFrom.isValid() || !m_dateTo.isValid()) return result;

            QString from = m_dateFrom.toString("yyyy-MM-dd");
            QString to = m_dateTo.toString("yyyy-MM-dd");

            auto stats = m_panel->m_engine->rangeStats(from, to);
            result[0] = stats.cnt1;
            result[1] = stats.cnt2;
            result[2] = stats.cnt3;
            result[3] = stats.cnt4;
            result[4] = stats.cnt5plus;
            return result;
        }

        InsightPanel* m_panel = nullptr;
        int m_selectedMode = 0;
        QDate m_dateFrom;
        QDate m_dateTo;
        QRect m_modeRects[3];
        QRect m_dateRect;
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

        // 订阅引擎数据更新
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
        m_analysisPage = new AnalysisPage(this);

        m_tabWidget->addTab(m_levelPage, "🏆 段位");
        m_tabWidget->addTab(m_todayPage, "📊 今日");
        m_tabWidget->addTab(m_analysisPage, "📈 分析");
        

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
        if (m_levelPage) m_levelPage->update();
        if (m_analysisPage) m_analysisPage->update();
    }

    void InsightPanel::updateData()
    {
        if (!m_engine) return;

        // 今日数据
        m_cache.todayChars = m_engine->todayChars();
        m_cache.todayCommits = m_engine->todayCommits();
        m_cache.avgSpeed = m_engine->avgSpeed();
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