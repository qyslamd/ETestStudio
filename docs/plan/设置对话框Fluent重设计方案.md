# 设置对话框 Fluent 重设计方案

## 问题陈述

当前设置对话框（`SettingsDialog`）是普通 QDialog（800x500，左 QListWidget 导航 + 右
QStackedWidget 页面 + 底部关闭），已有基础 QSS（`etest--app--SettingsDialog` 块），
但缺 Fluent 质感：左导航纯文字无图标、复选是普通 QCheckBox、下拉/数字/按钮未
Fluent 化。按已确认的 HTML 设计稿（`docs/prototype/SettingsDialog设计.html`）
重设计为 Fluent 风格。

## 架构回顾

- `SettingsDialog`（`src/app/dialogs/SettingsDialog.cpp`）：QDialog，
  `resize(800,500)`；左 `QListWidget`（180px 固定）6 项（通用/编辑器/终端/外观/
  项目/备份）；右 `QStackedWidget` 6 页；底部 `SettingsButtonBar` 含「关闭」。
- 辅助：`createSettingsCard(parent, title)`（卡片）、`addSettingRow(parent, title,
  desc)`（行 = 标题+描述+控件）；控件类型：QComboBox / QSpinBox / QCheckBox /
  QPushButton，经 `*_map_` 绑定 ConfigManager 双向同步。
- 外观页内容多，包 `QScrollArea`。
- 现有 QSS：`etest--app--SettingsDialog` 块（QListWidget 项样式、卡片、按钮栏）。
- 图标：`settings`/`file_cpp`/`tab_terminal`/`folder`/`file_save` 均有 SVG 对；
  缺「外观」调色板图标。

## 方案选项及理由

### 对话框形态
- **选项 A（选定）**：转 `OverlayDialog`（无边框 + 遮罩卡片）。理由：统一跨平台外观
  （Linux 原生标题栏/边框难看），与登录/关于/向导一致。**接受代价**（用户确认）：
  模态（配置期间主窗口被阻塞）、无原生标题（加 in-dialog 标题栏「设置」图标+标题）、
  固定尺寸（800x500 卡片居中）、Linux 子覆盖层（不可独立移动）。MainWindow 改 `exec()`。
- 选项 B 保持 QDialog：不选（Linux 标题/边框难看，不统一）。

### 左导航图标
- **选项 A（选定）**：加图标：通用=`settings`、编辑器=`file_cpp`、终端=`tab_terminal`、
  外观=`palette`（新增 SVG 对）、项目=`folder`、备份=`file_save`。

### 复选控件
- **选项 A（选定）**：复用 `utils/switch_button.h`（SwitchButton，自绘胶囊 + 白色滑块
  + 滑动动画）。理由：设计稿确认要真 Fluent Toggle；**纯 QSS 无法自绘滑块**（Qt QSS
  无伪元素/动画，`::indicator` 只能做纯色 pill，审查 🔴1）。
  `addCheckBoxRow` 改用 SwitchButton，`check_map_` 类型改 `QAbstractButton*`
  （isChecked/setChecked/toggled 均为 QAbstractButton API），双向绑定不受影响；
  on/off 底色取 `ThemeManager` accent + 中性灰。

## 决策记录

1. 转 `OverlayDialog`：`SettingsDialog : public OverlayDialog`；UI 包进
   `setWidget(content)`，content `setFixedSize(800, 500)`；加 in-dialog 标题栏
   （设置图标 + 「设置」标题）；MainWindow `show()` → `exec()`（模态）。
2. 左导航 6 项加图标（AppIconProvider）；新增 `palette_light/dark.svg` 注册进
   resource.qrc。
3. 右内容沿用 `createSettingsCard`/`addSettingRow` 结构，Fluent QSS 美化：
   卡片圆角 + 边框、行分隔线、控件右对齐。
4. `addCheckBoxRow` 改用 `SwitchButton`（自绘滑块 Toggle）；`check_map_` 改
   `QAbstractButton*`；on=ThemeManager accent、off=中性灰。旧的
   `etest--app--SettingsDialog QCheckBox` QSS（含 ::indicator/checkbox SVG）成为死样式，
   一并清理（审查 Y1/Y2 由 SwitchButton 自绘天然覆盖，无需 QSS）。
5. QComboBox / QSpinBox / QPushButton 补 Fluent QSS（圆角、边框、hover/accent）。
6. QSS 只写 `default.qss`（亮）+ `vscode.qss`（暗）两套（主题已精简为两套）。
7. 外观页 `QScrollArea` 内容滚动条样式沿用现有。
8. 卡片背景为**不透明纯色** + 圆角 + 边框（非半透明，审查 Y3 澄清；半透明会透出
   QStackedWidget 底色，未验证效果）。

## 详细设计

### 文件与类

| 文件 | 说明 |
|------|------|
| 修改 `src/app/dialogs/SettingsDialog.cpp` | 左导航项加图标 |
| 新增 `src/app/resources/icons/svg/palette_light/dark.svg` | 外观导航图标 |
| 修改 `src/app/resource.qrc` | 注册 palette 图标 |
| 修改 `src/app/resources/styles/default.qss` + `vscode.qss` | 设置对话框 Fluent 样式 |

### 左导航图标映射

```
通用 → settings    编辑器 → file_cpp    终端 → tab_terminal
外观 → palette     项目 → folder       备份 → file_save
```

### QSS 要点

- `etest--app--SettingsDialog QListWidget::item`：padding 9px 12px、圆角 7px、
  selected 背景 accent-light、图标 16px（`setIconSize` 已预设）。
- `#SettingsCard`（卡片）：不透明纯色 + 圆角 12px + 1px 边框 + 行分隔。
- Toggle 由 SwitchButton 自绘（胶囊 + 滑块），无需 QCheckBox QSS。
- `QComboBox`/`QSpinBox`：圆角 6px、hover 边框、focus accent。
- 按钮（浏览/备份/立即备份）：主按钮 accent 填充、次级 accent-light。

## 验证

1. `scripts/build_ninja.bat -t debug -m ETestStudio` 编译通过。
2. 手动（GUI 需人工确认）：
   - 左导航 6 项带图标、选中态 accent 高亮；点击切换页面
   - 卡片圆角/边框/行分隔正常；复选为 Fluent Toggle（开=accent）
   - 下拉/数字/按钮 Fluent 化；外观页滚动正常
   - 亮（default）/ 暗（vscode）两套外观一致
   - 设置项读写配置仍正常（改设置即时生效）
