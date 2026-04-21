#include "ConfigManager.h"
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

class ConfigManager::Impl
{
public:
    Impl() {
        // 跨平台配置路径
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configPath);
        m_settings = std::make_unique<QSettings>(configPath + "/etest.ini", QSettings::IniFormat);
        m_settings->setIniCodec("UTF-8");
        initDefaultValues();
    }

    ~Impl() = default;

    void initDefaultValues() {
        // 初始化所有默认值映射
        m_defaultValues[CONFIG_WINDOW_WIDTH] = 1280;
        m_defaultValues[CONFIG_WINDOW_HEIGHT] = 720;
        m_defaultValues[CONFIG_WINDOW_X] = 100;
        m_defaultValues[CONFIG_WINDOW_Y] = 100;
        m_defaultValues[CONFIG_WINDOW_MAXIMIZED] = false;

        m_defaultValues[CONFIG_RECENT_PROJECT_LIST] = QStringList();
        m_defaultValues[CONFIG_RECENT_LAST_OPEN_PATH] = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

        m_defaultValues[CONFIG_LOG_LEVEL] = CONFIG_LOG_DEFAULT_LEVEL;
        m_defaultValues[CONFIG_LOG_MAX_FILE_SIZE] = CONFIG_LOG_DEFAULT_MAX_FILE_SIZE;
        m_defaultValues[CONFIG_LOG_MAX_FILE_COUNT] = CONFIG_LOG_DEFAULT_MAX_FILE_COUNT;
        m_defaultValues[CONFIG_LOG_KEEP_DAYS] = CONFIG_LOG_DEFAULT_KEEP_DAYS;

        m_defaultValues[CONFIG_BACKUP_ENABLED] = CONFIG_BACKUP_DEFAULT_ENABLED;
        m_defaultValues[CONFIG_BACKUP_INTERVAL_MIN] = CONFIG_BACKUP_DEFAULT_INTERVAL_MIN;
        m_defaultValues[CONFIG_BACKUP_MAX_COUNT] = CONFIG_BACKUP_DEFAULT_MAX_COUNT;
        m_defaultValues[CONFIG_BACKUP_PATH] = "";

        m_defaultValues[CONFIG_DEFAULT_PROJECT_PATH] = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        m_defaultValues[CONFIG_DEFAULT_PROTOCOL_PATH] = "";
    }

    QVariant getDefaultValue(const QString& key) const {
        return m_defaultValues.value(key, QVariant());
    }

    std::unique_ptr<QSettings> m_settings;
    QMap<QString, QVariant> m_defaultValues;
};

ConfigManager::ConfigManager()
    : m_impl(std::make_unique<Impl>())
{
}

ConfigManager::~ConfigManager() = default;

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

template<>
QVariant ConfigManager::get<QVariant>(const QString& key, const QVariant& defaultValue) const
{
    QVariant defaultVal = defaultValue.isValid() ? defaultValue : m_impl->getDefaultValue(key);
    return m_impl->m_settings->value(key, defaultVal);
}

template<>
void ConfigManager::set<QVariant>(const QString& key, const QVariant& value)
{
    if (get<QVariant>(key) == value) {
        return; // 值未变化，不触发更新
    }
    m_impl->m_settings->setValue(key, value);
    Q_EMIT configChanged(key);
}

