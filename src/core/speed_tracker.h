// SPDX-License-Identifier: LGPL-2.1-or-later
// 敲字速度追踪 — 60 秒滚动窗口
// 定义：每分钟上屏字数，基于最近 60 秒的提交样本
#ifndef WORDCOUNT_SPEED_TRACKER_H
#define WORDCOUNT_SPEED_TRACKER_H

#include <QQueue>
#include <QPair>
#include <QElapsedTimer>
#include <qglobal.h>

namespace wordcount {

    class SpeedTracker {
    public:
        static constexpr qint64 kWindowMs = 60'000;   // 60s 滚动窗口

        /// 记录一次提交
        /// \param chars 本次提交字数
        /// \param nowMs 当前单调毫秒，传 0 则自动取内部单调时钟
        void onCommit(qint64 chars, qint64 nowMs = 0);

        /// 返回当前敲字速度（字/分）
        /// 如果窗口为空，返回最后一次有效速度（永不归零）
        /// \param nowMs 当前单调毫秒，传 0 自动取
        /// \return 速度；从未打过字返回 0.0
        double wpm(qint64 nowMs = 0);

        /// 清空窗口（用于插件禁用/重置全部）
        void clear();

        int peakSpeed() const;  // 返回10s窗口内的峰值速度（字/分）

    private:
        /// 淘汰窗口外（超过 60s）的样本
        void evict(qint64 nowMs);

        /// 单调时钟基准（进程内递增，不受系统时间调整影响）
        QElapsedTimer m_clock;

        QQueue<QPair<qint64, qint64>> m_window;  // (monotonic_ms, chars)

        /// ⭐ 最后一次有效速度（窗口非空时计算的值）
        /// 窗口为空时返回此值，永不归零
        double m_lastValidWpm = 0.0;
    };

} // namespace wordcount

#endif // WORDCOUNT_SPEED_TRACKER_H
