// src/ui/insight_panel.h
// SPDX-License-Identifier: LGPL-2.1-or-later
// 统计仪表盘 — 多标签页展示详细统计
// 自管理：订阅 StatisticsEngine 数据更新，自行控制显示/隐藏

#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QPointer>
#include <array>
#include <qglobal.h>

namespace wordcount {
    class StatisticsEngine;
}

namespace wordcount {

    /**
     * @brief 统计仪表盘主窗口（普通窗口，带标题栏）
     * 自管理：构造时传入引擎，自动订阅数据更新
     */
    class InsightPanel : public QWidget {
        Q_OBJECT
    public:
        explicit InsightPanel(StatisticsEngine* engine, QWidget* parent = nullptr);
        ~InsightPanel() override;

        // ========== 面板管理（静态方法） ==========
        static InsightPanel* instance();
        static void showPanel();
        static void togglePanel();
        static void hidePanel();
        static bool isVisible();

    protected:
        void closeEvent(QCloseEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private slots:
        void onStatsChanged(qint64 total, qint64 today, double wpm);

    private:
        void setupUI();
        void updateData();

        // 内部页面
        class TodayPage;
        class EfficiencyPage;
        class TrendPage;
        class LevelPage;

        StatisticsEngine* m_engine = nullptr;
        QTabWidget* m_tabWidget = nullptr;
        TodayPage* m_todayPage = nullptr;
        EfficiencyPage* m_efficiencyPage = nullptr;
        TrendPage* m_trendPage = nullptr;
        LevelPage* m_levelPage = nullptr;

        // 数据缓存（各页面共享）
        struct Cache {
            qint64 todayChars = 0;
            int todayCommits = 0;
            double avgSpeed = 0.0;
            int peakSpeed = 0;
            int dist1 = 0, dist2 = 0, dist3 = 0, dist4 = 0, dist5plus = 0;
            QString tierName;
            QString tierRank;
            QString tierSubtitle;
            QColor tierColor;
            double progress = 0.0;
            qint64 nextThreshold = 0;
            qint64 totalChars = 0;
            std::array<qint64, 7> daily7{};
        } m_cache;

        static QPointer<InsightPanel> s_instance;
    };

} // namespace wordcount