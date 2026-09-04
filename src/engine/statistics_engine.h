// src/engine/statistics_engine.h
#pragma once

#include <QObject>
#include <QTimer>
#include <QJsonArray>
#include <array>
#include <qglobal.h>

namespace wordcount {

    class DatabaseStorage;
    class SpeedTracker;

    class StatisticsEngine : public QObject {
        Q_OBJECT
    public:
        static constexpr int kHourlyCount = 24;
        static constexpr int kDaily7Count = 7;

        explicit StatisticsEngine(DatabaseStorage* db, QObject* parent = nullptr);
        ~StatisticsEngine();

        void load();
        void flushToDatabase();
        void onTextCommitted(const QString& text);
        void checkDateRollover();

        void setCountPunctuation(bool enable);
        void setCountEmoji(bool enable);

        qint64 totalChars() const;
        qint64 todayChars() const;
        double currentWpm() const;
        QJsonArray hourlyToday() const;
        QJsonArray daily7() const;

        void resetToday();
        void resetAll();

        int todayCommits() const;                  // 今日提交次数
        int peakSpeed() const;                     // 今日峰值速度
        double avgSpeed() const;                   // 今日平均速度
        std::array<qint64, 5> distToday() const;   // 字词分布

        /// 获取指定日期范围的每日数据
        std::vector<std::pair<QString, qint64>> dailyRange(
            const QString& from, const QString& to) const;

        /// 获取指定日期范围的聚合统计
        struct RangeStats {
            qint64 totalChars = 0;
            int totalCommits = 0;
            int cnt1 = 0, cnt2 = 0, cnt3 = 0, cnt4 = 0, cnt5plus = 0;
        };
        RangeStats rangeStats(const QString& from, const QString& to) const;

        /// 获取数据库存储（仅供设置面板使用）
        DatabaseStorage* getDatabase() const { return m_db; }

    signals:
        void statsChanged(qint64 total, qint64 today, double wpm);
        void levelUp(int oldLevel, int newLevel, int levelsCrossed);

    private:
        void refreshDaily7Cache();

        DatabaseStorage* m_db = nullptr;
        SpeedTracker* m_speed = nullptr;

        // 缓存
        qint64 m_cachedTotal = 0;
        qint64 m_cachedToday = 0;
        QString m_todayDate;
        std::array<qint64, 24> m_cachedHourly{};
        std::array<qint64, 7> m_cachedDaily7{};
        std::array<qint64, 5> m_cachedDist{};

        int m_lastGlobalLevel = 0;
        QString m_lastRolloverCheckDate;
        bool m_dirty = false;
        QTimer* m_flushTimer = nullptr;

        struct PendingCommit {
            qint64 ts;
            QString date;
            int hour;
            int chars;
            int lenCat;
        };
        QVector<PendingCommit> m_pendingCommits;
        static constexpr int kFlushIntervalMs = 5000;
        static constexpr int kMaxPendingSize = 100;

        bool m_countPunctuation = false;
        bool m_countEmoji = false;

        void updateSpeedStats(qint64 chars, qint64 now);
        struct SpeedSample {
            qint64 ts;
            qint64 chars;
        };
        QVector<SpeedSample> m_speedSamples;
        double m_avgSpeed = 0.0;
        int m_peakSpeed = 0;
        qint64 m_todayFirstCommitTime = 0;   // 今日第一次提交时间
        qint64 m_todayLastCommitTime = 0;    // 今日最后一次提交时间
        double m_todayTotalTimeMinutes = 0;  // 今日总输入时间（分钟）
    };

} // namespace wordcount