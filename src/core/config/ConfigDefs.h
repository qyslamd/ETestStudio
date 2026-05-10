#ifndef ETEST_CORE_CONFIG_CONFIGDEFS_H_
#define ETEST_CORE_CONFIG_CONFIGDEFS_H_

namespace etest {
namespace core {
namespace config {

// 配置版本号
constexpr int CONFIG_VERSION = 1;

// 窗口配置组
constexpr const char* CONFIG_WINDOW_WIDTH = "window/width";
constexpr const char* CONFIG_WINDOW_HEIGHT = "window/height";
constexpr const char* CONFIG_WINDOW_X = "window/x";
constexpr const char* CONFIG_WINDOW_Y = "window/y";
constexpr const char* CONFIG_WINDOW_MAXIMIZED = "window/maximized";
constexpr const char* CONFIG_DOCK_LAYOUT = "window/dock_layout";

// 窗口默认值
constexpr int CONFIG_WINDOW_DEFAULT_WIDTH = 1200;
constexpr int CONFIG_WINDOW_DEFAULT_HEIGHT = 800;
constexpr int CONFIG_WINDOW_DEFAULT_X = -1;
constexpr int CONFIG_WINDOW_DEFAULT_Y = -1;
constexpr bool CONFIG_WINDOW_DEFAULT_MAXIMIZED = false;

// 最近项目配置组
constexpr const char* CONFIG_RECENT_PROJECT_LIST = "recent/project_list";
constexpr const char* CONFIG_RECENT_LAST_OPEN_PATH = "recent/last_open_path";
constexpr int CONFIG_RECENT_MAX_COUNT = 10; // 最近项目保留10个

// 日志配置组
constexpr const char* CONFIG_LOG_LEVEL = "log/level";
constexpr const char* CONFIG_LOG_MAX_FILE_SIZE = "log/max_file_size";
constexpr const char* CONFIG_LOG_MAX_FILE_COUNT = "log/max_file_count";
constexpr const char* CONFIG_LOG_KEEP_DAYS = "log/keep_days";

// 日志默认值
constexpr int CONFIG_LOG_DEFAULT_LEVEL = 2; // 0=debug,1=info,2=warn,3=error,4=fatal
constexpr int CONFIG_LOG_DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
constexpr int CONFIG_LOG_DEFAULT_MAX_FILE_COUNT = 20; // 保留20份
constexpr int CONFIG_LOG_DEFAULT_KEEP_DAYS = 7; // 保留7天

// 备份配置组
constexpr const char* CONFIG_BACKUP_ENABLED = "backup/enabled";
constexpr const char* CONFIG_BACKUP_INTERVAL_MIN = "backup/interval_min";
constexpr const char* CONFIG_BACKUP_MAX_COUNT = "backup/max_count";
constexpr const char* CONFIG_BACKUP_PATH = "backup/path";

// 备份默认值
constexpr bool CONFIG_BACKUP_DEFAULT_ENABLED = true; // 默认开启自动备份
constexpr int CONFIG_BACKUP_DEFAULT_INTERVAL_MIN = 5; // 默认5分钟间隔
constexpr int CONFIG_BACKUP_DEFAULT_MAX_COUNT = 5; // 默认保留5份

// 默认参数配置组
constexpr const char* CONFIG_DEFAULT_PROJECT_PATH = "default/project_path";
constexpr const char* CONFIG_DEFAULT_PROTOCOL_PATH = "default/protocol_path";

// 插件配置组
constexpr const char* CONFIG_PLUGIN_SEARCH_PATHS = "plugin/search_paths";  // 分号分隔的自定义搜索路径

// 项目配置组
constexpr const char* CONFIG_PROJECT_AUTO_OPEN_LAST = "project/auto_open_last";
constexpr bool CONFIG_PROJECT_DEFAULT_AUTO_OPEN_LAST = false;

// 工具栏配置组
constexpr const char* CONFIG_TOOLBAR_VISIBLE = "toolbar/visible";
constexpr const char* CONFIG_TOOLBAR_ICON_SIZE = "toolbar/icon_size";
constexpr const char* CONFIG_TOOLBAR_TEXT_VISIBLE = "toolbar/text_visible";

constexpr bool CONFIG_TOOLBAR_DEFAULT_VISIBLE = true;
constexpr int CONFIG_TOOLBAR_DEFAULT_ICON_SIZE = 16;
constexpr bool CONFIG_TOOLBAR_DEFAULT_TEXT_VISIBLE = false;

// 编辑器配置组
constexpr const char* CONFIG_EDITOR_FONT_SIZE = "editor/font_size";
constexpr const char* CONFIG_EDITOR_SHOW_LINE_NUMBER = "editor/show_line_number";
constexpr const char* CONFIG_EDITOR_AUTO_INDENT = "editor/auto_indent";
constexpr const char* CONFIG_EDITOR_TAB_WIDTH = "editor/tab_width";
constexpr const char* CONFIG_EDITOR_SPACES_FOR_TAB = "editor/spaces_for_tab";

// 编辑器默认值
constexpr int CONFIG_EDITOR_DEFAULT_FONT_SIZE = 12;
constexpr bool CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER = true;
constexpr bool CONFIG_EDITOR_DEFAULT_AUTO_INDENT = true;
constexpr int CONFIG_EDITOR_DEFAULT_TAB_WIDTH = 4;
constexpr bool CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB = true;

// 终端配置组
constexpr const char* CONFIG_TERMINAL_SHELL = "terminal/shell";
constexpr const char* CONFIG_TERMINAL_FONT_SIZE = "terminal/font_size";
constexpr const char* CONFIG_TERMINAL_SCROLLBACK = "terminal/scrollback";

// 终端默认值
#ifdef Q_OS_WIN
constexpr const char* CONFIG_TERMINAL_DEFAULT_SHELL = "cmd.exe";
#else
constexpr const char* CONFIG_TERMINAL_DEFAULT_SHELL = "/bin/bash";
#endif
constexpr int CONFIG_TERMINAL_DEFAULT_FONT_SIZE = 11;
constexpr int CONFIG_TERMINAL_DEFAULT_SCROLLBACK = 10000;

}  // namespace config
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_CONFIG_CONFIGDEFS_H_
