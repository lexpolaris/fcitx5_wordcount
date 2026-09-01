// SPDX-License-Identifier: LGPL-2.1-or-later
// 字数统计算法 — 标准口径
// 口径：中文每字 1，英文每词 1，数字串每串 1，标点/空格/控制字符不计，emoji 暂不计
#ifndef WORDCOUNT_CHAR_COUNTER_H
#define WORDCOUNT_CHAR_COUNTER_H

#include <QString>

namespace wordcount {

class CharCounter {
public:
    /// 按标准口径统计字数
    /// \param text 上屏文本（可能含中英文混排、标点、emoji）
    /// \return 字数（中文每字 1，英文每词 1，数字串每串 1）
    static qint64 count(const QString &text);

    /// 带配置的计数
    /// \param countPunctuation 是否统计标点（默认 false）
    /// \param countEmoji 是否统计 emoji（默认 false）
    static qint64 count(const QString &text, bool countPunctuation, bool countEmoji);

private:
    /// 判断码点是否为 CJK 文字（含中日韩统一汉字、假名、谚文、扩展区）
    static bool isCJK(uint ucs4);

    /// 判断码点是否为 emoji / 符号（Symbols & Pictographs 等）
    static bool isEmoji(uint ucs4);

    /// 判断字符是否为"单词字符"（字母、数字、撇号、连字符）
    /// 用于英文单词计数
    static bool isWordChar(QChar ch);

    /// 判断字符是否为标点
    static bool isPunctuation(QChar ch);
};

} // namespace wordcount

#endif // WORDCOUNT_CHAR_COUNTER_H
