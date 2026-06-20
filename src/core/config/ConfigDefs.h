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

// Splitter 布局状态
constexpr const char* CONFIG_WINDOW_H_SPLITTER_STATE =
    "window/h_splitter_state";
constexpr const char* CONFIG_WINDOW_V_SPLITTER_STATE =
    "window/v_splitter_state";

// 侧边栏状态
constexpr const char* CONFIG_SIDEBAR_VISIBLE = "sidebar/visible";
constexpr const char* CONFIG_SIDEBAR_EXPANDED_WIDTH = "sidebar/expanded_width";
constexpr const char* CONFIG_SIDEBAR_ACTIVE_PAGE = "sidebar/active_page";

// 底部面板状态
constexpr const char* CONFIG_BOTTOM_PANEL_VISIBLE = "bottom_panel/visible";
constexpr const char* CONFIG_BOTTOM_PANEL_HEIGHT = "bottom_panel/height";

// 窗口默认值
constexpr int CONFIG_WINDOW_DEFAULT_WIDTH = 1200;
constexpr int CONFIG_WINDOW_DEFAULT_HEIGHT = 800;
constexpr int CONFIG_WINDOW_DEFAULT_X = -1;
constexpr int CONFIG_WINDOW_DEFAULT_Y = -1;
constexpr bool CONFIG_WINDOW_DEFAULT_MAXIMIZED = false;

// 最近项目配置组
constexpr const char* CONFIG_RECENT_PROJECT_LIST = "recent/project_list";
constexpr const char* CONFIG_RECENT_PROJECT_TIMESTAMPS = "recent/project_timestamps";
constexpr const char* CONFIG_RECENT_LAST_OPEN_PATH = "recent/last_open_path";
constexpr int CONFIG_RECENT_MAX_COUNT = 10;  // 最近项目保留10个

// 日志配置组
constexpr const char* CONFIG_LOG_LEVEL = "log/level";
constexpr const char* CONFIG_LOG_MAX_FILE_SIZE = "log/max_file_size";
constexpr const char* CONFIG_LOG_MAX_FILE_COUNT = "log/max_file_count";
constexpr const char* CONFIG_LOG_KEEP_DAYS = "log/keep_days";

// 日志默认值
constexpr int CONFIG_LOG_DEFAULT_LEVEL =
    2;  // 0=debug,1=info,2=warn,3=error,4=fatal
constexpr int CONFIG_LOG_DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB
constexpr int CONFIG_LOG_DEFAULT_MAX_FILE_COUNT = 20;               // 保留20份
constexpr int CONFIG_LOG_DEFAULT_KEEP_DAYS = 7;                     // 保留7天

// 备份配置组
constexpr const char* CONFIG_BACKUP_ENABLED = "backup/enabled";
constexpr const char* CONFIG_BACKUP_INTERVAL_MIN = "backup/interval_min";
constexpr const char* CONFIG_BACKUP_MAX_COUNT = "backup/max_count";
constexpr const char* CONFIG_BACKUP_PATH = "backup/path";

// 备份默认值
constexpr bool CONFIG_BACKUP_DEFAULT_ENABLED = true;   // 默认开启自动备份
constexpr int CONFIG_BACKUP_DEFAULT_INTERVAL_MIN = 5;  // 默认5分钟间隔
constexpr int CONFIG_BACKUP_DEFAULT_MAX_COUNT = 5;     // 默认保留5份

// 默认参数配置组
constexpr const char* CONFIG_DEFAULT_PROJECT_PATH = "default/project_path";
constexpr const char* CONFIG_DEFAULT_PROTOCOL_PATH = "default/protocol_path";
constexpr const char* CONFIG_DEFAULT_FILE_SAVE_PATH = "default/file_save_path";

// 插件配置组
constexpr const char* CONFIG_PLUGIN_SEARCH_PATHS =
    "plugin/search_paths";  // 分号分隔的自定义搜索路径

// 项目配置组
constexpr const char* CONFIG_PROJECT_AUTO_OPEN_LAST = "project/auto_open_last";
constexpr bool CONFIG_PROJECT_DEFAULT_AUTO_OPEN_LAST = false;

// 登录认证配置组
constexpr const char* CONFIG_AUTH_REMEMBER_USERNAME = "auth/remember_username";
constexpr const char* CONFIG_AUTH_REMEMBER_PASSWORD = "auth/remember_password";

