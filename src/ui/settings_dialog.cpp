#include "settings_dialog.h"
#include "tray_icon_manager.h"
#include "theme_helper.h"
#include "engine/statistics_engine.h"
#include "engine/database_storage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QProgressDialog>

SettingsDialog::SettingsDialog(QWidget* parent)
: QDialog(parent)
{
    setupUI();
    connectSignals();
    loadSettings();
    updateDbInfo();
}

void SettingsDialog::setupUI()
{
    setWindowTitle(tr("设置"));
    setModal(true);
    resize(500, 450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QTabWidget* tabs = new QTabWidget(this);
    tabs->addTab(createGeneralTab(), tr("通用"));
    tabs->addTab(createDatabaseTab(), tr("数据库管理"));

    mainLayout->addWidget(tabs);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(m_buttonBox);

    setLayout(mainLayout);
}

QWidget* SettingsDialog::createGeneralTab()
{
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(widget);

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

    layout->addLayout(formLayout);
    layout->addStretch();

    return widget;
}

QWidget* SettingsDialog::createDatabaseTab()
{
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // 数据库信息
    QGroupBox* infoGroup = new QGroupBox(tr("数据库信息"), this);
    QFormLayout* infoLayout = new QFormLayout(infoGroup);
    m_dbSizeLabel = new QLabel(tr("正在加载..."), this);
    m_dbRecordsLabel = new QLabel(tr("正在加载..."), this);
    QPushButton* refreshBtn = new QPushButton(tr("刷新"), this);
    connect(refreshBtn, &QPushButton::clicked, this, &SettingsDialog::updateDbInfo);

    QHBoxLayout* infoBtnLayout = new QHBoxLayout();
    infoBtnLayout->addWidget(m_dbSizeLabel);
    infoBtnLayout->addStretch();
    infoBtnLayout->addWidget(refreshBtn);
    infoLayout->addRow(tr("大小:"), infoBtnLayout);
    infoLayout->addRow(tr("记录条数:"), m_dbRecordsLabel);
    layout->addWidget(infoGroup);

    // 清理旧数据
    QGroupBox* cleanupGroup = new QGroupBox(tr("清理旧数据"), this);
    QHBoxLayout* cleanupLayout = new QHBoxLayout(cleanupGroup);
    QLabel* keepLabel = new QLabel(tr("保留最近"), this);
    m_cleanupMonths = new QSpinBox(this);
    m_cleanupMonths->setRange(1, 36);
    m_cleanupMonths->setValue(12);
    QLabel* monthLabel = new QLabel(tr("个月的数据"), this);
    m_cleanupBtn = new QPushButton(tr("立即清理"), this);
    cleanupLayout->addWidget(keepLabel);
    cleanupLayout->addWidget(m_cleanupMonths);
    cleanupLayout->addWidget(monthLabel);
    cleanupLayout->addStretch();
    cleanupLayout->addWidget(m_cleanupBtn);
    layout->addWidget(cleanupGroup);

    // 数据库压缩
    QGroupBox* vacuumGroup = new QGroupBox(tr("数据库压缩"), this);
    QHBoxLayout* vacuumLayout = new QHBoxLayout(vacuumGroup);
    QLabel* vacuumLabel = new QLabel(tr("压缩数据库以回收空间，提升性能"), this);
    m_vacuumBtn = new QPushButton(tr("立即压缩"), this);
    vacuumLayout->addWidget(vacuumLabel);
    vacuumLayout->addStretch();
    vacuumLayout->addWidget(m_vacuumBtn);
    layout->addWidget(vacuumGroup);

    // 导入/导出
    QGroupBox* importExportGroup = new QGroupBox(tr("导入/导出"), this);
    QHBoxLayout* ieLayout = new QHBoxLayout(importExportGroup);
    ieLayout->setSpacing(10);

    m_exportJsonBtn = new QPushButton(tr("导出为 JSON"), this);
    m_exportJsonBtn->setMinimumWidth(120);
    m_importJsonBtn = new QPushButton(tr("从 JSON 导入"), this);
    m_importJsonBtn->setMinimumWidth(120);

    QLabel* importWarning = new QLabel(tr("导入将覆盖当前所有数据"), this);
    importWarning->setStyleSheet("color: #E64545; font-size: 9pt;");

    ieLayout->addWidget(m_exportJsonBtn);
    ieLayout->addWidget(m_importJsonBtn);
    ieLayout->addWidget(importWarning);
    ieLayout->addStretch();

    layout->addWidget(importExportGroup);

    // 状态标签
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color: #666; font-size: 10pt;");
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    return widget;
}

void SettingsDialog::connectSignals()
{
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::settingsApplied);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 监听配置变化，当外部修改配置时更新UI
    connect(&ConfigManager::instance(), &ConfigManager::anySettingChanged,
            this, &SettingsDialog::onConfigChanged);

    // 数据库维护信号
    connect(m_cleanupBtn, &QPushButton::clicked, this, &SettingsDialog::onCleanup);
    connect(m_vacuumBtn, &QPushButton::clicked, this, &SettingsDialog::onVacuum);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &SettingsDialog::onExportJson);
    connect(m_importJsonBtn, &QPushButton::clicked, this, &SettingsDialog::onImportJson);
}

