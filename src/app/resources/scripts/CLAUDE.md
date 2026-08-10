# CLAUDE.md

本文件为 `src/app/resources/scripts/` 目录作用域，仅在此目录下工作时加载（懒加载），根 CLAUDE.md 不常驻这些内容。

## 新增主题（JSON 驱动）完整手册

主题数据全在 JSON 中定义。两处 QADS 加载点（`MainWindow::onThemeChanged` / `ThemeManager::loadQss`）已按主题 ID 推导 `ads_<id>.qss` 路径，**新增主题无需改 C++**。

### 快速生成（推荐）

使用 `src/app/resources/scripts/gen_themes.py` 批量生成，只需在脚本中填写主题 ID、显示名和主色值：

```python
themes = [
    { 'id': 'my_theme', 'displayName': '我的主题 My Theme', 'accent': '#FF6600' },
]
```

运行后自动生成 JSON + QSS + Ribbon QSS + ads QSS 四个文件（脚本同时修复 QSS 中的 QADS 注释指向，并注入 QSpinBox/QComboBox 主题适配样式）。

### 手动创建

1. `src/app/resources/themes/<id>.json` — 16 个语义色 + 26 个编辑器色
2. `src/app/resources/styles/<id>.qss` — 基于 `vscode.qss`（暗色）或 `default.qss`（亮色）替换颜色
3. `src/app/resources/styles/ribbon_<id>.qss` — 基于 `theme-dark2.qss`（暗色）或 `theme-office2021-blue.qss`（亮色）替换颜色
4. `src/app/resources/styles/ads_<id>.qss` — QADS dock 样式，基于 `ads_template.qss`（亮色）或 `ads_vscode.qss`（暗色）替换颜色
5. 亮色 ribbon QSS 必须补 `SARibbonButtonGroupWidget > QToolButton` + `SARibbonQuickAccessBar` 块（见 `ribbon_default.qss` 头部），暗色不需要
6. 用 Python 批量替换颜色时，旧值和新值都要带 `#` 前缀避双井号
7. `src/app/resource.qrc` 注册 4 个文件（JSON/QSS/ribbon QSS/ads QSS）
8. 亮色 ads QSS 引用 `_dark` 图标变体、暗色引用 `_light`（与 `AppIconProvider` 规则一致）
9. dock 标签(tab) 的选中/形状配色由 `DockAreaTabBarStyle` 程序化绘制，不受 ads QSS 控制
10. 运行 `python src/app/resources/scripts/gen_themes.py --widgets` 注入 QSpinBox/QComboBox 主题适配样式（基于 `styles/spinbox_template.qss` / `combobox_template.qss` 模板，幂等；快速生成已自动注入，无需重复）
