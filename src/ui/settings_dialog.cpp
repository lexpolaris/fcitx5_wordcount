#include "settings_dialog.h"
#include "tray_icon_manager.h"
#include "theme_helper.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QCoreApplication>
#include <QStandardPaths>

SettingsDialog::SettingsDialog(QWidget* parent)
: QDialog(parent)
{
    setupUI();
    connectSignals();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    setWindowTitle(tr("设置"));
    setModal(true);
    resize(380, 280);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* formLayout = new QFormLayout();

    // 统计选项
    m_checkPunctuation = new QCheckBox(tr("统计标点符号"), this);
    formLayout->addRow(tr("字数统计"), m_checkPunctuation);

    m_checkEmoji = new QCheckBox(tr("统计 Emoji"), this);
    formLayout->addRow(QString(), m_checkEmoji);

    // 托盘显示模式
    m_comboTrayMode = new QComboBox(this);
    m_comboTrayMode->addItem(tr("今日字数"), static_cast<int>(TrayIconManager::ModeToday));
    m_comboTrayMode->addItem(tr("段位进度"), static_cast<int>(TrayIconManager::ModeLevel));
    formLayout->addRow(tr("托盘图标"), m_comboTrayMode);

    // 主题选择
    m_comboTheme = new QComboBox(this);
    m_comboTheme->addItem(tr("跟随系统"), static_cast<int>(wordcount::ThemeMode::System));
    m_comboTheme->addItem(tr("亮色"), static_cast<int>(wordcount::ThemeMode::Light));
    m_comboTheme->addItem(tr("暗色"), static_cast<int>(wordcount::ThemeMode::Dark));
    formLayout->addRow(tr("主题"), m_comboTheme);

    // 开机自启
    m_checkAutoStart = new QCheckBox(tr("开机自动启动"), this);
    formLayout->addRow(tr("启动"), m_checkAutoStart);

    mainLayout->addLayout(formLayout);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    setLayout(mainLayout);
}

void SettingsDialog::connectSignals()
{
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::settingsApplied);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 监听配置变化，当外部修改配置时更新UI
    connect(&ConfigManager::instance(), &ConfigManager::anySettingChanged,
            this, &SettingsDialog::onConfigChanged);
}

void SettingsDialog::loadSettings()
{
    auto& config = ConfigManager::instance();

    m_checkPunctuation->setChecked(config.countPunctuation());
    m_checkEmoji->setChecked(config.countEmoji());

    // 托盘模式
    int trayMode = config.trayDisplayMode();
    int index = m_comboTrayMode->findData(trayMode);
    if (index < 0) index = 0;
    m_comboTrayMode->setCurrentIndex(index);

    // 主题
    int themeMode = config.themeMode();
    int themeIndex = m_comboTheme->findData(themeMode);
    if (themeIndex < 0) themeIndex = 0;
    m_comboTheme->setCurrentIndex(themeIndex);

    // 开机自启
    m_checkAutoStart->setChecked(config.autoStart());
}

void SettingsDialog::saveSettings()
{
    auto& config = ConfigManager::instance();

    // 屏蔽 ConfigManager 的信号，避免触发 onConfigChanged
    config.blockSignals(true);

    config.setCountPunctuation(m_checkPunctuation->isChecked());
    config.setCountEmoji(m_checkEmoji->isChecked());
    config.setTrayDisplayMode(m_comboTrayMode->currentData().toInt());
    config.setThemeMode(m_comboTheme->currentData().toInt());
    config.setAutoStart(m_checkAutoStart->isChecked());

    // 恢复信号
    config.blockSignals(false);

    // 手动更新配置变化（如果外部需要响应）
    // 但不需要触发 UI 刷新，因为对话框即将关闭
    // 通知外部设置已应用（不会触发 UI 刷新）
    emit settingsApplied();

    // 应用主题
    auto themeMode = static_cast<wordcount::ThemeMode>(m_comboTheme->currentData().toInt());
    wordcount::ThemeHelper::instance().setThemeMode(themeMode);

    // 应用托盘显示模式
    if (m_trayManager) {
        auto displayMode = static_cast<TrayIconManager::DisplayMode>(
            m_comboTrayMode->currentData().toInt());
        m_trayManager->setDisplayMode(displayMode);
    }

    accept();
}

void SettingsDialog::onConfigChanged()
{
    // 仅在对话框未关闭时响应外部配置变化
    // 检查对话框是否可见（用户正在操作时）
    if (isVisible()) {
        loadSettings();
    }
}

void SettingsDialog::setTrayManager(TrayIconManager* manager)
{
    m_trayManager = manager;
}
