// src/engine/database_storage.cpp
#include "database_storage.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QDir>

namespace wordcount {

DatabaseStorage::DatabaseStorage(const QString& dbPath, QObject* parent)
    : QObject(parent), m_dbPath(dbPath)
{
    // 确保目录存在
    QFileInfo info(m_dbPath);
    QDir dir = info.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "无法创建数据库目录:" << dir.path();
        }
    }
}

DatabaseStorage::~DatabaseStorage()
{
    close();
}

bool DatabaseStorage::init()
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) return true;

    // 打开连接
    if (!open()) return false;

    // 创建表
    if (!createTables()) return false;

    // 确保 meta 表存在（已包含在 createTables 中）
    if (!ensureMetaTable()) return false;

    m_initialized = true;
    return true;
}

bool DatabaseStorage::open()
{
    if (m_db.isOpen()) return true;

    // 使用唯一的连接名，避免与其他地方冲突
    QString connName = QString("wordcount_conn_%1").arg(reinterpret_cast<quintptr>(this));
    
    m_db = QSqlDatabase::addDatabase("QSQLITE", connName);
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        qWarning() << "打开数据库失败:" << m_db.lastError().text();
        return false;
    }

    // 设置 PRAGMA (必须在连接打开后立即执行)
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL;");
    pragma.exec("PRAGMA synchronous=NORMAL;");
    pragma.exec("PRAGMA cache_size=-65536;");   // 64MB
    pragma.exec("PRAGMA temp_store=MEMORY;");
    pragma.exec("PRAGMA mmap_size=268435456;"); // 256MB

    // 外键约束（虽然本设计未使用）
    pragma.exec("PRAGMA foreign_keys=ON;");

    return true;
}

void DatabaseStorage::close()
{
    QMutexLocker locker(&m_mutex);

    if (m_db.isOpen()) {
        qDebug() << "关闭数据库连接...";
        m_db.close();
    }

    // 移除连接前，确保所有查询对象都已销毁
    // 使用连接名移除
    QString connName = m_db.connectionName();
    if (!connName.isEmpty()) {
        // 先让 QSqlDatabase 对象无效
        m_db = QSqlDatabase();
        // 然后移除连接
        QSqlDatabase::removeDatabase(connName);
        qDebug() << "数据库连接已移除:" << connName;
    }

    m_initialized = false;
}

bool DatabaseStorage::createTables()
{
    QSqlQuery query(m_db);

    // commits 表
    QString sqlCommits = R"(
        CREATE TABLE IF NOT EXISTS commits (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ts INTEGER NOT NULL,
            date TEXT NOT NULL,
            hour TINYINT NOT NULL,
            chars SMALLINT NOT NULL,
            len_cat TINYINT NOT NULL
        );
    )";
    if (!query.exec(sqlCommits)) {
        qWarning() << "创建 commits 表失败:" << query.lastError().text();
        return false;
    }

    // 索引
    QString idxDate = "CREATE INDEX IF NOT EXISTS idx_commits_date ON commits(date);";
    if (!query.exec(idxDate)) {
        qWarning() << "创建日期索引失败:" << query.lastError().text();
        return false;
    }
    QString idxDateHour = "CREATE INDEX IF NOT EXISTS idx_commits_date_hour ON commits(date, hour);";
    if (!query.exec(idxDateHour)) {
        qWarning() << "创建日期-小时索引失败:" << query.lastError().text();
        return false;
    }

    // daily_agg 表
    QString sqlDaily = R"(
        CREATE TABLE IF NOT EXISTS daily_agg (
            date TEXT PRIMARY KEY,
            total_chars INTEGER NOT NULL,
            cnt_commits INTEGER NOT NULL,
            cnt_1 INTEGER DEFAULT 0,
            cnt_2 INTEGER DEFAULT 0,
            cnt_3 INTEGER DEFAULT 0,
            cnt_4 INTEGER DEFAULT 0,
            cnt_5plus INTEGER DEFAULT 0
        );
    )";
    if (!query.exec(sqlDaily)) {
        qWarning() << "创建 daily_agg 表失败:" << query.lastError().text();
        return false;
    }

    // meta 表（用于存储全局总字数等）
    QString sqlMeta = R"(
        CREATE TABLE IF NOT EXISTS meta (
            key TEXT PRIMARY KEY,
            value INTEGER NOT NULL
        );
    )";
    if (!query.exec(sqlMeta)) {
        qWarning() << "创建 meta 表失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseStorage::ensureMetaTable()
{
    // meta 表已在 createTables 中创建，这里确保 total 键存在
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO meta (key, value) VALUES ('total', 0);");
    if (!query.exec()) {
        qWarning() << "初始化 meta.total 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

// ---------- 元数据读写 ----------
bool DatabaseStorage::setMeta(const QString& key, qint64 value)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (:key, :value);");
    query.bindValue(":key", key);
    query.bindValue(":value", value);
    if (!query.exec()) {
        qWarning() << "设置 meta 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

qint64 DatabaseStorage::getMeta(const QString& key, qint64 defaultValue) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM meta WHERE key = :key;");
    query.bindValue(":key", key);
    if (!query.exec()) {
        qWarning() << "读取 meta 失败:" << query.lastError().text();
        return defaultValue;
    }
    if (query.next()) {
        return query.value(0).toLongLong();
    }
    return defaultValue;
}

// ---------- 公开 API ----------
bool DatabaseStorage::addCommit(qint64 ts, const QString& date, int hour, int chars, int lenCat)
{
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        if (!open()) return false;
    }

    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO commits (ts, date, hour, chars, len_cat) "
        "VALUES (:ts, :date, :hour, :chars, :len_cat);"
    );
    query.bindValue(":ts", ts);
    query.bindValue(":date", date);
    query.bindValue(":hour", hour);
    query.bindValue(":chars", chars);
    query.bindValue(":len_cat", lenCat);

    if (!query.exec()) {
        qWarning() << "addCommit 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseStorage::upsertDailyAgg(const QString& date,
                                     qint64 totalChars,
                                     int cntCommits,
                                     int cnt1, int cnt2, int cnt3, int cnt4, int cnt5plus)
{
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        if (!open()) return false;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO daily_agg
            (date, total_chars, cnt_commits, cnt_1, cnt_2, cnt_3, cnt_4, cnt_5plus)
        VALUES
            (:date, :total_chars, :cnt_commits, :cnt_1, :cnt_2, :cnt_3, :cnt_4, :cnt_5plus);
    )");
    query.bindValue(":date", date);
    query.bindValue(":total_chars", totalChars);
    query.bindValue(":cnt_commits", cntCommits);
    query.bindValue(":cnt_1", cnt1);
    query.bindValue(":cnt_2", cnt2);
    query.bindValue(":cnt_3", cnt3);
    query.bindValue(":cnt_4", cnt4);
    query.bindValue(":cnt_5plus", cnt5plus);

    if (!query.exec()) {
        qWarning() << "upsertDailyAgg 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

// ---------- 查询 ----------
qint64 DatabaseStorage::getTotal() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        const_cast<DatabaseStorage*>(this)->open();
    }
    return getMeta("total", 0);
}

bool DatabaseStorage::setTotal(qint64 total)
{
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        if (!open()) return false;
    }
    return setMeta("total", total);
}