// 外观配置组
constexpr const char* CONFIG_APPEARANCE_THEME = "appearance/theme";

// 欢迎页背景配置
constexpr const char* CONFIG_WELCOME_BG_IMAGE = "welcome/background_image";
constexpr const char* CONFIG_WELCOME_BG_DIR = "welcome/background_dir";
constexpr const char* CONFIG_WELCOME_BG_MODE = "welcome/background_mode";

// 屏保配置
constexpr const char* CONFIG_TUXSAVER_ENABLED = "tuxsaver/enabled";
constexpr const char* CONFIG_TUXSAVER_MODE = "tuxsaver/mode";
constexpr const char* CONFIG_TUXSAVER_IDLE_TIMEOUT = "tuxsaver/idle_timeout";
constexpr bool CONFIG_TUXSAVER_DEFAULT_ENABLED = true;
constexpr const char* CONFIG_TUXSAVER_DEFAULT_MODE = "wisdom";
constexpr int CONFIG_TUXSAVER_DEFAULT_TIMEOUT = 5;

// 外观默认值
constexpr const char* CONFIG_APPEARANCE_DEFAULT_THEME = "default";

// Ribbon 配置组
constexpr const char* CONFIG_RIBBON_MINIMIZED = "ribbon/minimized";
constexpr bool CONFIG_RIBBON_DEFAULT_MINIMIZED = false;

// 工具栏配置组
constexpr const char* CONFIG_TOOLBAR_VISIBLE = "toolbar/visible";
constexpr const char* CONFIG_TOOLBAR_ICON_SIZE = "toolbar/icon_size";
constexpr const char* CONFIG_TOOLBAR_TEXT_VISIBLE = "toolbar/text_visible";

constexpr bool CONFIG_TOOLBAR_DEFAULT_VISIBLE = true;
constexpr int CONFIG_TOOLBAR_DEFAULT_ICON_SIZE = 16;
constexpr bool CONFIG_TOOLBAR_DEFAULT_TEXT_VISIBLE = false;

// 编辑器配置组
constexpr const char* CONFIG_EDITOR_FONT_SIZE = "editor/font_size";
constexpr const char* CONFIG_EDITOR_SHOW_LINE_NUMBER =
    "editor/show_line_number";
constexpr const char* CONFIG_EDITOR_AUTO_INDENT = "editor/auto_indent";
constexpr const char* CONFIG_EDITOR_TAB_WIDTH = "editor/tab_width";
constexpr const char* CONFIG_EDITOR_SPACES_FOR_TAB = "editor/spaces_for_tab";

// 编辑器默认值
constexpr int CONFIG_EDITOR_DEFAULT_FONT_SIZE = 12;
constexpr bool CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER = true;
constexpr bool CONFIG_EDITOR_DEFAULT_AUTO_INDENT = true;
constexpr int CONFIG_EDITOR_DEFAULT_TAB_WIDTH = 4;
constexpr bool CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB = true;

// 编辑器 Splitter / 窗口布局状态
constexpr const char* CONFIG_PROTOCOL_SPLITTER_STATE =
    "editor/protocol_splitter";

// 终端配置组
constexpr const char* CONFIG_TERMINAL_SHELL = "terminal/shell";
constexpr const char* CONFIG_TERMINAL_FONT_SIZE = "terminal/font_size";
constexpr const char* CONFIG_TERMINAL_SCROLLBACK = "terminal/scrollback";

// 终端ANSI 16色调色板 (存储为#RRGGBB字符串)
constexpr const char* CONFIG_TERMINAL_COLOR_BLACK = "terminal/color/black";
constexpr const char* CONFIG_TERMINAL_COLOR_RED = "terminal/color/red";
constexpr const char* CONFIG_TERMINAL_COLOR_GREEN = "terminal/color/green";
constexpr const char* CONFIG_TERMINAL_COLOR_YELLOW = "terminal/color/yellow";
constexpr const char* CONFIG_TERMINAL_COLOR_BLUE = "terminal/color/blue";
constexpr const char* CONFIG_TERMINAL_COLOR_MAGENTA = "terminal/color/magenta";
constexpr const char* CONFIG_TERMINAL_COLOR_CYAN = "terminal/color/cyan";
constexpr const char* CONFIG_TERMINAL_COLOR_WHITE = "terminal/color/white";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_BLACK =
    "terminal/color/bright_black";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_RED =
    "terminal/color/bright_red";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_GREEN =
    "terminal/color/bright_green";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_YELLOW =
    "terminal/color/bright_yellow";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_BLUE =
    "terminal/color/bright_blue";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_MAGENTA =
    "terminal/color/bright_magenta";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_CYAN =
    "terminal/color/bright_cyan";
