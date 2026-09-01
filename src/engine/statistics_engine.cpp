// src/engine/statistics_engine.cpp
#include "statistics_engine.h"
#include "database_storage.h"
#include "core/speed_tracker.h"
#include "core/level_system.h"
#include "core/char_counter.h"
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>

namespace wordcount {

// ---------- 构造函数 / 析构函数 ----------
StatisticsEngine::StatisticsEngine(DatabaseStorage* db, QObject* parent)
    : QObject(parent), m_db(db)
{
    m_speed = new SpeedTracker();

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(kFlushIntervalMs);
    connect(m_flushTimer, &QTimer::timeout, this, &StatisticsEngine::flushToDatabase);

    QTimer* dateTimer = new QTimer(this);
    dateTimer->setInterval(60000);
    connect(dateTimer, &QTimer::timeout, this, &StatisticsEngine::checkDateRollover);
    dateTimer->start();

    load();
}

StatisticsEngine::~StatisticsEngine()
{
    flushToDatabase();
    delete m_speed;
}

// ---------- load ----------
void StatisticsEngine::load()
{
    if (!m_db || !m_db->init()) {
        qWarning() << "数据库初始化失败，使用空数据";
        m_cachedTotal = 0;
        m_cachedToday = 0;
        m_todayDate = QDate::currentDate().toString("yyyy-MM-dd");
        m_cachedHourly.fill(0);
        m_cachedDaily7.fill(0);
        m_cachedDist.fill(0);
        m_lastGlobalLevel = 0;
        m_lastRolloverCheckDate = m_todayDate;
        emit statsChanged(0, 0, 0.0);
        return;
    }

    m_cachedTotal = m_db->getTotal();
    m_todayDate = QDate::currentDate().toString("yyyy-MM-dd");
    m_cachedToday = m_db->getToday(m_todayDate);

    int cnt1, cnt2, cnt3, cnt4, cnt5plus;
    if (m_db->getDist(m_todayDate, cnt1, cnt2, cnt3, cnt4, cnt5plus)) {
        m_cachedDist = { cnt1, cnt2, cnt3, cnt4, cnt5plus };
    } else {
        m_cachedDist.fill(0);
    }

    m_cachedHourly = m_db->getHourly(m_todayDate);
    m_cachedDaily7 = m_db->getDaily7(m_todayDate);
    m_lastGlobalLevel = LevelSystem::globalLevel(m_cachedTotal);
    m_lastRolloverCheckDate = m_todayDate;

    emit statsChanged(m_cachedTotal, m_cachedToday, currentWpm());
    qDebug() << "数据加载完成: total=" << m_cachedTotal << ", today=" << m_cachedToday;
}

// ---------- flushToDatabase ----------
void StatisticsEngine::flushToDatabase()
{
    if (!m_dirty || m_pendingCommits.isEmpty()) {
        m_flushTimer->stop();
        return;
    }

    if (!m_db) {
        qWarning() << "数据库未初始化，丢失数据";
        m_pendingCommits.clear();
        m_dirty = false;
        m_flushTimer->stop();
        return;
    }

    QSqlDatabase db = QSqlDatabase::database("wordcount_conn");
    if (!db.isOpen()) {
        if (!m_db->open()) {
            qWarning() << "无法打开数据库，稍后重试";
            return;
        }
        db = QSqlDatabase::database("wordcount_conn");
    }

    db.transaction();
    bool ok = true;

    for (const auto& pc : m_pendingCommits) {
        if (!m_db->addCommit(pc.ts, pc.date, pc.hour, pc.chars, pc.lenCat)) {
            ok = false;
            break;
        }
    }

    if (ok) {
        int cnt1 = m_cachedDist[0];
        int cnt2 = m_cachedDist[1];
        int cnt3 = m_cachedDist[2];
        int cnt4 = m_cachedDist[3];
        int cnt5plus = m_cachedDist[4];
        int cntCommits = cnt1 + cnt2 + cnt3 + cnt4 + cnt5plus;
        if (!m_db->upsertDailyAgg(m_todayDate, m_cachedToday, cntCommits,
                                  cnt1, cnt2, cnt3, cnt4, cnt5plus)) {
            ok = false;
        }
    }

    if (ok) {
        if (!m_db->setTotal(m_cachedTotal)) {
            ok = false;
        }
    }

    if (ok) {
        db.commit();
        m_pendingCommits.clear();
        m_dirty = false;
        m_flushTimer->stop();
        qDebug() << "刷盘成功，提交" << m_pendingCommits.size() << "条记录";
    } else {
        db.rollback();
        qWarning() << "刷盘失败，保留队列稍后重试";
    }
}

// ---------- onTextCommitted ----------
void StatisticsEngine::onTextCommitted(const QString& text)
{
    qint64 chars = CharCounter::count(text, m_countPunctuation, m_countEmoji);
    if (chars <= 0) return;

    checkDateRollover();

    m_cachedTotal += chars;
    m_cachedToday += chars;

    int hour = QTime::currentTime().hour();
    if (hour >= 0 && hour < 24) {
        m_cachedHourly[hour] += chars;
    }
    m_cachedDaily7[6] += chars;

    int lenCat = (chars >= 5) ? 4 : (chars - 1);
    if (lenCat >= 0 && lenCat < 5) {
        m_cachedDist[lenCat]++;
    }

    m_speed->onCommit(chars);

    int newLv = LevelSystem::globalLevel(m_cachedTotal);
    if (newLv > m_lastGlobalLevel) {
        emit levelUp(m_lastGlobalLevel, newLv, newLv - m_lastGlobalLevel);
        m_lastGlobalLevel = newLv;
    }

    PendingCommit pc;
    pc.ts = QDateTime::currentMSecsSinceEpoch();
    pc.date = m_todayDate;
    pc.hour = hour;
    pc.chars = static_cast<int>(chars);
    pc.lenCat = (chars >= 5) ? 5 : static_cast<int>(chars);
    m_pendingCommits.append(pc);

    m_dirty = true;
    if (m_pendingCommits.size() >= kMaxPendingSize) {
        flushToDatabase();
    } else if (!m_flushTimer->isActive()) {
        m_flushTimer->start();
    }

    emit statsChanged(m_cachedTotal, m_cachedToday, currentWpm());
}

// ---------- checkDateRollover ----------
void StatisticsEngine::checkDateRollover()
{
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    if (today == m_lastRolloverCheckDate) return;
    m_lastRolloverCheckDate = today;

    if (m_todayDate != today) {
        if (m_dirty) {
            flushToDatabase();
        }

        m_todayDate = today;
        m_cachedToday = 0;
        m_cachedHourly.fill(0);
        m_cachedDist.fill(0);

        m_cachedToday = m_db->getToday(today);
        int cnt1, cnt2, cnt3, cnt4, cnt5plus;
        if (m_db->getDist(today, cnt1, cnt2, cnt3, cnt4, cnt5plus)) {
            m_cachedDist = { cnt1, cnt2, cnt3, cnt4, cnt5plus };
        }
        m_cachedHourly = m_db->getHourly(today);

        refreshDaily7Cache();

        if (!m_pendingCommits.isEmpty()) {
            qWarning() << "日期切换时仍有未刷盘数据，将丢弃";
            m_pendingCommits.clear();
            m_dirty = false;
            m_flushTimer->stop();
        }

        emit statsChanged(m_cachedTotal, m_cachedToday, currentWpm());
        qDebug() << "日期切换至" << today;
    }
}

// ---------- refreshDaily7Cache ----------
void StatisticsEngine::refreshDaily7Cache()
{
    m_cachedDaily7 = m_db->getDaily7(m_todayDate);
}

// ---------- 查询接口 ----------
qint64 StatisticsEngine::totalChars() const
{
    return m_cachedTotal;
}

qint64 StatisticsEngine::todayChars() const
{
    return m_cachedToday;
}

double StatisticsEngine::currentWpm() const
{
    return m_speed ? m_speed->wpm() : 0.0;
}

QJsonArray StatisticsEngine::hourlyToday() const
{
    QJsonArray arr;
    for (qint64 v : m_cachedHourly) {
        arr.append(static_cast<double>(v));
    }
    return arr;
}

QJsonArray StatisticsEngine::daily7() const
{
    QJsonArray arr;
    for (qint64 v : m_cachedDaily7) {
        arr.append(static_cast<double>(v));
    }
    return arr;
}

// ---------- 配置接口 ----------
void StatisticsEngine::setCountPunctuation(bool enable)
{
    m_countPunctuation = enable;
}

void StatisticsEngine::setCountEmoji(bool enable)
{
    m_countEmoji = enable;
}

// ---------- resetToday ----------
void StatisticsEngine::resetToday()
{
    flushToDatabase();

    m_cachedToday = 0;
    m_cachedHourly.fill(0);
    m_cachedDist.fill(0);
    m_cachedDaily7[6] = 0;

    if (m_db) {
        QSqlDatabase db = QSqlDatabase::database("wordcount_conn");
        if (!db.isOpen()) m_db->open();
        QSqlQuery query(db);
        query.prepare("DELETE FROM commits WHERE date = :date;");
        query.bindValue(":date", m_todayDate);
        query.exec();

        query.prepare("DELETE FROM daily_agg WHERE date = :date;");
        query.bindValue(":date", m_todayDate);
        query.exec();

        QSqlQuery sumQuery(db);
        sumQuery.exec("SELECT SUM(total_chars) FROM daily_agg;");
        if (sumQuery.next()) {
            m_cachedTotal = sumQuery.value(0).toLongLong();
        } else {
            m_cachedTotal = 0;
        }
        m_db->setTotal(m_cachedTotal);
    }

    m_lastGlobalLevel = LevelSystem::globalLevel(m_cachedTotal);
    emit statsChanged(m_cachedTotal, m_cachedToday, currentWpm());
}

// ---------- resetAll ----------
void StatisticsEngine::resetAll()
{
    flushToDatabase();

    if (m_db) {
        QSqlDatabase db = QSqlDatabase::database("wordcount_conn");
        if (!db.isOpen()) m_db->open();
        QSqlQuery query(db);
        query.exec("DELETE FROM commits;");
        query.exec("DELETE FROM daily_agg;");
        query.exec("DELETE FROM meta;");
        query.exec("INSERT INTO meta (key, value) VALUES ('total', 0);");
    }

    m_cachedTotal = 0;
    m_cachedToday = 0;
    m_cachedHourly.fill(0);
    m_cachedDaily7.fill(0);
    m_cachedDist.fill(0);
    m_lastGlobalLevel = 0;
    m_pendingCommits.clear();
    m_dirty = false;
    m_flushTimer->stop();
    if (m_speed) m_speed->clear();

    emit statsChanged(0, 0, 0.0);
}

int StatisticsEngine::todayCommits() const
{
    int total = 0;
    for (int i = 0; i < 5; ++i) {
        total += m_cachedDist[i];
    }
    return total;
}

int StatisticsEngine::peakSpeed() const
{
    return m_speed ? m_speed->peakSpeed() : 0;
}

std::array<qint64, 5> StatisticsEngine::distToday() const
{
    return m_cachedDist;
}

} // namespace wordcount