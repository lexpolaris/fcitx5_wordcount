#pragma once

#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QObject>

namespace wordcount {

    enum class ThemeMode {
        System,     // 跟随系统
        Light,      // 强制亮色
        Dark        // 强制暗色
    };

    class ThemeHelper : public QObject
    {
        Q_OBJECT
    public:
        static ThemeHelper& instance() {
            static ThemeHelper helper;
            return helper;
        }

        void setThemeMode(ThemeMode mode) {
            if (m_mode == mode) return;
            m_mode = mode;
            m_settings.setValue("themeMode", static_cast<int>(mode));
            emit themeChanged();
        }

        ThemeMode themeMode() const { return m_mode; }

        // 改为静态方法，方便直接调用
        static bool isDarkTheme() {
            return instance().isDarkThemeImpl();
        }

        static QColor textColor() {
            return instance().textColorImpl();
        }

        static QColor subTextColor() {
            return instance().subTextColorImpl();
        }

        static QColor bgColor() {
            return instance().bgColorImpl();
        }

        static QColor cardBgColor() {
            return instance().cardBgColorImpl();
        }

        static QColor separatorColor() {
            return instance().separatorColorImpl();
        }

        static QColor adjustedColor(const QColor& c) {
            return instance().adjustedColorImpl(c);
        }

        static QColor iconBgColor() {
            return instance().iconBgColorImpl();
        }

        static QColor iconTextColor() {
            return instance().iconTextColorImpl();
        }

        static QColor iconRingBgColor() {
            return instance().iconRingBgColorImpl();
        }

    signals:
        void themeChanged();

    private:
        ThemeHelper() {
            int mode = m_settings.value("themeMode", static_cast<int>(ThemeMode::System)).toInt();
            m_mode = static_cast<ThemeMode>(mode);
        }

        // 实际的实现方法（非静态）
        bool isDarkThemeImpl() const {
            switch (m_mode) {
                case ThemeMode::Light:
                    return false;
                case ThemeMode::Dark:
                    return true;
                case ThemeMode::System:
                default:
                    QPalette pal = QGuiApplication::palette();
                    return pal.window().color().lightness() < 128;
            }
        }

        QColor textColorImpl() const {
            return isDarkThemeImpl() ? QColor(0xF2, 0xF2, 0xF2) : QColor(0x1C, 0x1C, 0x1E);
        }

        QColor subTextColorImpl() const {
            return isDarkThemeImpl() ? QColor(0xA8, 0xA8, 0xAD) : QColor(0x6E, 0x6E, 0x73);
        }

        QColor bgColorImpl() const {
            return isDarkThemeImpl() ? QColor(0x1C, 0x1C, 0x20, 0xEF) : QColor(0xFF, 0xFF, 0xFF, 0xEF);
        }

        QColor cardBgColorImpl() const {
            return isDarkThemeImpl() ? QColor(0x1C, 0x1C, 0x20, 0xD8) : QColor(0xFF, 0xFF, 0xFF, 0xD8);
        }

        QColor separatorColorImpl() const {
            return isDarkThemeImpl() ? QColor(0xFF, 0xFF, 0xFF, 0x1A) : QColor(0x00, 0x00, 0x00, 0x14);
        }

        QColor adjustedColorImpl(const QColor& c) const {
            return isDarkThemeImpl() ? c.lighter(140) : c.lighter(108);
        }

        QColor iconBgColorImpl() const {
            return isDarkThemeImpl() ? QColor(200, 200, 200) : QColor(60, 60, 60);
        }

        QColor iconTextColorImpl() const {
            return isDarkThemeImpl() ? Qt::black : Qt::white;
        }

        QColor iconRingBgColorImpl() const {
            return isDarkThemeImpl() ? QColor(0, 0, 0, 30) : QColor(255, 255, 255, 30);
        }

        ThemeMode m_mode = ThemeMode::System;
        QSettings m_settings;
    };

} // namespace wordcount
