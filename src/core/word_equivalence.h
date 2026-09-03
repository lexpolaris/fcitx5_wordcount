// SPDX-License-Identifier: LGPL-2.1-or-later
// 「约等于一部作品」类比 — 根据总字数匹配经典作品
// 例如总字数接近 82 万时提示「约等于写了一部《西游记》」
#ifndef WORDCOUNT_WORD_EQUIVALENCE_H
#define WORDCOUNT_WORD_EQUIVALENCE_H

#include <QString>
#include <QVector>
#include <qglobal.h>

namespace wordcount {
    struct TierScheme;
}

namespace wordcount {

struct WorkRef {
    const char *name;   // 作品名（UTF-8）
    qint64 chars;       // 参考字数
};

class WordEquivalence {
public:
    /// 根据总字数返回类比文案，如「约等于写了一部《西游记》」
    static QString describe(qint64 totalChars);

private:
    /// 参考作品表（按字数升序）— 仅 describe() 内部使用（审计：降为 private）
    static const QVector<WorkRef>& works();
};

} // namespace wordcount

#endif // WORDCOUNT_WORD_EQUIVALENCE_H
