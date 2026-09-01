// SPDX-License-Identifier: LGPL-2.1-or-later
// 字数统计算法实现 — 标准口径
#include "char_counter.h"

namespace wordcount {

    bool CharCounter::isCJK(uint ucs4) {
        // CJK 统一汉字及扩展区（Unicode 15.1 完整版）
        return (ucs4 >= 0x4E00 && ucs4 <= 0x9FFF)    // CJK 统一汉字
            || (ucs4 >= 0x3400 && ucs4 <= 0x4DBF)    // CJK 扩展 A
            || (ucs4 >= 0x20000 && ucs4 <= 0x2A6DF)  // CJK 扩展 B
            || (ucs4 >= 0x2A700 && ucs4 <= 0x2B73F)  // CJK 扩展 C
            || (ucs4 >= 0x2B740 && ucs4 <= 0x2B81F)  // CJK 扩展 D
            || (ucs4 >= 0x2B820 && ucs4 <= 0x2CEAF)  // CJK 扩展 E
            || (ucs4 >= 0x2CEB0 && ucs4 <= 0x2EBEF)  // CJK 扩展 F
            || (ucs4 >= 0x30000 && ucs4 <= 0x3134F)  // CJK 扩展 G
            || (ucs4 >= 0x31350 && ucs4 <= 0x323AF)  // CJK 扩展 H
            || (ucs4 >= 0x2EBF0 && ucs4 <= 0x2EE5F)  // CJK 扩展 I (Unicode 15.1)
            // CJK 兼容表意
            || (ucs4 >= 0xF900 && ucs4 <= 0xFAFF)    // CJK 兼容表意
            || (ucs4 >= 0x2F800 && ucs4 <= 0x2FA1F)  // CJK 兼容表意补充
            // 康熙部首
            || (ucs4 >= 0x2F00 && ucs4 <= 0x2FDF)    // 康熙部首
            // 日文假名
            || (ucs4 >= 0x3040 && ucs4 <= 0x309F)    // 平假名
            || (ucs4 >= 0x30A0 && ucs4 <= 0x30FF)    // 片假名
            // 韩文谚文
            || (ucs4 >= 0xAC00 && ucs4 <= 0xD7AF)    // 谚文音节
            || (ucs4 >= 0x1100 && ucs4 <= 0x11FF)    // 谚文字母
            // CJK 部首补充
            || (ucs4 >= 0x2E80 && ucs4 <= 0x2EFF)    // CJK 部首补充
            // 表意文字描述字符
            || (ucs4 >= 0x2FF0 && ucs4 <= 0x2FFB)    // 表意文字描述字符
            // 圈中日文
            || (ucs4 >= 0x3200 && ucs4 <= 0x32FF)    // 带圈中日韩文字
            || (ucs4 >= 0x3300 && ucs4 <= 0x33FF);   // CJK 兼容
    }

    bool CharCounter::isEmoji(uint ucs4) {
        // Emoji 区间（不含 CJK，按 Unicode 标准 Emoji 分类）
        return (ucs4 >= 0x1F300 && ucs4 <= 0x1F9FF)   // 杂项符号与象形
            || (ucs4 >= 0x1FA00 && ucs4 <= 0x1FAFF)   // 扩展 Emoji
            || (ucs4 >= 0x2600 && ucs4 <= 0x27BF)     // 杂项符号 + 装饰符号
            || (ucs4 >= 0x2B50 && ucs4 <= 0x2B55);    // 附加杂项符号
    }

    bool CharCounter::isWordChar(QChar ch) {
        // CJK 字符（中文/日文/韩文）不是英文单词字符：
        // QChar::isLetterOrNumber() 对汉字也返回 true，若不排除会把
        // 整段中文都吞进一个"单词"，导致计数严重偏小
        if (isCJK(ch.unicode())) {
            return false;
        }
        // 字母、数字、撇号、连字符构成英文单词
        // 注意：QChar::isLetterOrNumber 不含撇号，需要单独判断
        return ch.isLetterOrNumber()
            || ch == QLatin1Char('\'')
            || ch == QLatin1Char('-')
            || ch == QChar(0x2019); // 右单引号（曲引号）
    }

    bool CharCounter::isPunctuation(QChar ch) {
        // Qt Unicode 分类：Punctuation_Connector(Pc) ~ Punctuation_Other(Po)
        auto cat = ch.category();
        return (cat >= QChar::Punctuation_Connector &&
                cat <= QChar::Punctuation_Other);
    }

    qint64 CharCounter::count(const QString &text) {
        return count(text, false, false);
    }

    qint64 CharCounter::count(const QString &text, bool countPunctuation, bool countEmoji) {
        qint64 n = 0;
        int i = 0;
        int size = text.size();

        while (i < size) {
            QChar ch = text.at(i);

            // 1. 空白与控制字符 → 不计
            if (ch.isSpace()) {
                i++;
                continue;
            }

            // 2. 标点 → 默认不计，配置开启才计
            if (isPunctuation(ch)) {
                if (countPunctuation) n++;
                i++;
                continue;
            }

            // 3. 处理代理对（emoji、扩展汉字）
            //    必须同时校验下一字符是低位代理，孤立高代理按普通字符处理，
            //    避免 surrogateToUcs4 产出非法码点导致计数失真（NEW-N）
            uint ucs4 = ch.unicode();
            bool isSurrogatePair = false;
            if (QChar::isHighSurrogate(ucs4) && i + 1 < size
                    && QChar::isLowSurrogate(text.at(i + 1).unicode())) {
                ucs4 = QChar::surrogateToUcs4(ucs4, text.at(i + 1).unicode());
                isSurrogatePair = true;
            }

            // 4. CJK 文字 → 每字 1
            if (isCJK(ucs4)) {
                n++;
                i += isSurrogatePair ? 2 : 1;
                continue;
            }

            // 5. Emoji → 默认不计，配置开启才计
            if (isEmoji(ucs4)) {
                if (countEmoji) n++;
                // emoji 可能由多个码点组成（ZWJ 序列），这里简化处理，跳过一个码点
                i += isSurrogatePair ? 2 : 1;
                continue;
            }

            // 6. 西文单词：连续单词字符算 1 词
            if (isWordChar(ch)) {
                int j = i;
                while (j < size && isWordChar(text.at(j))) j++;
                n++;
                i = j;
                continue;
            }

            // 7. 其他符号 → 不计
            i++;
        }

        return n;
    }

} // namespace wordcount