constexpr const char* CONFIG_TERMINAL_COLOR_BRIGHT_WHITE =
    "terminal/color/bright_white";
constexpr const char* CONFIG_TERMINAL_COLOR_FG = "terminal/color/fg";
constexpr const char* CONFIG_TERMINAL_COLOR_BG = "terminal/color/bg";

// 编辑器主题色 (存储为#RRGGBB字符串)
constexpr const char* CONFIG_EDITOR_THEME_PAPER = "editor/theme/paper";
constexpr const char* CONFIG_EDITOR_THEME_TEXT = "editor/theme/text";
constexpr const char* CONFIG_EDITOR_THEME_CARET_LINE =
    "editor/theme/caret_line";
constexpr const char* CONFIG_EDITOR_THEME_CARET = "editor/theme/caret";
constexpr const char* CONFIG_EDITOR_THEME_SELECTION_BG =
    "editor/theme/selection_bg";
constexpr const char* CONFIG_EDITOR_THEME_SELECTION_FG =
    "editor/theme/selection_fg";
constexpr const char* CONFIG_EDITOR_THEME_MARGIN_BG = "editor/theme/margin_bg";
constexpr const char* CONFIG_EDITOR_THEME_LINE_NUMBER =
    "editor/theme/line_number";
constexpr const char* CONFIG_EDITOR_THEME_INDENT_GUIDE =
    "editor/theme/indent_guide";
constexpr const char* CONFIG_EDITOR_THEME_BRACE_LIGHT_BG =
    "editor/theme/brace_light_bg";
constexpr const char* CONFIG_EDITOR_THEME_BRACE_LIGHT_FG =
    "editor/theme/brace_light_fg";
constexpr const char* CONFIG_EDITOR_THEME_BRACE_BAD_BG =
    "editor/theme/brace_bad_bg";
constexpr const char* CONFIG_EDITOR_THEME_BRACE_BAD_FG =
    "editor/theme/brace_bad_fg";
constexpr const char* CONFIG_EDITOR_THEME_FOLD_MARGIN =
    "editor/theme/fold_margin";

// 编辑器语法色 (存储为#RRGGBB字符串)
constexpr const char* CONFIG_EDITOR_SYNTAX_KEYWORD = "editor/syntax/keyword";
constexpr const char* CONFIG_EDITOR_SYNTAX_COMMENT = "editor/syntax/comment";
constexpr const char* CONFIG_EDITOR_SYNTAX_STRING = "editor/syntax/string";
constexpr const char* CONFIG_EDITOR_SYNTAX_NUMBER = "editor/syntax/number";
constexpr const char* CONFIG_EDITOR_SYNTAX_FUNCTION = "editor/syntax/function";
constexpr const char* CONFIG_EDITOR_SYNTAX_TAG = "editor/syntax/tag";
constexpr const char* CONFIG_EDITOR_SYNTAX_PREPROCESSOR =
    "editor/syntax/preprocessor";
constexpr const char* CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS =
    "editor/syntax/global_class";
constexpr const char* CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ =
    "editor/syntax/escape_seq";
constexpr const char* CONFIG_EDITOR_SYNTAX_PROPERTY = "editor/syntax/property";
constexpr const char* CONFIG_EDITOR_SYNTAX_OPERATOR = "editor/syntax/operator";

// 终端默认值
#ifdef Q_OS_WIN
constexpr const char* CONFIG_TERMINAL_DEFAULT_SHELL = "cmd.exe";
#else
constexpr const char* CONFIG_TERMINAL_DEFAULT_SHELL = "/bin/bash";
#endif
constexpr int CONFIG_TERMINAL_DEFAULT_FONT_SIZE = 11;
constexpr int CONFIG_TERMINAL_DEFAULT_SCROLLBACK = 10000;

// 拓扑编辑器配置组
constexpr const char* CONFIG_TOPOLOGY_RESIZE_HANDLES = "topology/resize_handles";
constexpr bool CONFIG_TOPOLOGY_DEFAULT_RESIZE_HANDLES = false;

}  // namespace config
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_CONFIG_CONFIGDEFS_H_