void SettingsDialog::setEngine(wordcount::StatisticsEngine* engine)
{
    m_engine = engine;
    // 引擎设置后立即更新数据库信息
    updateDbInfo();
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

// ========== 数据库维护功能 ==========

void SettingsDialog::updateDbInfo()
{
    if (!m_engine) {
        m_dbSizeLabel->setText(tr("未连接"));
        m_dbRecordsLabel->setText(tr("未连接"));
        return;
    }

    wordcount::DatabaseStorage* db = m_engine->getDatabase();
    if (!db) {
        m_dbSizeLabel->setText(tr("未连接"));
        m_dbRecordsLabel->setText(tr("未连接"));
        return;
    }

    // 获取数据库文件大小
    QString dbPath = db->getDbPath();
    QFileInfo info(dbPath);
    if (info.exists()) {
        qint64 size = info.size();
        QString sizeStr;
        if (size < 1024) {
            sizeStr = QString::number(size) + " B";
        } else if (size < 1024 * 1024) {
            sizeStr = QString::number(size / 1024.0, 'f', 1) + " KB";
        } else if (size < 1024 * 1024 * 1024) {
            sizeStr = QString::number(size / (1024.0 * 1024.0), 'f', 1) + " MB";
        } else {
            sizeStr = QString::number(size / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
        }
        m_dbSizeLabel->setText(sizeStr);
    } else {
        m_dbSizeLabel->setText(tr("数据库文件不存在"));
    }

    // 使用 db 自己的连接
    QSqlDatabase conn = db->getDatabase();
    if (conn.isOpen()) {
        QSqlQuery query(conn);
        if (query.exec("SELECT COUNT(*) FROM commits;")) {
            if (query.next()) {
                qint64 count = query.value(0).toLongLong();
                m_dbRecordsLabel->setText(QString::number(count));
            } else {
                m_dbRecordsLabel->setText("0");
            }
        } else {
            m_dbRecordsLabel->setText(tr("查询失败"));
        }
    } else {
        m_dbRecordsLabel->setText(tr("数据库未打开"));
        // 尝试重新打开
        if (db->open()) {
            m_dbRecordsLabel->setText(tr("已重新打开，请刷新"));
        }
    }
}

void SettingsDialog::onCleanup()
{
    if (!m_engine) return;

    int months = m_cleanupMonths->value();
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("确认清理"),
        tr("将删除 %1 个月前的所有数据，此操作不可撤销！\n确定要继续吗？").arg(months),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    m_statusLabel->setText(tr("正在清理数据..."));
    m_statusLabel->setStyleSheet("color: #0066CC;");

    wordcount::DatabaseStorage* db = m_engine->getDatabase();
    if (db && db->cleanupOldRecords(months)) {
        m_statusLabel->setText(tr("清理完成！已删除 %1 个月前的数据").arg(months));
        m_statusLabel->setStyleSheet("color: #00AA00;");
        updateDbInfo();
        // 刷新引擎缓存
        m_engine->load();
    } else {
        m_statusLabel->setText(tr("清理失败，请检查日志"));
        m_statusLabel->setStyleSheet("color: #E64545;");
    }
}

void SettingsDialog::onVacuum()
{
    if (!m_engine) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("确认压缩"),
        tr("压缩数据库期间程序可能短暂无响应，确定要继续吗？"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    m_statusLabel->setText(tr("正在压缩数据库，请稍候..."));
    m_statusLabel->setStyleSheet("color: #0066CC;");
    m_vacuumBtn->setEnabled(false);

    QCoreApplication::processEvents();

    wordcount::DatabaseStorage* db = m_engine->getDatabase();
    bool success = false;

    if (db) {
        // 先刷新数据
        m_engine->flushToDatabase();
        success = db->vacuum();
    }

    m_vacuumBtn->setEnabled(true);

    if (success) {
        m_statusLabel->setText(tr("数据库压缩完成！"));
        m_statusLabel->setStyleSheet("color: #00AA00;");
        updateDbInfo();
    } else {
        m_statusLabel->setText(tr("压缩失败，请检查日志"));
        m_statusLabel->setStyleSheet("color: #E64545;");
    }
}

void SettingsDialog::onExportJson()
{
    if (!m_engine) {
        m_statusLabel->setText(tr("导出失败：引擎未初始化"));
        m_statusLabel->setStyleSheet("color: #E64545;");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("导出数据"), QDir::homePath() + "/wordcount_backup.json",
        tr("JSON 文件 (*.json)"));

    if (filePath.isEmpty()) return;

    m_statusLabel->setText(tr("正在导出数据..."));
    m_statusLabel->setStyleSheet("color: #0066CC;");
    QCoreApplication::processEvents();

    wordcount::DatabaseStorage* db = m_engine->getDatabase();
    if (!db) {
        m_statusLabel->setText(tr("导出失败：数据库未连接"));
        m_statusLabel->setStyleSheet("color: #E64545;");
        return;
    }

    // ★ 使用 DatabaseStorage 的导出方法
    QJsonObject root = db->exportAll();

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        int count = root["commits"].toArray().size();
        m_statusLabel->setText(tr("数据导出成功！共导出 %1 条记录").arg(count));
        m_statusLabel->setStyleSheet("color: #00AA00;");
    } else {
        m_statusLabel->setText(tr("导出失败：无法写入文件"));
        m_statusLabel->setStyleSheet("color: #E64545;");
    }
}

void SettingsDialog::onImportJson()
{
    if (!m_engine) {
        m_statusLabel->setText(tr("导入失败：引擎未初始化"));
        m_statusLabel->setStyleSheet("color: #E64545;");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this, tr("导入数据"), QDir::homePath(),
        tr("JSON 文件 (*.json)"));

    if (filePath.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this, tr("确认导入"),
        tr("导入将覆盖当前所有数据！\n确定要继续吗？"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    m_statusLabel->setText(tr("正在导入数据..."));
    m_statusLabel->setStyleSheet("color: #0066CC;");
    QCoreApplication::processEvents();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_statusLabel->setText(tr("导入失败：无法读取文件"));
        m_statusLabel->setStyleSheet("color: #E64545;");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        m_statusLabel->setText(tr("导入失败：无效的 JSON 格式"));
        m_statusLabel->setStyleSheet("color: #E64545;");
        return;
    }

    wordcount::DatabaseStorage* db = m_engine->getDatabase();
    if (!db) {
        m_statusLabel->setText(tr("导入失败：数据库未连接"));
        m_statusLabel->setStyleSheet("color: #E64545;");
        return;
    }

    //确保数据库已打开
    if (!db->isOpen()) {
        if (!db->open()) {
            m_statusLabel->setText(tr("导入失败：无法打开数据库"));
            m_statusLabel->setStyleSheet("color: #E64545;");
            return;
        }
    }

    // ★ 使用 DatabaseStorage 的导入方法
    QString errorMsg;
    if (db->importAll(doc.object(), &errorMsg)) {
        m_statusLabel->setText(tr("导入成功！"));
        m_statusLabel->setStyleSheet("color: #00AA00;");
        updateDbInfo();
        // 重新加载数据
        m_engine->load();
    } else {
        m_statusLabel->setText(tr("导入失败：%1").arg(errorMsg));
        m_statusLabel->setStyleSheet("color: #E64545;");
    }
}