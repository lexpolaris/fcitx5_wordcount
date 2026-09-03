// src/core/level_system.cpp
#include "level_system.h"
#include <QSettings>
#include <algorithm>
#include <cmath>

namespace wordcount {

int LevelSystem::s_currentSchemeIndex = 0;
QVector<TierScheme> LevelSystem::s_schemes;

// 初始化所有方案
void LevelSystem::initSchemes()
{
    if (!s_schemes.isEmpty()) return;

    // ===== 方案1: 文人雅称 =====
    TierScheme scheme1;
    scheme1.name = "文人雅称";
    scheme1.description = "传统文人成长之路";
    scheme1.tiers[0] = {"蒙童", "初识文字，启润文心", "童", 0, 500, "#888888"};
    scheme1.tiers[1] = {"布衣", "布衣书生，寒窗独守", "布", 500, 1500, "#4A9D8E"};
    scheme1.tiers[2] = {"青衿", "青衿学子，博学笃志", "青", 2000, 3000, "#4A7D9D"};
    scheme1.tiers[3] = {"文士", "文采斐然，落笔成章", "文", 5000, 5000, "#7D4A9D"};
    scheme1.tiers[4] = {"墨客", "骚人墨客，兴会淋漓", "墨", 10000, 10000, "#C9A227"};
    scheme1.tiers[5] = {"雅士", "雅量高致，不流于俗", "雅", 20000, 30000, "#A8821D"};
    scheme1.tiers[6] = {"鸿儒", "谈笑有鸿儒，往来无白丁", "儒", 50000, 80000, "#2E86AB"};
    scheme1.tiers[7] = {"文宗", "文坛领袖，笔力千钧", "宗", 130000, 100000, "#B33A3A"};
    scheme1.tiers[8] = {"文圣", "一代文圣，万世师表", "圣", 230000, 0, "#8B5CF6"};
    s_schemes.append(scheme1);

    // ===== 方案2: 科举制 =====
    TierScheme scheme2;
    scheme2.name = "科举制";
    scheme2.description = "古代科举进阶之路";
    scheme2.tiers[0] = {"童生", "初识经义，发蒙启智", "童", 0, 300, "#8D8D8D"};
    scheme2.tiers[1] = {"秀才", "秀才及第，初显才名", "秀", 300, 700, "#4A9D8E"};
    scheme2.tiers[2] = {"举人", "乡试中举，春风得意", "举", 1000, 2000, "#4A7D9D"};
    scheme2.tiers[3] = {"贡士", "会试中贡，殿试在望", "贡", 3000, 5000, "#7D4A9D"};
    scheme2.tiers[4] = {"进士", "进士及第，金榜题名", "进", 8000, 12000, "#C9A227"};
    scheme2.tiers[5] = {"翰林", "翰林编修，清贵之选", "翰", 20000, 30000, "#A8821D"};
    scheme2.tiers[6] = {"学士", "入阁拜相，国之栋梁", "大", 50000, 70000, "#2E86AB"};
    scheme2.tiers[7] = {"太傅", "天子之师，德高望重", "傅", 120000, 100000, "#B33A3A"};
    scheme2.tiers[8] = {"文正", "谥号文正，千古名臣", "正", 220000, 0, "#8B5CF6"};
    s_schemes.append(scheme2);

    // ===== 方案3: 武道修行 =====
    TierScheme scheme3;
    scheme3.name = "武道修行";
    scheme3.description = "武者修炼之路";
    scheme3.tiers[0] = {"学徒", "初入武道，筑基锻体", "学", 0, 200, "#8D8D8D"};
    scheme3.tiers[1] = {"武者", "武艺初成，登堂入室", "武", 200, 600, "#4A9D8E"};
    scheme3.tiers[2] = {"武士", "武艺精湛，可堪一战", "士", 800, 1200, "#4A7D9D"};
    scheme3.tiers[3] = {"武师", "开宗立派，广收门徒", "师", 2000, 3000, "#7D4A9D"};
    scheme3.tiers[4] = {"大师", "一代宗师，名震江湖", "大", 5000, 10000, "#C9A227"};
    scheme3.tiers[5] = {"宗师", "武道巅峰，以武入道", "宗", 15000, 25000, "#A8821D"};
    scheme3.tiers[6] = {"武帝", "武帝之名，天下无敌", "帝", 40000, 60000, "#2E86AB"};
    scheme3.tiers[7] = {"武神", "神境强者，号令天下", "神", 100000, 100000, "#B33A3A"};
    scheme3.tiers[8] = {"武圣", "超凡入圣，万古流芳", "圣", 200000, 0, "#8B5CF6"};
    s_schemes.append(scheme3);

    // ===== 方案4: 修仙境界 =====
    TierScheme scheme4;
    scheme4.name = "修仙境界";
    scheme4.description = "仙途漫漫，长生久视";
    scheme4.tiers[0] = {"凡人", "凡人之躯，初入仙途", "凡", 0, 100, "#8D8D8D"};
    scheme4.tiers[1] = {"练气", "炼气化神，引气入体", "气", 100, 400, "#4A9D8E"};
    scheme4.tiers[2] = {"筑基", "筑基凝丹，仙基初成", "基", 500, 1500, "#4A7D9D"};
    scheme4.tiers[3] = {"金丹", "金丹结成，长生有望", "丹", 2000, 6000, "#7D4A9D"};
    scheme4.tiers[4] = {"元婴", "元婴出窍，神通初显", "婴", 8000, 22000, "#C9A227"};
    scheme4.tiers[5] = {"化神", "化神合道，逍遥天地", "化", 30000, 50000, "#A8821D"};
    scheme4.tiers[6] = {"合体", "合体归真，大道可期", "合", 80000, 100000, "#2E86AB"};
    scheme4.tiers[7] = {"大乘", "大乘圆满，飞升在即", "乘", 180000, 120000, "#B33A3A"};
    scheme4.tiers[8] = {"渡劫", "渡劫飞升，证道成仙", "劫", 300000, 0, "#8B5CF6"};
    s_schemes.append(scheme4);

    // ===== 方案5: 现代职场 =====
    TierScheme scheme5;
    scheme5.name = "现代职场";
    scheme5.description = "打工人升级之路";
    scheme5.tiers[0] = {"实习", "初入职场，虚心学习", "实", 0, 200, "#8D8D8D"};
    scheme5.tiers[1] = {"初级", "基础扎实，独立工作", "初", 200, 600, "#4A9D8E"};
    scheme5.tiers[2] = {"中级", "经验丰富，独当一面", "中", 800, 1200, "#4A7D9D"};
    scheme5.tiers[3] = {"高级", "技术骨干，带领团队", "高", 2000, 4000, "#7D4A9D"};
    scheme5.tiers[4] = {"资深", "领域专家，技术核心", "资", 6000, 9000, "#C9A227"};
    scheme5.tiers[5] = {"专家", "行业翘楚，标准制定", "专", 15000, 25000, "#A8821D"};
    scheme5.tiers[6] = {"总监", "统筹全局，运筹帷幄", "监", 40000, 60000, "#2E86AB"};
    scheme5.tiers[7] = {"副总", "公司高层，战略决策", "副", 100000, 100000, "#B33A3A"};
    scheme5.tiers[8] = {"CEO", "执掌公司，登顶巅峰", "总", 200000, 0, "#8B5CF6"};
    s_schemes.append(scheme5);

    // ===== 方案6: 游戏段位 =====
    TierScheme scheme6;
    scheme6.name = "游戏段位";
    scheme6.description = "竞技游戏排位之路";
    scheme6.tiers[0] = {"青铜", "初入峡谷，百折不挠", "铜", 0, 300, "#CD7F32"};
    scheme6.tiers[1] = {"白银", "白银之姿，稳步前行", "银", 300, 700, "#C0C0C0"};
    scheme6.tiers[2] = {"黄金", "黄金段位，高手初成", "金", 1000, 2000, "#FFD700"};
    scheme6.tiers[3] = {"铂金", "铂金之才，出类拔萃", "铂", 3000, 5000, "#E5E4E2"};
    scheme6.tiers[4] = {"钻石", "钻石之心，坚不可摧", "钻", 8000, 12000, "#B9F2FF"};
    scheme6.tiers[5] = {"大师", "大师风范，技艺超群", "师", 20000, 30000, "#9B59B6"};
    scheme6.tiers[6] = {"宗师", "一代宗师，登峰造极", "宗", 50000, 70000, "#E74C3C"};
    scheme6.tiers[7] = {"王者", "王者之巅，荣耀加身", "王", 120000, 130000, "#F1C40F"};
    scheme6.tiers[8] = {"传说", "传说之上，永垂不朽", "传", 250000, 0, "#8B5CF6"};
    s_schemes.append(scheme6);
}

// 获取当前方案
const TierScheme& LevelSystem::currentScheme()
{
    initSchemes();
    if (s_currentSchemeIndex < 0 || s_currentSchemeIndex >= s_schemes.size()) {
        s_currentSchemeIndex = 0;
    }
    return s_schemes[s_currentSchemeIndex];
}

// 兼容旧接口
const TierDef& LevelSystem::tierAt(int index)
{
    initSchemes();
    const auto& scheme = currentScheme();
    if (index < 0 || index >= kTierCount) {
        index = 0;
    }
    return scheme.tiers[index];
}

// 切换方案
bool LevelSystem::setScheme(int schemeIndex)
{
    initSchemes();
    if (schemeIndex < 0 || schemeIndex >= s_schemes.size()) {
        qWarning() << "setScheme: 无效索引" << schemeIndex;
        return false;
    }
    s_currentSchemeIndex = schemeIndex;
    qDebug() << "setScheme: 切换到方案" << schemeIndex << s_schemes[schemeIndex].name;
    QSettings settings("Fcitx5WordCount", "Fcitx5WordCount");
    settings.setValue("tierScheme", schemeIndex);
    return true;
}

int LevelSystem::currentSchemeIndex()
{
    initSchemes();
    return s_currentSchemeIndex;
}

const QVector<TierScheme>& LevelSystem::schemes()
{
    initSchemes();
    return s_schemes;
}

const QStringList LevelSystem::schemeNames()
{
    initSchemes();
    QStringList names;
    for (const auto& s : s_schemes) {
        names.append(QString::fromUtf8(s.name));
    }
    return names;
}

// 段内等级 → 中文品级
const char* LevelSystem::rankName(int levelInTier)
{
    static const char* kRankNames[9] = {
        "九品", "八品", "七品", "六品", "五品",
        "四品", "三品", "二品", "一品"
    };
    return kRankNames[qBound(0, levelInTier, 8)];
}

// 段内第 k 级的字数下限
static qint64 tierLevelMin(qint64 span, int k)
{
    if (span <= 0 || k <= 0) return 0;
    const double ratio = std::pow((k + 1) / 9.0, 1.5);
    return static_cast<qint64>(span * ratio);
}

int LevelSystem::globalLevel(qint64 totalChars)
{
    const auto& scheme = currentScheme();
    int lo = 0, hi = kTierCount - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (scheme.tiers[mid].minChars <= totalChars) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    int tierIdx = lo;

    qint64 offset = totalChars - scheme.tiers[tierIdx].minChars;
    qint64 span = scheme.tiers[tierIdx].spanChars;
    int levelInTier = 0;
    if (span > 0) {
        for (int k = 0; k < kLevelsPerTier; ++k) {
            if (offset >= tierLevelMin(span, k)) levelInTier = k;
            else break;
        }
    } else {
        levelInTier = kLevelsPerTier - 1;
    }

    return tierIdx * kLevelsPerTier + levelInTier;
}

LevelInfo LevelSystem::levelForTotal(qint64 totalChars)
{
    const auto& scheme = currentScheme();
    LevelInfo info{};

    int globalLv = globalLevel(totalChars);
    info.tierIndex = globalLv / kLevelsPerTier;
    info.levelInTier = globalLv % kLevelsPerTier;

    const TierDef& tier = scheme.tiers[info.tierIndex];
    info.tierName = QString::fromUtf8(tier.tierName);
    info.tierShort = QString::fromUtf8(tier.shortName);
    info.tierColor = QColor(QString::fromLatin1(tier.colorHex));
    info.tierRank = QString::fromUtf8(rankName(info.levelInTier));

    if (tier.spanChars <= 0) {
        info.currentMin = tier.minChars;
        info.nextMin = tier.minChars;
        info.isMax = true;
        info.progress = 1.0;
    } else {
        info.currentMin = tier.minChars + tierLevelMin(tier.spanChars, info.levelInTier);
        if (info.levelInTier < kLevelsPerTier - 1) {
            info.nextMin = tier.minChars + tierLevelMin(tier.spanChars, info.levelInTier + 1);
            info.isMax = false;
        } else {
            if (info.tierIndex < kTierCount - 1) {
                info.nextMin = scheme.tiers[info.tierIndex + 1].minChars;
            } else {
                info.nextMin = info.currentMin;
            }
            info.isMax = (info.tierIndex == kTierCount - 1);
        }
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