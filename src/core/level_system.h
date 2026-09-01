// SPDX-License-Identifier: LGPL-2.1-or-later
// 文人雅称等级系统 — 8 段位 × 10 级 = 80 级
// 段位：书生 / 秀才 / 举人 / 进士 / 翰林 / 大学士 / 文豪 / 文圣
// 字数门槛按对数曲线设计，新手升级快，后期有挑战
#ifndef WORDCOUNT_LEVEL_SYSTEM_H
#define WORDCOUNT_LEVEL_SYSTEM_H

#include <QString>
#include <QColor>
#include <qglobal.h>

namespace wordcount {

struct TierDef {
    const char* tierName;     // "进士" 段位名（等级行/升级通知用）
    const char* fullName;     // "进士出身" 段位雅称（详情面板副行用）
    const char* shortName;    // "进" 段位缩写（托盘 24×24 用）
    qint64 minChars;          // 进入本段的字数下限
    qint64 spanChars;         // 本段跨度（10 级的总量），0 = 顶级无限
    const char* colorHex;     // 段位主色
};

struct LevelInfo {
    int tierIndex;        // 段位索引 0..7
    int levelInTier;     // 段内等级 0..9
    QString tierName;    // "翰林"
    QString tierShort;   // "翰"
    QString tierRank;    // "四品"（段内等级的中文品级，一品~十品，与名号绑定显示）
    qint64 currentMin;   // 当前等级字数下限
    qint64 nextMin;      // 下一等级字数下限（顶级时 = currentMin）
    double progress;     // 当前段位内进度 0..1
    bool isMax;          // 是否顶级
    QColor tierColor;    // 段位颜色
};

class LevelSystem {
public:
    static const int kTierCount = 8;
    static const int kLevelsPerTier = 9;

    /// 根据总字数返回等级信息
    static LevelInfo levelForTotal(qint64 totalChars);

    /// 段位定义表
    static const TierDef& tierAt(int index);

    /// 段内等级 → 中文品级名（一品~十品）
    /// \param levelInTier 段内等级 0..9，越界自动夹取
    static const char* rankName(int levelInTier);

    /// 给定总字数，返回 0-based 全局等级号
    static int globalLevel(qint64 totalChars);
};

} // namespace wordcount

#endif // WORDCOUNT_LEVEL_SYSTEM_H
