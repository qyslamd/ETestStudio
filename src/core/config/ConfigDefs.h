#ifndef ETEST_CORE_CONFIG_CONFIGDEFS_H_
#define ETEST_CORE_CONFIG_CONFIGDEFS_H_

#include <QString>

namespace etest {
namespace core {
namespace config {

// 配置版本号
constexpr int CONFIG_VERSION = 1;

// 窗口配置组
const QString CONFIG_WINDOW_WIDTH = "window/width";
const QString CONFIG_WINDOW_HEIGHT = "window/height";
const QString CONFIG_WINDOW_X = "window/x";
const QString CONFIG_WINDOW_Y = "window/y";
const QString CONFIG_WINDOW_MAXIMIZED = "window/maximized";
const QString CONFIG_DOCK_LAYOUT = "window/dock_layout";

// 最近项目配置组
const QString CONFIG_RECENT_PROJECT_LIST = "recent/project_list";
const QString CONFIG_RECENT_LAST_OPEN_PATH = "recent/last_open_path";
constexpr int CONFIG_RECENT_MAX_COUNT = 10; // 最近项目保留10个

// 日志配置组
const QString CONFIG_LOG_LEVEL = "log/level";
const QString CONFIG_LOG_MAX_FILE_SIZE = "log/max_file_size";
const QString CONFIG_LOG_MAX_FILE_COUNT = "log/max_file_count";
const QString CONFIG_LOG_KEEP_DAYS = "log/keep_days";

// 日志默认值
constexpr int CONFIG_LOG_DEFAULT_LEVEL = 2; // 0=debug,1=info,2=warn,3=error,4=fatal
constexpr int CONFIG_LOG_DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
constexpr int CONFIG_LOG_DEFAULT_MAX_FILE_COUNT = 20; // 保留20份
constexpr int CONFIG_LOG_DEFAULT_KEEP_DAYS = 7; // 保留7天

// 备份配置组
const QString CONFIG_BACKUP_ENABLED = "backup/enabled";
const QString CONFIG_BACKUP_INTERVAL_MIN = "backup/interval_min";
const QString CONFIG_BACKUP_MAX_COUNT = "backup/max_count";
const QString CONFIG_BACKUP_PATH = "backup/path";

// 备份默认值
constexpr bool CONFIG_BACKUP_DEFAULT_ENABLED = true; // 默认开启自动备份
constexpr int CONFIG_BACKUP_DEFAULT_INTERVAL_MIN = 5; // 默认5分钟间隔
constexpr int CONFIG_BACKUP_DEFAULT_MAX_COUNT = 5; // 默认保留5份

// 默认参数配置组
const QString CONFIG_DEFAULT_PROJECT_PATH = "default/project_path";
const QString CONFIG_DEFAULT_PROTOCOL_PATH = "default/protocol_path";

// 插件配置组
const QString CONFIG_PLUGIN_SEARCH_PATHS = "plugin/search_paths";  // 分号分隔的自定义搜索路径

// 项目配置组
const QString CONFIG_PROJECT_AUTO_OPEN_LAST = "project/auto_open_last";
constexpr bool CONFIG_PROJECT_DEFAULT_AUTO_OPEN_LAST = false;

}  // namespace config
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_CONFIG_CONFIGDEFS_H_
