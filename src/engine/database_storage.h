// src/engine/database_storage.h
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMutex>
#include <QString>
#include <QVector>
#include <array>
#include <qglobal.h>
#include <QJsonObject>

namespace wordcount {

/**
 * @brief SQLite 存储后端，替代 JsonStorage
 * 
 * 表结构：
 *   commits (ts, date, hour, chars, len_cat)
 *   daily_agg (date, total_chars, cnt_commits, cnt_1, cnt_2, cnt_3, cnt_4, cnt_5plus)
 *   meta (key, value)
 * 
 * 线程安全：所有公开方法均持有互斥锁。
 */
class DatabaseStorage : public QObject {
    Q_OBJECT
public:
    explicit DatabaseStorage(const QString& dbPath, QObject* parent = nullptr);
    ~DatabaseStorage();

    /// 初始化：创建表、设置 PRAGMA、迁移旧数据（如果存在）
    bool init();

    // ---------- 写入 ----------
    /// 插入一条提交记录（ts: 毫秒时间戳, date: "YYYY-MM-DD", hour: 0-23,
    /// chars: 字数, lenCat: 1-5）
    bool addCommit(qint64 ts, const QString& date, int hour, int chars, int lenCat);

    /// 批量插入多条记录（用于 flush 时一次性写入）
    bool batchAddCommits(const QVector<QPair<qint64, QString>> &records); 
    // 先不实现复杂的批量，简单循环即可，在外部控制事务

    /// 更新或插入当天的聚合数据
    bool upsertDailyAgg(const QString& date,
                        qint64 totalChars,
                        int cntCommits,
                        int cnt1, int cnt2, int cnt3, int cnt4, int cnt5plus);

    // ---------- 读取 ----------
    /// 获取总字数（从 meta 表读取）
    qint64 getTotal() const;

    /// 获取今日字数（从 daily_agg 读取）
    qint64 getToday(const QString& date) const;

    /// 获取当日 24 小时分布（从 commits 按小时聚合）
    std::array<qint64, 24> getHourly(const QString& date) const;

    /// 获取近 7 天每日聚合（从 daily_agg 读取）
    std::array<qint64, 7> getDaily7(const QString& today) const;

    /// 获取当日字词分布（从 daily_agg 读取）
    bool getDist(const QString& date, int& cnt1, int& cnt2, int& cnt3, int& cnt4, int& cnt5plus) const;

    /// 更新 meta 总字数
    bool setTotal(qint64 total);

    // ---------- 导出/导入 ----------
    /// 导出所有数据为 JSON 对象
    QJsonObject exportAll() const;

    /// 从 JSON 对象导入数据（会清空现有数据）
    /// \param data JSON 对象
    /// \param errorMsg 错误信息
    /// \return 是否成功
    bool importAll(const QJsonObject& data, QString* errorMsg = nullptr);

    // ---------- 维护 ----------
    /// 清理旧数据（保留最近 N 个月），注意：该操作耗时会锁表，建议空闲时执行
    bool cleanupOldRecords(int monthsToKeep = 12);

    /// 执行 VACUUM 回收空间（排他操作，建议空闲时执行）
    bool vacuum();

    /// 打开/关闭连接（一般不用手动）
    bool open();
    void close();

    /// 获取数据库路径
    QString getDbPath() const { return m_dbPath; }

    /// 获取数据库连接（仅供内部使用）
    QSqlDatabase getDatabase() const { return m_db; }

    /// 确认数据库是否打开
    bool isOpen() const { return m_db.isOpen(); }


private:
    bool createTables();
    bool ensureMetaTable();
    bool setMeta(const QString& key, qint64 value);
    qint64 getMeta(const QString& key, qint64 defaultValue = 0) const;

    QString m_dbPath;
    mutable QMutex m_mutex;
    QSqlDatabase m_db;
    bool m_initialized = false;
};

} // namespace wordcount