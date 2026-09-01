// src/ui/insight_panel.h
#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QVector>
#include <QColor>
#include <QString>
#include <array>
#include <qglobal.h>

namespace wordcount {

/**
 * @brief 统计仪表盘数据（所有标签页共享）
 */
struct InsightData {
    // 今日
    qint64 todayChars = 0;
    int todayCommits = 0;
    double avgSpeed = 0.0;      // 字/分（60s窗口）
    int peakSpeed = 0;          // 字/分（10s窗口）

    // 字词分布（今日）
    int dist1 = 0, dist2 = 0, dist3 = 0, dist4 = 0, dist5plus = 0;

    // 段位
    QString tierName;
    QString tierRank;
    QString tierSubtitle;
    QColor tierColor;
    double progress = 0.0;
    qint64 nextThreshold = 0;
    qint64 totalChars = 0;

    // 近7天（索引0为7天前，索引6为今天）
    std::array<qint64, 7> daily7{};

    // 速度历史（可选）
    QVector<QPair<qint64, int>> speedHistory;
};

/**
 * @brief 统计仪表盘主窗口（普通窗口，带标题栏）
 */
class InsightPanel : public QWidget {
    Q_OBJECT
public:
    explicit InsightPanel(QWidget* parent = nullptr);
    ~InsightPanel() override = default;

    void refresh(const InsightData& data);

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUI();

    QTabWidget* m_tabWidget = nullptr;

    // 各标签页内部 Widget
    class TodayPage;
    class EfficiencyPage;
    class TrendPage;
    class LevelPage;

    TodayPage* m_todayPage = nullptr;
    EfficiencyPage* m_efficiencyPage = nullptr;
    TrendPage* m_trendPage = nullptr;
    LevelPage* m_levelPage = nullptr;

    InsightData m_data;
};

} // namespace wordcount