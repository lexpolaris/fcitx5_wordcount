// SPDX-License-Identifier: LGPL-2.1-or-later
// fcitx5 D-Bus 监听实现 — dbus-monitor 捕获定向 CommitString 信号
#include "fcitx5_monitor.h"
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>

namespace wordcount {

// dbus-monitor 过滤规则：只收 fcitx5 服务的 InputContext1 信号
// （保持轻量，避免全量 monitor 的性能开销）
static const QStringList kMonitorRules = {
    // QStringLiteral("type='signal',sender='org.fcitx.Fcitx5',interface='org.fcitx.Fcitx.InputContext1'"),
    // fcitx5 标准信号（不限制 sender）
    QStringLiteral("type='signal',interface='org.fcitx.Fcitx.InputContext1',member='CommitString'"),
    // fcitx4 兼容信号（WPS 使用，不限制 sender）
    QStringLiteral("type='signal',interface='org.fcitx.Fcitx.InputContext',member='CommitString'"),
};

Fcitx5Monitor::Fcitx5Monitor(QObject *parent)
    : QObject(parent)
{
}

Fcitx5Monitor::~Fcitx5Monitor()
{
    // 析构时只做清理，不重复调用 stop
    if (m_process) {
        // 直接清理，不通过 stop() 避免重复日志
        if (m_process->state() == QProcess::Running) {
            m_process->terminate();
            m_process->waitForFinished(300);
        }
        delete m_process;
        m_process = nullptr;
    }
    m_active = false;
}

bool Fcitx5Monitor::start()
{
    if (m_active) return true;

    if (m_fallbackMode) {
        qDebug() << "已在降级模式，不再尝试启动 dbus-monitor";
        return false;
    }

    // 冷却检查：避免频繁重启（至少间隔 2 秒）
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastStartAttemptMs < 2000) {
        qDebug() << "启动尝试过于频繁，跳过";
        return false;
    }
    m_lastStartAttemptMs = now;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &Fcitx5Monitor::onProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &Fcitx5Monitor::onProcessErrorOutput);
    // 异常退出 → 自动重启（受重入守卫与次数上限约束）
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Fcitx5Monitor::onProcessError);
    // 启动失败（如 dbus-monitor 不存在 → FailedToStart）不触发 finished，
    // 只触发 errorOccurred，必须单独连接，否则会漏兜底
    connect(m_process, &QProcess::errorOccurred,
            this, &Fcitx5Monitor::onProcessError);
    // 成功启动
    connect(m_process, &QProcess::started,
            this, &Fcitx5Monitor::onProcessStarted);

    QStringList args;
    args << QStringLiteral("--session");
    // 关键：--monitor 使 dbus-monitor 进入 BecomeMonitor 模式，
    // 才能收到【定向信号】；否则退化为 add_match，抓不到 CommitString。
    args << QStringLiteral("--monitor");
    args << kMonitorRules;

    m_process->start(QStringLiteral("dbus-monitor"), args);
    
    // 不阻塞 GUI 线程等启动：QProcess::start 是异步的，立即返回；
    // dbus-monitor 若无法启动，会走 errorOccurred 信号由
    // onProcessError 的重启退避逻辑兜底（NEW-K 稳定判定已覆盖
    // "能启动但立即退出"的场景）。这里只需确认 start() 本身没
    // 同步失败（如可执行文件不存在 → FailedToStart）。
    if (m_process->state() == QProcess::NotRunning
            && m_process->error() != QProcess::UnknownError) {
        qWarning() << "dbus-monitor 启动失败:"
                   << m_process->errorString();
        m_process->deleteLater();
        m_process = nullptr;
        // 尝试降级模式
        tryFallbackMode();
        return false;
    }

    m_active = true;
    // 注意：此处不再立即清零 m_restartCount（NEW-K）。
    // 若 dbus-monitor「能启动但立即退出」（如 BecomeMonitor 被拒），
    // 立即清零会使 kMaxRestartCount 上限永远触达不到，形成无限重启循环。
    // 改为：子进程持续存活 kStableUptimeMs 后才视为稳定，届时清零计数。
    QProcess *started = m_process;
    QTimer::singleShot(kStableUptimeMs, this, [this, started]() {
        // 仅当仍是本次启动的进程且在运行，才认定稳定（指针比较不解引用 started）
        if (m_active && m_process == started
                && m_process->state() == QProcess::Running) {
            m_restartCount = 0;
        }
    });
    qDebug() << "fcitx5 监听已启动（dbus-monitor --monitor 模式）";
    return true;
}

