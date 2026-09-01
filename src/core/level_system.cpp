// SPDX-License-Identifier: LGPL-2.1-or-later
// 文人雅称等级系统实现
#include "level_system.h"
#include <algorithm>
#include <cmath>

namespace wordcount {

// 8 段位定义，字数门槛按近似对数曲线
// 字数单位：个
// 注意 spanChars = 0 表示顶级，进度永远 < 1
static const TierDef kTiers[9] = {
    {"蒙童",   "初识文字，启润文心", "童",       0,    500,   "#888888"},
    {"布衣",   "布衣书生，寒窗独守", "布",     500,   1500,   "#4A9D8E"},
    {"青衿",   "青衿学子，博学笃志", "青",    2000,   3000,   "#4A7D9D"},
    {"文士",   "文采斐然，落笔成章", "文",    5000,   5000,   "#7D4A9D"},
    {"墨客",   "骚人墨客，兴会淋漓", "墨",   10000,  10000,   "#C9A227"},
    {"雅士",   "雅量高致，不流于俗", "雅",   20000,  30000,   "#A8821D"},
    {"鸿儒",   "谈笑有鸿儒，往来无白丁", "儒", 50000, 80000, "#2E86AB"},
    {"文宗",   "文坛领袖，笔力千钧", "宗",  130000, 100000,  "#B33A3A"},
    {"文圣",   "一代文圣，万世师表", "圣",  230000,      0,   "#8B5CF6"},
};

const TierDef& LevelSystem::tierAt(int index) {
    Q_ASSERT(index >= 0 && index < LevelSystem::kTierCount);
    return kTiers[index];
}

// 段内等级 → 中文品级，与名号绑定显示。
// 审计：此前 detail_panel.cpp 重复定义一份 kRankNames，提升为公共接口去重
const char* LevelSystem::rankName(int levelInTier) {
    static const char *kRankNames[9] = {
        "九品", "八品", "七品", "六品", "五品",
        "四品", "三品", "二品", "一品"
    };
    return kRankNames[qBound(0, levelInTier, 8)];
}

// 段内第 k 级（0-based）的段内字数下限
// 指数 1.5 曲线：开头加强难度、后期递增更难
// 例：书生段 span=500 →
//   一品 0 / 二品 45 / 三品 82 / 四品 126 / 五品 177 /
//   六品 232 / 七品 293 / 八品 358 / 九品 427 / 十品 500
// 公式：span × ((k+1)/9)^1.5
static qint64 tierLevelMin(qint64 span, int k) {
    if (span <= 0 || k <= 0) return 0; // 一品从段位起点开始
    const double ratio = std::pow((k + 1) / 9.0, 1.5);
    return static_cast<qint64>(span * ratio);
}

int LevelSystem::globalLevel(qint64 totalChars) {
    // 二分查找当前段位
    int lo = 0, hi = LevelSystem::kTierCount - 1;
    // 找到第一个 minChars > totalChars 的段，再回退一个
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (kTiers[mid].minChars <= totalChars) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    int tierIdx = lo;

    // 段内等级 0..9（平方曲线）
    qint64 offset = totalChars - kTiers[tierIdx].minChars;
    qint64 span = kTiers[tierIdx].spanChars;
    int levelInTier = 0;
    if (span > 0) {
        for (int k = 0; k < LevelSystem::kLevelsPerTier; ++k) {
            if (offset >= tierLevelMin(span, k)) levelInTier = k;
            else break;
        }
    } else {
        // 顶级段，spanChars = 0
        levelInTier = LevelSystem::kLevelsPerTier - 1; // 永远顶级
    }

    return tierIdx * LevelSystem::kLevelsPerTier + levelInTier;
}

LevelInfo LevelSystem::levelForTotal(qint64 totalChars) {
    LevelInfo info{};

    int globalLv = globalLevel(totalChars);
    info.tierIndex = globalLv / LevelSystem::kLevelsPerTier;
    info.levelInTier = globalLv % LevelSystem::kLevelsPerTier;

    const TierDef& tier = kTiers[info.tierIndex];
    // 中文段位名以 UTF-8 存储，必须用 fromUtf8 转换（fromLatin1 会乱码）
    info.tierName = QString::fromUtf8(tier.tierName);
    info.tierShort = QString::fromUtf8(tier.shortName);
    info.tierColor = QColor(QString::fromLatin1(tier.colorHex));

    // 调用品级
    info.tierRank = QString::fromUtf8(
        LevelSystem::rankName(info.levelInTier));

    // 当前等级字数下限（平方曲线：前期快、后期难）
    if (tier.spanChars <= 0) {
        // 顶级段（文圣），没有下一级
        info.currentMin = tier.minChars;
        info.nextMin = tier.minChars;
        info.isMax = true;
        info.progress = 1.0;
    } else {
        // 当前品级下限 = tierMin + tierLevelMin(span, levelInTier)
        info.currentMin = tier.minChars + tierLevelMin(tier.spanChars, info.levelInTier);
        if (info.levelInTier < LevelSystem::kLevelsPerTier - 1) {
            // 段内下一品：用平方曲线计算
            info.nextMin = tier.minChars + tierLevelMin(tier.spanChars, info.levelInTier + 1);
            info.isMax = false;
        } else {
            // 段内顶级（十品），下一级是下一段起始（如果有）
            if (info.tierIndex < LevelSystem::kTierCount - 1) {
                info.nextMin = kTiers[info.tierIndex + 1].minChars;
            } else {
                info.nextMin = info.currentMin;
            }
            info.isMax = (info.tierIndex == LevelSystem::kTierCount - 1);
        }
        // 段内进度
        if (info.nextMin > info.currentMin) {
            info.progress = static_cast<double>(totalChars - info.currentMin)
                          / static_cast<double>(info.nextMin - info.currentMin);
            if (info.progress < 0.0) info.progress = 0.0;
            if (info.progress > 1.0) info.progress = 1.0;
        } else {
            info.progress = 1.0;
        }
    }

    return info;
}

} // namespace wordcount