qint64 DatabaseStorage::getToday(const QString& date) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        const_cast<DatabaseStorage*>(this)->open();
    }
    QSqlQuery query(m_db);
    query.prepare("SELECT total_chars FROM daily_agg WHERE date = :date;");
    query.bindValue(":date", date);
    if (!query.exec()) {
        qWarning() << "getToday 查询失败:" << query.lastError().text();
        return 0;
    }
    if (query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
}

std::array<qint64, 24> DatabaseStorage::getHourly(const QString& date) const
{
    std::array<qint64, 24> result{};
    result.fill(0);

    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        const_cast<DatabaseStorage*>(this)->open();
    }

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT hour, SUM(chars) FROM commits WHERE date = :date GROUP BY hour;"
    );
    query.bindValue(":date", date);
    if (!query.exec()) {
        qWarning() << "getHourly 查询失败:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        int hour = query.value(0).toInt();
        if (hour >= 0 && hour < 24) {
            result[hour] = query.value(1).toLongLong();
        }
    }
    return result;
}

std::array<qint64, 7> DatabaseStorage::getDaily7(const QString& today) const
{
    std::array<qint64, 7> result{};
    result.fill(0);

    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        const_cast<DatabaseStorage*>(this)->open();
    }

    // 用 date 范围查询，避免依赖 LIMIT 不确定顺序
    QDate current = QDate::fromString(today, "yyyy-MM-dd");
    if (!current.isValid()) {
        qWarning() << "getDaily7: 无效日期格式" << today;
        return result;
    }
    QDate start = current.addDays(-6);
    QString startStr = start.toString("yyyy-MM-dd");

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT date, total_chars FROM daily_agg WHERE date >= :start AND date <= :today ORDER BY date;"
    );
    query.bindValue(":start", startStr);
    query.bindValue(":today", today);
    if (!query.exec()) {
        qWarning() << "getDaily7 查询失败:" << query.lastError().text();
        return result;
    }
    int idx = 0;
    while (query.next()) {
        if (idx < 7) {
            result[idx++] = query.value(1).toLongLong();
        }
    }
    return result;
}

bool DatabaseStorage::getDist(const QString& date,
                              int& cnt1, int& cnt2, int& cnt3, int& cnt4, int& cnt5plus) const
{
    cnt1 = cnt2 = cnt3 = cnt4 = cnt5plus = 0;

    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        const_cast<DatabaseStorage*>(this)->open();
    }

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT cnt_1, cnt_2, cnt_3, cnt_4, cnt_5plus FROM daily_agg WHERE date = :date;"
    );
    query.bindValue(":date", date);
    if (!query.exec()) {
        qWarning() << "getDist 查询失败:" << query.lastError().text();
        return false;
    }
    if (query.next()) {
        cnt1 = query.value(0).toInt();
        cnt2 = query.value(1).toInt();
        cnt3 = query.value(2).toInt();
        cnt4 = query.value(3).toInt();
        cnt5plus = query.value(4).toInt();
        return true;
    }
    return true; // 当日无数据，返回成功但值全0
}

// ---------- 维护 ----------
bool DatabaseStorage::cleanupOldRecords(int monthsToKeep)
{
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        if (!open()) return false;
    }

    QDate cutoff = QDate::currentDate().addMonths(-monthsToKeep);
    QString cutoffStr = cutoff.toString("yyyy-MM-dd");

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM commits WHERE date < :cutoff;");
    query.bindValue(":cutoff", cutoffStr);
    if (!query.exec()) {
        qWarning() << "清理旧记录失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "清理了" << query.numRowsAffected() << "条旧记录";
    return true;
}

bool DatabaseStorage::vacuum()
{
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) {
        if (!open()) return false;
    }
    QSqlQuery query(m_db);
    if (!query.exec("VACUUM;")) {
        qWarning() << "VACUUM 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

} // namespace wordcount