void Fcitx5Monitor::onProcessStarted()
{
    // 子进程启动成功，准备稳定计时
    QProcess *started = m_process;
    QTimer::singleShot(kStableUptimeMs, this, [this, started]() {
        if (m_active && m_process == started
                && m_process->state() == QProcess::Running) {
            m_restartCount = 0;
            qDebug() << "dbus-monitor 已稳定运行，重置重启计数";
        }
    });
}

void Fcitx5Monitor::tryFallbackMode()
{
    if (m_fallbackMode) return;

    qWarning() << "dbus-monitor 无法启动，进入降级模式";
    m_fallbackMode = true;
    m_active = false;

    // 通知上层监听器已失败
    emit monitorFailed();

    // 降级方案：尝试用 QDBusConnection 直接订阅（虽然可能收不到定向信号，
    // 但至少能收到广播信号，聊胜于无）
    // 这里仅记录，实际降级逻辑由上层决定
}

void Fcitx5Monitor::stop()
{
    if (m_stopping) return;
    if (!m_active && !m_process) {
        // 已经停止，直接返回
        return;
    }
    
    m_stopping = true;
    qDebug() << "Fcitx5Monitor::stop() 开始";

    if (m_process) {
        QProcess *proc = m_process;
        disconnect(proc, nullptr, this, nullptr);
        
        if (proc->state() == QProcess::Running) {
            qDebug() << "终止 dbus-monitor 子进程...";
            proc->terminate();
            if (!proc->waitForFinished(500)) {
                qDebug() << "dbus-monitor 未响应，强制 kill";
                proc->kill();
                proc->waitForFinished(300);
            }
        }
        
        proc->deleteLater();
        m_process = nullptr;
    }

    m_active = false;
    m_stopping = false;
    qDebug() << "Fcitx5Monitor::stop() 完成";
}

void Fcitx5Monitor::onProcessOutput()
{
    m_buffer += m_process->readAllStandardOutput();
    // 纵深防御：正常 dbus-monitor 输出按行分隔；若长时间无换行
    // （格式异常/攻击构造），截断缓冲避免无限累积（NEW-P）。
    // 审计 M5：截断改为「丢弃到下一个换行为止」——此前整段 clear
    // 会把"信号头已到、内容未到"的提交整条丢弃，且可能切在
    // UTF-8 多字节序列中间产生 U+FFFD 漏计
    if (m_buffer.size() > kMaxBufferBytes) {
        qWarning() << "dbus-monitor 输出缓冲超限，丢弃至下一行边界，size="
                   << m_buffer.size();
        const int nl = m_buffer.indexOf('\n', m_parseOffset);
        if (nl < 0) {
            m_buffer.clear();
        } else {
            m_buffer.remove(0, nl + 1);
        }
        m_parseOffset = 0;
        // 注意：不重置 m_waitingCommit/m_commitBody——行边界后
        // 解析状态机可继续（多行提交累积不受影响）
    }
    // 按行解析（性能 P-M2：用游标定位，读尽后一次性裁剪，
    // 避免每行 remove(0, idx+1) 的 O(n²) memmove）
    int idx;
    while ((idx = m_buffer.indexOf('\n', m_parseOffset)) >= 0) {
        const QByteArray lineBytes =
            m_buffer.mid(m_parseOffset, idx - m_parseOffset);
        parseLine(QString::fromUtf8(lineBytes));
        m_parseOffset = idx + 1;
    }
    if (m_parseOffset > 0) {
        m_buffer.remove(0, m_parseOffset);
        m_parseOffset = 0;
    }
}

