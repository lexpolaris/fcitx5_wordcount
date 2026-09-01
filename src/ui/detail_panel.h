#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <QJsonArray>
#include <QFont>
#include <QFontMetrics>

namespace wordcount {

    class DetailPanel : public QWidget
    {
        Q_OBJECT
    public:
        explicit DetailPanel(QWidget *parent = nullptr);

        void refresh(qint64 total, qint64 today, double wpm,
                     const QJsonArray &hourly, const QJsonArray &daily7);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void showEvent(QShowEvent *event) override;

    private:
        void drawHero(QPainter &p, const QRect &area);
        void drawNumberCards(QPainter &p, const QRect &area);
        void drawHourlyHeatmap(QPainter &p, const QRect &area);
        void drawDaily7(QPainter &p, const QRect &area);
        void drawEquivalence(QPainter &p, const QRect &area);

        static QString formatNumber(qint64 n);

        // 颜色辅助
        bool isDarkTheme() const;
        QColor textColor() const;
        QColor subTextColor() const;
        QColor bgColor() const;
        QColor cardBgColor() const;
        QColor separatorColor() const;
        QColor accentColor() const;
        QColor heatColor() const;
        QColor adjustedTierColor() const;

        // 数据
        qint64 m_total = 0;
        qint64 m_today = 0;
        double m_wpm = 0.0;
        QJsonArray m_hourly;
        QJsonArray m_daily7;

        // 等级相关
        QString m_tierName;
        QString m_tierRank;
        QString m_tierSubtitle;
        QString m_progressLine;
        QColor m_tierColor;
        double m_levelProgress = 0.0;
        bool m_isMax = false;

        // 预计算文本
        QString m_tierLine;
        QString m_cardToday, m_cardSpeed, m_cardTotal;
        QString m_badgeText;
        QColor m_badgeColor;
        QString m_daily7Delta;

        // 非段位模式缓存（保留扩展）
        QString m_rhythmTitle;
        QString m_rhythmSubtitle;
        double m_rhythmProgress = 0.0;
        QString m_peakLine;
        bool m_peakIsNewRecord = false;

        // 字体与度量
        QFont m_fontToday = QFont("", 18, QFont::Bold);
        QFont m_fontT4   = QFont("", 13, QFont::Bold);
        QFont m_fontT5   = QFont("", 12, QFont::Bold);
        QFont m_fontT7   = QFont("", 11);
        QFont m_fontT8   = QFont("", 9);
        QFont m_fontT10  = QFont("", 8);
        QFontMetrics m_fmToday{m_fontToday};
        QFontMetrics m_fmT4{m_fontT4};
        QFontMetrics m_fmT5{m_fontT5};
        QFontMetrics m_fmT7{m_fontT7};
        QFontMetrics m_fmT8{m_fontT8};
        QFontMetrics m_fmT10{m_fontT10};

        // 面板尺寸（外部可见）
        static constexpr int kPanelWidth = 280;
        static constexpr int kPanelHeight = 440;   // 增高至 440
    };

} // namespace wordcount
