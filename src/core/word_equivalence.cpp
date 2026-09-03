// SPDX-License-Identifier: LGPL-2.1-or-later
// 「约等于一部作品」类比实现
#include "word_equivalence.h"
#include "level_system.h"
#include <algorithm>

namespace wordcount {

// 经典作品参考字数（约数，按字数升序，含中外名著）
// 用户总字数达到某作品字数时，提示"约等于写了该作品"
static const WorkRef kWorks[] = {
    // === 百字级 ===
    { "《陋室铭》",       80 },
    { "《爱莲说》",       120 },
    { "《马说》",         150 },

    // === 二百~三百字级 ===
    { "《狼》",           200 },
    { "《桃花源记》",     300 },

    // === 三百~五百字级 ===
    { "《岳阳楼记》",     400 },
    { "《论语·学而》",    500 },

    // === 六百~八百字级 ===
    { "《论语·为政》",    600 },
    { "《论语·子罕》",    800 },

    // === 九百~千字级 ===
    { "《论语·先进》",    900 },
    { "《千字文》",       1000 },
    { "《论语·宪问》",    1400 },

    // === 进阶 ===
    { "《大学》",         1700 },
    { "《中庸》",         3500 },
    { "《道德经》",       5000 },
    { "《孙子兵法》",     6000 },

    // === 长篇 ===
    { "《唐诗三百首》",   20000 },
    { "《小王子》",       25000 },
    { "《老人与海》",     27000 },
    { "《边城》",         50000 },
    { "《呐喊》",         100000 },
    { "《活着》",         120000 },
    { "《围城》",         250000 },
    { "《百年孤独》",     260000 },
    { "《双城记》",       350000 },
    { "《战争与和平》",   580000 },
    { "《悲惨世界》",     650000 },
    { "《三国演义》",     730000 },
    { "《西游记》",       820000 },
    { "《红楼梦》",       870000 },
    { "《水浒传》",       960000 },
    { "《追忆似水年华》", 1200000 },
};

const QVector<WorkRef>& WordEquivalence::works()
{
    static QVector<WorkRef> s_works;
    if (s_works.isEmpty()) {
        for (const WorkRef &w : kWorks) {
            // 仅丢弃异常小的占位项（< 50 字无实际意义）
            if (w.chars < 50) continue;
            s_works.append(w);
        }
        // 按字数升序排序（保证二分/线性匹配正确）
        std::sort(s_works.begin(), s_works.end(),
                  [](const WorkRef &a, const WorkRef &b) {
                      return a.chars < b.chars;
                  });
    }
    return s_works;
}

QString WordEquivalence::describe(qint64 totalChars)
{
    // 获取当前方案名称
    const auto& scheme = LevelSystem::currentScheme();
    QString schemeName = QString::fromUtf8(scheme.name);
    
    // 只在"文人雅称"和"科举制"方案下显示作品类比
    if (schemeName != "文人雅称" && schemeName != "科举制") {
        return QString();
    }

    const QVector<WorkRef> &list = works();
    if (list.isEmpty()) {
        return QString();
    }

    // 找到字数 <= totalChars 的最后一个作品（即"已达到"的最长作品）
    const WorkRef *reached = nullptr;
    for (const WorkRef &w : list) {
        if (totalChars >= w.chars) {
            reached = &w;
        } else {
            break;
        }
    }

    if (!reached) {
        // 总字数还不到最短作品（动态取表中最短条目，如《爱莲说》100 字）
        const WorkRef &first = list.first();
        return QStringLiteral("继续书写，向%1（约%2字）进发")
            .arg(QString::fromUtf8(first.name))
            .arg(first.chars);
    }

    // 已达到某作品：若还差一半以内就够下一部，提示下一部
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].chars != reached->chars) continue;
        // reached 是 list[i]
        if (i + 1 < list.size()) {
            const WorkRef &next = list[i + 1];
            // 如果已超过下一部的一半，提示"接近"
            if (totalChars >= next.chars / 2 && totalChars < next.chars) {
                return QStringLiteral("约等于写了 %1\n正接近 %2")
                    .arg(QString::fromUtf8(reached->name))
                    .arg(QString::fromUtf8(next.name));
            }
        }
        break;
    }

    return QStringLiteral("约等于写了 %1")
        .arg(QString::fromUtf8(reached->name));
}

} // namespace wordcount
