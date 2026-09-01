// SPDX-License-Identifier: LGPL-2.1-or-later
// 敲字速度追踪实现
#include "speed_tracker.h"

namespace wordcount {

    namespace {
        /// 取单调毫秒：外部未显式传入时用内部单调钟（首次调用自动启动）
        inline qint64 monoMs(QElapsedTimer &clock) {
            if (!clock.isValid()) {
                clock.start();
            }
            return clock.elapsed();
        }
    } // namespace

    void SpeedTracker::evict(qint64 nowMs) {
        while (!m_window.isEmpty() && nowMs - m_window.head().first > kWindowMs) {
            m_window.dequeue();
        }
    }

    void SpeedTracker::onCommit(qint64 chars, qint64 nowMs) {
        if (chars <= 0) return;
        if (nowMs <= 0) {
            nowMs = monoMs(m_clock);
        }
        m_window.enqueue(qMakePair(nowMs, chars));
        evict(nowMs);
    }

    double SpeedTracker::wpm(qint64 nowMs) {
        if (nowMs <= 0) {
            nowMs = monoMs(m_clock);
        }
        // 读取路径也裁剪过期样本，保证空闲后速度平滑归零而非骤降到 0
        evict(nowMs);

        // 如果窗口非空，计算真实速度并更新缓存
        if (!m_window.isEmpty()) {
            qint64 total = 0;
            for (const auto &sample : m_window) {
                total += sample.second;
            }
            // 窗口 60s = 1min，所以 total = 字/分
            m_lastValidWpm = static_cast<double>(total);
            return m_lastValidWpm;
        }

        // 窗口为空，返回最后一次有效速度
        return m_lastValidWpm;
    }

    void SpeedTracker::clear() {
        m_window.clear();
        // 重置全部时清零
        m_lastValidWpm = 0.0;
    }

    int SpeedTracker::peakSpeed() const
    {
        // 简单实现：返回当前窗口速度（已有 wpm 方法）
        // 如果要真正的10s峰值，需要单独维护10s窗口
        // 这里先复用 wpm 作为近似
        return static_cast<int>(m_lastValidWpm);
    }

} // namespace wordcount