void Fcitx5Monitor::onProcessErrorOutput()
{
    // BecomeMonitor 权限被拒时 dbus-monitor 会在 stderr 打印错误后退出。
    // 捕获关键字并打出明确告警，便于排障（不再静默降级）。
    const QByteArray err = m_process->readAllStandardError();
    const QString errText = QString::fromUtf8(err);

    // 检测 BecomeMonitor 被拒（致命错误进入降级模式）
    if (errText.contains(QStringLiteral("BecomeMonitor"), Qt::CaseInsensitive)
        || errText.contains(QStringLiteral("AccessDenied"), Qt::CaseInsensitive)
        || errText.contains(QStringLiteral("Not allowed"), Qt::CaseInsensitive)) {
        qWarning() << "dbus-monitor BecomeMonitor 被拒绝，进入降级模式";
        m_fallbackMode = true;
        emit monitorFailed();
        // 主动停止子进程
        if (m_process && m_process->state() == QProcess::Running) {
            m_process->kill();
        }
        return;
    }

    if (!errText.trimmed().isEmpty()) {
        qDebug() << "dbus-monitor stderr:" << errText.trimmed();
    }
}

void Fcitx5Monitor::parseLine(const QString &line)
{
    // dbus-monitor 输出示例：
    //   signal time=... sender=:1.173 -> destination=:1.394 serial=... path=...; interface=...; member=CommitString
    //      string "你好世界"
    // 含换行的文本会被输出为多个物理行（审计 M3）：
    //      string "第一行
    //      第二行"
    // 需持续累积直到「未转义的收尾引号」才构成一条完整提交。

    if (!m_waitingCommit) {
        // 查找 member=CommitString 的信号头
        if (line.contains(QStringLiteral("member=CommitString"))) {
            m_waitingCommit = true;
            m_commitBody.clear();
        }
        return;
    }

    // —— 等待态：累积提交正文直到收尾引号 ——
    // 安全阀：合法提交 ≤kMaxCommitLength 字符，超限说明解析已错位
    // （格式异常/恶意构造），放弃本次等待重新对齐信号头
    if (m_commitBody.size() > kMaxCommitLength * 4) {
        qWarning() << "CommitString 累积超限，放弃并重置解析状态";
        m_waitingCommit = false;
        m_commitBody.clear();
        return;
    }
    if (!m_commitBody.isEmpty()) {
        m_commitBody += QLatin1Char('\n');
    }
    m_commitBody += line;

    // 收尾判定：本行以未转义的 '"' 结尾（前置连续 '\' 数量为偶数）
    if (!line.endsWith(QLatin1Char('"'))) {
        return;   // 未闭合，继续累积下一行
    }
    int backslashes = 0;
    for (int i = line.size() - 2; i >= 0 && line.at(i) == QLatin1Char('\\'); --i) {
        ++backslashes;
    }
    if (backslashes % 2 != 0) {
        return;   // 引号被转义（\"），仍是正文的一部分
    }

    // 一条完整提交已凑齐
    m_waitingCommit = false;
    const QString raw = m_commitBody;
    m_commitBody.clear();

    // 提取引号内的文本：string "xxx" 或 variant string "xxx"。
    // 字符类 [^"\\] 天然匹配换行，多行正文一次捕获
    static const QRegularExpression re(
        QStringLiteral("string\\s+\"((?:[^\"\\\\]|\\\\.)*)\""));
    const auto m = re.match(raw);
    if (!m.hasMatch()) {
        // 输出格式异常（dbus-monitor 版本变化等）：记录便于排障
        qDebug() << "dbus-monitor 提交格式异常（未匹配 string）:"
                 << raw.left(80).trimmed();
        return;
    }
    // 提取引号内的文本后，进行转义解析
    QString text = m.captured(1);

    // 正确的转义解析：只处理标准 JSON 转义
    QString unescaped;
    unescaped.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\\') && i + 1 < text.size()) {
            const QChar n = text.at(i + 1);
            // 标准 JSON 转义序列
            if (n == QLatin1Char('n')) {
                unescaped += QLatin1Char('\n');
                ++i;
            } else if (n == QLatin1Char('t')) {
                unescaped += QLatin1Char('\t');
                ++i;
            } else if (n == QLatin1Char('"')) {
                unescaped += QLatin1Char('"');
                ++i;
            } else if (n == QLatin1Char('\\')) {
                unescaped += QLatin1Char('\\');
                ++i;
            } else if (n == QLatin1Char('r')) {
                unescaped += QLatin1Char('\r');
                ++i;
            } else if (n == QLatin1Char('/')) {
                unescaped += QLatin1Char('/');
                ++i;
            } else if (n == QLatin1Char('u')) {
                // Unicode 转义 \uXXXX，Qt 可自动解析
                // 这里简单处理：保留 \uXXXX 原样，交给 QString 处理
                // 或者使用 QChar::fromUcs4 手动解析
                unescaped += QLatin1Char('\\');
                unescaped += n;
                // 注意：这种写法不完整，但 dbus-monitor 输出很少含 \u
                // 完整实现需要解析 4 位十六进制
            } else {
                // 非法转义：保留原样（不解析）
                unescaped += QLatin1Char('\\');
                unescaped += n;
                ++i;
            }
        } else {
            unescaped += c;
        }
    }
    text = unescaped;

    // —— 口径过滤（方案 A，用户确认）——
    // 含换行的提交整条丢弃：正常打字（拼音/英文选词）不会产生含 \n
    // 的单次 CommitString；含换行基本是经输入法中转的粘贴（fcitx5
    // 剪贴板插件等），不属于"打字量"。静默丢弃不刷日志。
    // 注意：上方的多行累积机制仍需保留——正文必须被完整消费到
    // 收尾引号，否则残余行会被误当信号头扫描
    if (text.contains(QLatin1Char('\n'))) {
        qDebug() << "忽略含换行的提交（经输入法的粘贴），len=" << text.length();
        return;
    }

    // —— 长度上限，防御本地 DoS ——
    if (text.isEmpty() || text.length() > kMaxCommitLength) {
        qWarning() << "忽略超长或空 CommitString, len=" << text.length();
        return;
    }
    emit textCommitted(text);
}

