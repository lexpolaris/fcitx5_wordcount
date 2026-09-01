#include "config_manager.h"
#include "tray_icon_manager.h"
#include "theme_helper.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QDebug>

ConfigManager::ConfigManager(QObject* parent)
: QObject(parent)
, m_settings("Fcitx5WordCount", "Fcitx5WordCount")
{
    // 禁用后备机制，只使用我们自己的配置文件
    m_settings.setFallbacksEnabled(false);

    // 确保配置目录存在
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (!configDir.isEmpty()) {
        QDir dir(configDir);
        if (!dir.exists("Fcitx5WordCount")) {
            dir.mkpath("Fcitx5WordCount");
        }
    }

    m_initialized = true;
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

void ConfigManager::sync()
{
    if (m_initialized) {
        m_settings.sync();
        qDebug() << "ConfigManager: settings synced to disk";
    }
}

void ConfigManager::reload()
{
    if (m_initialized) {
        m_settings.sync();  // 先确保写入完成
        // QSettings 会自动从磁盘重新读取
        qDebug() << "ConfigManager: settings reloaded from disk";
    }
}

template<typename T>
T ConfigManager::value(const QString& key, const T& defaultValue) const
{
    if (!m_initialized) {
        return defaultValue;
    }
    return m_settings.value(key, defaultValue).template value<T>();
}

template<typename T>
void ConfigManager::setValue(const QString& key, const T& value)
{
    if (!m_initialized) {
        return;
    }
    m_settings.setValue(key, value);
    m_settings.sync();  // 立即写入磁盘
}

// ========== 统计选项实现 ==========
bool ConfigManager::countPunctuation() const
{
    return value("countPunctuation", false);
}

void ConfigManager::setCountPunctuation(bool enabled)
{
    if (countPunctuation() == enabled) {
        return;  // 值没有变化
    }
    setValue("countPunctuation", enabled);
    emit countPunctuationChanged(enabled);
    emit anySettingChanged();
}

bool ConfigManager::countEmoji() const
{
    return value("countEmoji", false);
}

void ConfigManager::setCountEmoji(bool enabled)
{
    if (countEmoji() == enabled) {
        return;
    }
    setValue("countEmoji", enabled);
    emit countEmojiChanged(enabled);
    emit anySettingChanged();
}

// ========== 托盘设置实现 ==========
int ConfigManager::trayDisplayMode() const
{
    return value("trayDefaultMode", static_cast<int>(TrayIconManager::ModeToday));
}

void ConfigManager::setTrayDisplayMode(int mode)
{
    if (trayDisplayMode() == mode) {
        return;
    }
    setValue("trayDefaultMode", mode);
    emit trayDisplayModeChanged(mode);
    emit anySettingChanged();
}

// ========== 主题设置实现 ==========
int ConfigManager::themeMode() const
{
    return value("themeMode", static_cast<int>(wordcount::ThemeMode::System));
}

void ConfigManager::setThemeMode(int mode)
{
    if (themeMode() == mode) {
        return;
    }
    setValue("themeMode", mode);
    emit themeModeChanged(mode);
    emit anySettingChanged();
}

// ========== 启动设置实现 ==========
bool ConfigManager::autoStart() const
{
    // 检查开机自启文件是否存在 (.config/autostart)
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QString desktopFile = autostartDir + "/fcitx5wordcount.desktop";
    return QFile::exists(desktopFile);
}

void ConfigManager::setAutoStart(bool enabled)
{
    // 使用 .config/autostart 目录
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QDir().mkpath(autostartDir);
    QString desktopFile = autostartDir + "/fcitx5wordcount.desktop";

    if (enabled) {
        QString content = "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Fcitx5 Word Count\n"
        "Exec=" + QCoreApplication::applicationFilePath() + "\n"
        "Hidden=false\n"
        "NoDisplay=false\n"
        "X-GNOME-Autostart-enabled=true\n";
        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(content.toUtf8());
            file.close();
            qDebug() << "ConfigManager: autostart enabled at" << desktopFile;
        } else {
            qWarning() << "ConfigManager: failed to create autostart file:" << file.errorString();
        }
    } else {
        if (QFile::remove(desktopFile)) {
            qDebug() << "ConfigManager: autostart disabled, removed" << desktopFile;
        } else {
            qWarning() << "ConfigManager: failed to remove autostart file:" << desktopFile;
        }
    }

    emit autoStartChanged(enabled);
    emit anySettingChanged();
}

// 显式实例化模板
template bool ConfigManager::value<bool>(const QString&, const bool&) const;
template int ConfigManager::value<int>(const QString&, const int&) const;
template QString ConfigManager::value<QString>(const QString&, const QString&) const;
template void ConfigManager::setValue<bool>(const QString&, const bool&);
template void ConfigManager::setValue<int>(const QString&, const int&);
template void ConfigManager::setValue<QString>(const QString&, const QString&);
