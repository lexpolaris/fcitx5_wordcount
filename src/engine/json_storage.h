#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QLockFile>

namespace wordcount {

    class JsonStorage : public QObject
    {
        Q_OBJECT
    public:
        explicit JsonStorage(const QString& filePath, QObject* parent = nullptr);

        bool load(QJsonObject& root);
        bool save(const QJsonObject& root);

    private:
        bool restoreFromBackup(QJsonObject& root);  // 可选的备份恢复
        QString m_filePath;
        QLockFile m_lockFile;   // 文件锁
    };

} // namespace wordcount
