// SPDX-License-Identifier: LGPL-2.1-or-later
// fcitx5 D-Bus 监听 — 通过 dbus-monitor（monitor 模式）捕获 CommitString
//
// 为什么不用 QDBusConnection::connect 直接订阅？
//   实测 fcitx5 5.1.12 的 InputContext1 信号（CommitString/CurrentIM/
//   UpdateFormattedPreedit 等）是【定向信号】：sender 把信号发往
//   destination（创建该输入上下文的应用连接），而不是广播。
//   普通订阅（connect/addMatch）即使路径、接口、信号名完全正确也
//   收不到定向信号（20 秒实测 0 信号；dbus-monitor 却能捕获）。
//
// 解决方案：
//   dbus-monitor 进程运行在 D-Bus monitor 模式（BecomeMonitor），
//   能收到所有消息包括定向信号。本类用 QProcess 启动
//   dbus-monitor 过滤 fcitx5 的 InputContext1 信号，解析 stdout
//   中的 CommitString 文本。
//
// 安全：
//   - 只读监听公开信号，不调用 ProcessKeyEvent 等写入接口；
//   - 不修改 fcitx5 配置；不安装 addon；纯只读
//   - CommitString 文本只用于统计字数，不存储原文、不上传
#ifndef WORDCOUNT_FCITX5_MONITOR_H
#define WORDCOUNT_FCITX5_MONITOR_H

#include <QObject>
#include <QProcess>
#include <QByteArray>

namespace wordcount {

class Fcitx5Monitor : public QObject {
    Q_OBJECT
public:
    /// 单次上屏文本最大长度（字符）；超出直接丢弃，防御本地 DoS
    static constexpr int kMaxCommitLength = 65536;

    /// 子进程异常退出后的最大自动重启次数（防御无限重启循环）
    static constexpr int kMaxRestartCount = 5;

    /// 子进程持续存活该时长后才认为"稳定"，允许清零重启计数；
    /// 防止"能启动但立即退出"（如 BecomeMonitor 被拒）场景下
    /// start() 即清零导致重启上限永远触达不到 → 无限拉起循环
    static constexpr int kStableUptimeMs = 10'000;

    /// stdout 行缓冲上限（字节）：无换行时不无限累积（纵深防御）
    static constexpr int kMaxBufferBytes = 1'048'576; // 1 MiB

    explicit Fcitx5Monitor(QObject *parent = nullptr);
    ~Fcitx5Monitor() override;

    /// 启动监听。返回是否成功启动 dbus-monitor 子进程。
    bool start();

    /// 停止监听并终止子进程。
    void stop();

signals:
    /// 输入法上屏文本信号
    /// \param text 上屏文本（明文，接收方应只统计字数，不存储原文）
    void textCommitted(const QString &text);

private slots:
    /// dbus-monitor stdout 有数据
    void onProcessOutput();

    /// dbus-monitor stderr 有数据（检查 BecomeMonitor 失败告警）
    void onProcessErrorOutput();

    /// 子进程异常退出
    void onProcessError();

private:
    /// 解析 dbus-monitor 输出行，提取 CommitString 文本
    /// \param line 当前行
    void parseLine(const QString &line);

    bool m_active = false;

    /// dbus-monitor 子进程
    QProcess *m_process = nullptr;

    /// stdout 缓冲（按行解析）
    QByteArray m_buffer;

    /// 解析游标（性能 P-M2：此前每行 m_buffer.remove(0, idx+1) 导致
    /// 剩余缓冲整体 memmove，最坏 O(n²)；改游标定位，读尽后一次性裁剪）
    int m_parseOffset = 0;

    /// 上一行是 member=CommitString，等待取 string 内容
    /// （审计 M3：含换行的上屏文本由 dbus-monitor 输出为多物理行，
    /// 需累积直到未转义的收尾引号才构成完整提交）
    bool m_waitingCommit = false;

    /// 多行 CommitString 累积缓冲（与 m_waitingCommit 配套）
    QString m_commitBody;

    /// 重入守卫：stop() 进行中置位，阻止 onProcessError 重启
    bool m_stopping = false;

    /// 防重入：QProcess 异常退出时 errorOccurred 与 finished 双信号
    /// 都会触发 onProcessError，不加守卫会重启计数翻倍、排程两次重启
    bool m_restarting = false;

    /// 子进程异常退出后的累计重启次数
    int m_restartCount = 0;
};

} // namespace wordcount

#endif // WORDCOUNT_FCITX5_MONITOR_H
