#include "json_storage.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QDebug>

namespace wordcount {

    JsonStorage::JsonStorage(const QString& filePath, QObject* parent)
    : QObject(parent), m_filePath(filePath), m_lockFile(filePath + ".lock")
    {
    }

    bool JsonStorage::load(QJsonObject& root)
    {
        QFile file(m_filePath);
        if (!file.exists()) {
            root = QJsonObject();
            return true; // 空数据视为成功
        }
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "无法打开存储文件:" << file.errorString();
            return false;
        }
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "JSON解析错误:" << err.errorString();
            return false;
        }
        if (!doc.isObject()) {
            qWarning() << "JSON根不是对象";
            return false;
        }
        root = doc.object();
        return true;
    }

    bool JsonStorage::save(const QJsonObject& root)
    {
        // 尝试获取锁，最多等待 3 秒
        if (!m_lockFile.tryLock(3000)) {
            qWarning() << "无法获取文件锁，可能有其他进程正在写入";
            return false;
        }

        // 确保锁在函数退出时释放（RAII）
        QLockFile::LockError err = m_lockFile.error();
        if (err != QLockFile::NoError) {
            qWarning() << "文件锁错误:" << err;
            return false;
        }

        // 保存
        QSaveFile file(m_filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "无法写入存储文件:" << file.errorString();
            m_lockFile.unlock();
            return false;
        }
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        if (!file.commit()) {
            qWarning() << "提交存储文件失败";
            m_lockFile.unlock();
            return false;
        }

        m_lockFile.unlock();
        return true;
    }

} // namespace wordcount
