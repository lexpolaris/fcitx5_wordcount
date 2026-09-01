#pragma once

#include "tray_icon_manager.h"
#include "config_manager.h"

#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void setTrayManager(TrayIconManager* manager);

signals:
    void settingsApplied();

private slots:
    void loadSettings();
    void saveSettings();
    void onConfigChanged();  // 响应配置变化

private:
    void setupUI();
    void connectSignals();

    QCheckBox* m_checkPunctuation = nullptr;
    QCheckBox* m_checkEmoji = nullptr;
    QComboBox* m_comboTrayMode = nullptr;
    QComboBox* m_comboTheme = nullptr;
    QCheckBox* m_checkAutoStart = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;

    TrayIconManager* m_trayManager = nullptr;
};
