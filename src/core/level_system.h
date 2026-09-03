// src/core/level_system.h
#pragma once

#include <QString>
#include <QColor>
#include <QVector>
#include <QStringList>
#include <qglobal.h>

namespace wordcount {

// ★ 先定义 TierDef
struct TierDef {
    const char* tierName;     // 段位名
    const char* fullName;     // 段位雅称
    const char* shortName;    // 段位缩写
    qint64 minChars;          // 进入门槛
    qint64 spanChars;         // 段位跨度（0=顶级）
    const char* colorHex;     // 主色
};

// ★ 再定义 TierScheme（使用 TierDef）
struct TierScheme {
    const char* name;           // 方案名称
    const char* description;    // 简短描述
    TierDef tiers[9];           // 9个段位
};

struct LevelInfo {
    int tierIndex;
    int levelInTier;
    QString tierName;
    QString tierShort;
    QString tierRank;
    qint64 currentMin;
    qint64 nextMin;
    double progress;
    bool isMax;
    QColor tierColor;
};

class LevelSystem {
public:
    static const int kTierCount = 9;
    static const int kLevelsPerTier = 9;

    // 获取当前使用的方案
    static const TierScheme& currentScheme();
    
    // 切换方案
    static bool setScheme(int schemeIndex);
    static int currentSchemeIndex();
    
    // 获取所有方案列表
    static const QVector<TierScheme>& schemes();
    static const QStringList schemeNames();
    
    // ★ 兼容旧接口（用于 detail_panel 和 insight_panel）
    static const TierDef& tierAt(int index);
    
    // 核心接口
    static LevelInfo levelForTotal(qint64 totalChars);
    static const char* rankName(int levelInTier);
    static int globalLevel(qint64 totalChars);

private:
    static int s_currentSchemeIndex;
    static QVector<TierScheme> s_schemes;
    static void initSchemes();
};

} // namespace wordcount