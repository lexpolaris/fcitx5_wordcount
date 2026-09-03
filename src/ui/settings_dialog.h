#pragma once

#include "tray_icon_manager.h"
#include "config_manager.h"

#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void setTrayManager(TrayIconManager* manager);

    void setEngine(wordcount::StatisticsEngine* engine);

signals:
    void settingsApplied();

private slots:
    void loadSettings();
    void saveSettings();
    void onConfigChanged();  // 响应配置变化

    // 数据库维护槽函数
    void onExportJson();
    void onImportJson();
    void onCleanup();
    void onVacuum();
    void updateDbInfo();

private:
    void setupUI();
    void connectSignals();

    QWidget* createGeneralTab();
    QWidget* createDatabaseTab();

    // 通用设置
    QCheckBox* m_checkPunctuation = nullptr;
    QCheckBox* m_checkEmoji = nullptr;
    QComboBox* m_comboTrayMode = nullptr;
    QComboBox* m_comboTheme = nullptr;
    QComboBox* m_comboTierScheme = nullptr;
    QCheckBox* m_checkAutoStart = nullptr;

    // 数据库维护
    QLabel* m_dbSizeLabel = nullptr;
    QLabel* m_dbRecordsLabel = nullptr;
    QSpinBox* m_cleanupMonths = nullptr;
    QPushButton* m_cleanupBtn = nullptr;
    QPushButton* m_vacuumBtn = nullptr;
    QPushButton* m_exportJsonBtn = nullptr;
    QPushButton* m_importJsonBtn = nullptr;
    QLabel* m_statusLabel = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;

    TrayIconManager* m_trayManager = nullptr;

    wordcount::StatisticsEngine* m_engine = nullptr;
};