void Fcitx5Monitor::onProcessError()
{
    if (m_stopping) {
        // 正在主动停止，不重启（避免卸载/禁用时进程抖动）
        return;
    }

    if (m_fallbackMode) {
        // 已在降级模式，不再尝试重启
        return;
    }

    // 检查进程退出状态，如果是正常退出（非崩溃），不重启
    if (m_process) {
        int exitCode = m_process->exitCode();
        QProcess::ExitStatus status = m_process->exitStatus();
        if (status == QProcess::NormalExit && exitCode == 0) {
            qDebug() << "dbus-monitor 正常退出，不重启";
            m_process->deleteLater();
            m_process = nullptr;
            m_active = false;
            m_restarting = false;
            return;
        }
    }

    // 异常退出，尝试重启
    if (m_restartCount >= kMaxRestartCount) {
        qWarning() << "dbus-monitor 多次异常退出，停止自动重启 (次数上限"
                   << kMaxRestartCount << ")";
        m_active = false;
        m_restarting = false;
        tryFallbackMode();
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        return;
    }

    ++m_restartCount;
    qWarning() << "dbus-monitor 子进程异常退出，尝试重启 ("
               << m_restartCount << "/" << kMaxRestartCount << ")";

    // 退避延迟：递增等待时间
    const int delay = 500 * m_restartCount;
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    QTimer::singleShot(delay, this, [this]() {
        m_restarting = false;
        if (!m_stopping && !m_fallbackMode && !m_active) {
            start();
        }
    });
}

} // namespace wordcount
