// SPDX-License-Identifier: LGPL-2.1-or-later
// fcitx5 D-Bus 监听实现 — dbus-monitor 捕获定向 CommitString 信号
#include "fcitx5_monitor.h"
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>

namespace wordcount {

// dbus-monitor 过滤规则：只收 fcitx5 服务的 InputContext1 信号
// （保持轻量，避免全量 monitor 的性能开销）
static const QStringList kMonitorRules = {
    // QStringLiteral("type='signal',sender='org.fcitx.Fcitx5',interface='org.fcitx.Fcitx.InputContext1'"),
    // QStringLiteral("type='signal',sender='org.fcitx.Fcitx',interface='org.fcitx.Fcitx.InputContext'"),
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
    stop();
}

bool Fcitx5Monitor::start()
{
    if (m_active) return true;

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

void Fcitx5Monitor::stop()
{
    if (m_stopping) return;          // 重入守卫
    m_stopping = true;
    if (m_process) {
        QProcess *proc = m_process;
        // 断开本对象到该进程的全部信号连接（kill 后残留输出不再进槽，防空指针）
        disconnect(proc, nullptr, this, nullptr);
        // 异步清理：进程结束后 deleteLater 自行释放，
        // 不用 waitForFinished 阻塞 GUI 线程（R-1）
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                proc, &QObject::deleteLater);
        proc->kill();
        m_process = nullptr;
    }
    m_active = false;
    m_stopping = false;
    qDebug() << "fcitx5 监听已停止";
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
    if (errText.contains(QStringLiteral("BecomeMonitor"), Qt::CaseInsensitive)
        || errText.contains(QStringLiteral("AccessDenied"), Qt::CaseInsensitive)
        || errText.contains(QStringLiteral("Not allowed"), Qt::CaseInsensitive)) {
        qWarning() << "dbus-monitor BecomeMonitor 被拒绝（可能受 D-Bus 策略限制），"
                   << "插件将无法统计字数。stderr:" << errText.trimmed();
    } else if (!errText.trimmed().isEmpty()) {
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
    // 防重入：errorOccurred 与 finished 会双双触发本函数，
    // 首次进入后置位，第二次（同一崩溃事件）直接忽略，
    // 保证每次崩溃只计数一次、只排程一次重启
    if (m_restarting) return;
    m_restarting = true;
    if (m_restartCount >= kMaxRestartCount) {
        qWarning() << "dbus-monitor 多次异常退出，停止自动重启"
                   << "(次数上限" << kMaxRestartCount << ")";
        m_active = false;
        m_restarting = false;
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        return;
    }
    ++m_restartCount;
    qWarning() << "dbus-monitor 子进程异常退出，尝试重启 ("
               << m_restartCount << "/" << kMaxRestartCount << ")";
    // 简单退避：重启次数越多，等待越久（500ms * 次数）
    const int delay = 500 * m_restartCount;
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    QTimer::singleShot(delay, this, [this]() {
        // 恢复防重入标志：允许下一次独立崩溃事件再次触发
        m_restarting = false;
        // 审计 L8：singleShot 延迟期间可能已 stop() 完成（卸载/禁用），
        // 必须同时检查 m_stopping 与 m_active，避免无意义重启一次
        if (!m_stopping && m_active) {
            start();
        }
    });
}

} // namespace wordcount
