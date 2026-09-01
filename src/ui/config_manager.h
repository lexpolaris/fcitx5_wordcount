#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式
    static ConfigManager& instance();

    // 禁止拷贝和赋值
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 强制同步配置到磁盘
    void sync();

    // 重新加载配置
    void reload();

    // ========== 统计选项 ==========
    bool countPunctuation() const;
    void setCountPunctuation(bool enabled);

    bool countEmoji() const;
    void setCountEmoji(bool enabled);

    // ========== 托盘设置 ==========
    int trayDisplayMode() const;
    void setTrayDisplayMode(int mode);

    // ========== 主题设置 ==========
    int themeMode() const;
    void setThemeMode(int mode);

    // ========== 启动设置 ==========
    bool autoStart() const;
    void setAutoStart(bool enabled);

signals:
    // 配置变更信号
    void countPunctuationChanged(bool enabled);
    void countEmojiChanged(bool enabled);
    void trayDisplayModeChanged(int mode);
    void themeModeChanged(int mode);
    void autoStartChanged(bool enabled);

    // 通用配置变更信号（任何配置改变时触发）
    void anySettingChanged();

private:
    ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() = default;

    // 辅助方法：获取配置值，如果不存在则返回默认值
    template<typename T>
    T value(const QString& key, const T& defaultValue = T()) const;

    // 辅助方法：设置配置值
    template<typename T>
    void setValue(const QString& key, const T& value);

    QSettings m_settings;
    bool m_initialized = false;
};