bool ConfigManager::exportToJson(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to open config export file:" << filePath;
        return false;
    }

    QJsonObject root;
    root["config_version"] = CONFIG_VERSION;

    // 窗口配置
    QJsonObject windowObj;
    windowObj["width"] = get<int>(CONFIG_WINDOW_WIDTH);
    windowObj["height"] = get<int>(CONFIG_WINDOW_HEIGHT);
    windowObj["x"] = get<int>(CONFIG_WINDOW_X);
    windowObj["y"] = get<int>(CONFIG_WINDOW_Y);
    windowObj["maximized"] = get<bool>(CONFIG_WINDOW_MAXIMIZED);
    root["window"] = windowObj;

    // 最近项目配置
    QJsonObject recentObj;
    recentObj["project_list"] = QJsonArray::fromStringList(get<QStringList>(CONFIG_RECENT_PROJECT_LIST));
    recentObj["last_open_path"] = get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
    root["recent"] = recentObj;

    // 日志配置
    QJsonObject logObj;
    logObj["level"] = get<int>(CONFIG_LOG_LEVEL);
    logObj["max_file_size"] = get<int>(CONFIG_LOG_MAX_FILE_SIZE);
    logObj["max_file_count"] = get<int>(CONFIG_LOG_MAX_FILE_COUNT);
    logObj["keep_days"] = get<int>(CONFIG_LOG_KEEP_DAYS);
    root["log"] = logObj;

    // 备份配置
    QJsonObject backupObj;
    backupObj["enabled"] = get<bool>(CONFIG_BACKUP_ENABLED);
    backupObj["interval_min"] = get<int>(CONFIG_BACKUP_INTERVAL_MIN);
    backupObj["max_count"] = get<int>(CONFIG_BACKUP_MAX_COUNT);
    backupObj["path"] = get<QString>(CONFIG_BACKUP_PATH);
    root["backup"] = backupObj;

    // 默认参数配置
    QJsonObject defaultObj;
    defaultObj["project_path"] = get<QString>(CONFIG_DEFAULT_PROJECT_PATH);
    defaultObj["protocol_path"] = get<QString>(CONFIG_DEFAULT_PROTOCOL_PATH);
    root["default"] = defaultObj;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool ConfigManager::importFromJson(const QString& filePath, bool overrideExisting)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open config import file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Config parse error:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "Invalid config file format, root must be object";
        return false;
    }

    QJsonObject root = doc.object();
    int version = root.value("config_version").toInt(0);
    if (version > CONFIG_VERSION) {
        qWarning() << "Config version is too new, current version:" << CONFIG_VERSION << ", import version:" << version;
        return false;
    }

    // 版本兼容处理（后续版本升级时扩展）
    if (version < CONFIG_VERSION) {
        // 旧版本兼容逻辑占位
        qInfo() << "Importing older config version:" << version << ", current version:" << CONFIG_VERSION;
    }

    // 导入配置
    auto importGroup = [this, &root, overrideExisting](const QString& groupName, const QMap<QString, QString>& keyMap) {
        QJsonObject groupObj = root.value(groupName).toObject();
        for (auto it = keyMap.begin(); it != keyMap.end(); ++it) {
            QString jsonKey = it.key();
            QString configKey = it.value();
            if (groupObj.contains(jsonKey)) {
                if (overrideExisting || !m_impl->m_settings->contains(configKey)) {
                    set<QVariant>(configKey, groupObj.value(jsonKey).toVariant());
                }
            }
        }
    };

    // 导入窗口组
    importGroup("window", {
        {"width", CONFIG_WINDOW_WIDTH},
        {"height", CONFIG_WINDOW_HEIGHT},
        {"x", CONFIG_WINDOW_X},
        {"y", CONFIG_WINDOW_Y},
        {"maximized", CONFIG_WINDOW_MAXIMIZED}
    });

    // 导入最近项目组
    importGroup("recent", {
        {"project_list", CONFIG_RECENT_PROJECT_LIST},
        {"last_open_path", CONFIG_RECENT_LAST_OPEN_PATH}
    });

    // 导入日志组
    importGroup("log", {
        {"level", CONFIG_LOG_LEVEL},
        {"max_file_size", CONFIG_LOG_MAX_FILE_SIZE},
        {"max_file_count", CONFIG_LOG_MAX_FILE_COUNT},
        {"keep_days", CONFIG_LOG_KEEP_DAYS}
    });

    // 导入备份组
    importGroup("backup", {
        {"enabled", CONFIG_BACKUP_ENABLED},
        {"interval_min", CONFIG_BACKUP_INTERVAL_MIN},
        {"max_count", CONFIG_BACKUP_MAX_COUNT},
        {"path", CONFIG_BACKUP_PATH}
    });

    // 导入默认参数组
    importGroup("default", {
        {"project_path", CONFIG_DEFAULT_PROJECT_PATH},
        {"protocol_path", CONFIG_DEFAULT_PROTOCOL_PATH}
    });

    return true;
}

void ConfigManager::resetAllToDefault()
{
    for (auto it = m_impl->m_defaultValues.begin(); it != m_impl->m_defaultValues.end(); ++it) {
        set<QVariant>(it.key(), it.value());
    }
}

void ConfigManager::resetKeyToDefault(const QString& key)
{
    QVariant defaultValue = m_impl->getDefaultValue(key);
    if (defaultValue.isValid()) {
        set<QVariant>(key, defaultValue);
    }
}

QVariant ConfigManager::getDefaultValue(const QString& key) const
{
    return m_impl->getDefaultValue(key);
}
