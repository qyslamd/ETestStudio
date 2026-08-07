# -*- coding: utf-8 -*-
"""为所有主题注入「向导」QSS 块（多块）。

读取 src/app/resources/themes/<id>.json 的语义色，推导向导所需的派生色
（accent-hover / accent-light / success / 玻璃卡片 / 幽灵按钮等），
幂等写入 src/app/resources/styles/<id>.qss。

支持多个块，各块以独立的 START/END 标记为界，互不干扰：
- NewProjectWizard 向导（新建项目向导）
- TestProgramWizard 向导（新建测试程序文件向导）

用法:
    python src/app/resources/scripts/inject_wizard_qss.py

幂等：以块标记注释为界替换，重复运行不会叠加。
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__))))))
THEMES_DIR = os.path.join(ROOT, "src", "app", "resources", "themes")
STYLES_DIR = os.path.join(ROOT, "src", "app", "resources", "styles")

NEW_PROJECT_START = "/* ===== NewProjectWizard 向导 (自动生成, 勿手改) START ===== */"
NEW_PROJECT_END = "/* ===== NewProjectWizard 向导 (自动生成, 勿手改) END ===== */"
TEST_PROGRAM_START = "/* ===== TestProgramWizard 向导 (自动生成, 勿手改) START ===== */"
TEST_PROGRAM_END = "/* ===== TestProgramWizard 向导 (自动生成, 勿手改) END ===== */"

DARK_CARD_BG = "rgba(32, 32, 44, 0.78)"
DARK_CARD_BORDER = "rgba(255, 255, 255, 0.10)"
DARK_GHOST_HOVER = "rgba(255, 255, 255, 0.06)"
DARK_CARD_DEFAULT = "rgba(42, 42, 58, 0.85)"
DARK_CARD_HOVER = "rgba(52, 52, 70, 0.92)"
DARK_GRID_BG = "rgba(255, 255, 255, 0.04)"
DARK_INPUT_BG = "#3A3A44"
DARK_INPUT_BG_FOCUS = "#454550"
DARK_INPUT_BORDER = "#4A4A58"
DARK_SEP = "rgba(255, 255, 255, 0.06)"

LIGHT_CARD_BG = "rgba(255, 255, 255, 0.78)"
LIGHT_CARD_BORDER = "rgba(0, 0, 0, 0.07)"
LIGHT_GHOST_HOVER = "rgba(0, 0, 0, 0.05)"
LIGHT_CARD_DEFAULT = "rgba(255, 255, 255, 0.70)"
LIGHT_CARD_HOVER = "rgba(255, 255, 255, 0.95)"
LIGHT_GRID_BG = "rgba(0, 0, 0, 0.03)"
LIGHT_INPUT_BG = "rgba(255, 255, 255, 0.85)"
LIGHT_INPUT_BG_FOCUS = "#FFFFFF"
LIGHT_INPUT_BORDER = "#D0D0DD"
LIGHT_SEP = "rgba(0, 0, 0, 0.05)"


def hex_to_rgb(value):
    value = value.lstrip("#")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def mix(c1, c2, t):
    return tuple(round(a + (b - a) * t) for a, b in zip(c1, c2))


def rgb_to_hex(rgb):
    return "#%02X%02X%02X" % rgb


def rgba(rgb, alpha):
    return "rgba(%d, %d, %d, %s)" % (rgb[0], rgb[1], rgb[2], alpha)


def accent_light(accent, is_dark):
    if is_dark:
        return rgba(hex_to_rgb(accent), 0.16)
    return rgb_to_hex(mix(hex_to_rgb(accent), (255, 255, 255), 0.86))


def theme_context(theme):
    """提取主题公共派生色，供各块复用。"""
    colors = theme["colors"]
    is_dark = theme["isDark"]
    accent = colors["accentColor"]
    text = colors["textColor"]
    secondary = colors["secondaryTextColor"]
    disabled = colors["disabledTextColor"]
    accent_hover = rgb_to_hex(mix(hex_to_rgb(accent), (0, 0, 0), 0.12))
    accent_light_c = accent_light(accent, is_dark)

    if is_dark:
        card_bg, card_border = DARK_CARD_BG, DARK_CARD_BORDER
        ghost_hover = DARK_GHOST_HOVER
        card_default, card_hover = DARK_CARD_DEFAULT, DARK_CARD_HOVER
        grid_bg, sep = DARK_GRID_BG, DARK_SEP
        input_bg, input_bg_focus, input_border = (
            DARK_INPUT_BG, DARK_INPUT_BG_FOCUS, DARK_INPUT_BORDER)
        close_icon = "close_light.svg"      # 暗色主题用浅色图标
    else:
        card_bg, card_border = LIGHT_CARD_BG, LIGHT_CARD_BORDER
        ghost_hover = LIGHT_GHOST_HOVER
        card_default, card_hover = LIGHT_CARD_DEFAULT, LIGHT_CARD_HOVER
        grid_bg, sep = LIGHT_GRID_BG, LIGHT_SEP
        input_bg, input_bg_focus, input_border = (
            LIGHT_INPUT_BG, LIGHT_INPUT_BG_FOCUS, LIGHT_INPUT_BORDER)
        close_icon = "close_dark.svg"       # 亮色主题用深色图标

    return {
        "is_dark": is_dark,
        "accent": accent,
        "accent_hover": accent_hover,
        "accent_light": accent_light_c,
        "text": text,
        "secondary": secondary,
        "disabled": disabled,
        "card_bg": card_bg,
        "card_border": card_border,
        "ghost_hover": ghost_hover,
        "card_default": card_default,
        "card_hover": card_hover,
        "grid_bg": grid_bg,
        "sep": sep,
        "input_bg": input_bg,
        "input_bg_focus": input_bg_focus,
        "input_border": input_border,
        "close_icon": close_icon,
    }


def build_new_project_block(theme):
    colors = theme["colors"]
    is_dark = theme["isDark"]
    accent = colors["accentColor"]
    text = colors["textColor"]
    secondary = colors["secondaryTextColor"]
    disabled = colors["disabledTextColor"]
    success = "#3FB950" if is_dark else "#107C10"
    success_hover = "#2EA043" if is_dark else "#0A6E0A"

    if is_dark:
        card_bg, card_border = DARK_CARD_BG, DARK_CARD_BORDER
        ghost_hover = DARK_GHOST_HOVER
        card_default, card_hover = DARK_CARD_DEFAULT, DARK_CARD_HOVER
        grid_bg, sep = DARK_GRID_BG, DARK_SEP
        input_bg, input_bg_focus, input_border = (
            DARK_INPUT_BG, DARK_INPUT_BG_FOCUS, DARK_INPUT_BORDER)
    else:
        card_bg, card_border = LIGHT_CARD_BG, LIGHT_CARD_BORDER
        ghost_hover = LIGHT_GHOST_HOVER
        card_default, card_hover = LIGHT_CARD_DEFAULT, LIGHT_CARD_HOVER
        grid_bg, sep = LIGHT_GRID_BG, LIGHT_SEP
        input_bg, input_bg_focus, input_border = (
            LIGHT_INPUT_BG, LIGHT_INPUT_BG_FOCUS, LIGHT_INPUT_BORDER)

    accent_hover = rgb_to_hex(mix(hex_to_rgb(accent), (0, 0, 0), 0.12))
    accent_light_c = accent_light(accent, is_dark)

    edit_names = ["projectNameEdit", "projectLocationEdit",
                  "projectVersionEdit", "projectDescEdit"]
    edits = ",\n".join("QLineEdit#%s" % n for n in edit_names)
    edits_focus = ",\n".join("QLineEdit#%s:focus" % n for n in edit_names)

    lines = [
        NEW_PROJECT_START,
        "#wizardCard {",
        "  background-color: %s;" % card_bg,
        "  border: 1px solid %s;" % card_border,
        "  border-radius: 20px;",
        "}",
        "",
        "#wizardHeaderIcon {",
        "  background-color: %s;" % accent_light_c,
        "  border-radius: 14px;",
        "}",
        "#wizardHeaderTitle {",
        "  color: %s;" % text,
        "  font-size: 20px;",
        "  font-weight: 600;",
        "}",
        "#wizardHeaderSubtitle {",
        "  color: %s;" % secondary,
        "  font-size: 14px;",
        "}",
        "",
        "#templateIntro, #summaryIntro {",
        "  color: %s;" % secondary,
        "  font-size: 14px;",
        "}",
        "",
        "#templateCard {",
        "  background-color: %s;" % card_default,
        "  border: 2px solid transparent;",
        "  border-radius: 10px;",
        "}",
        "#templateCard:hover {",
        "  background-color: %s;" % card_hover,
        "}",
        "#templateCard:checked {",
        "  background-color: %s;" % accent_light_c,
        "  border: 2px solid %s;" % accent,
        "}",
        "#templateCardTitle {",
        "  color: %s;" % text,
        "  font-size: 15px;",
        "  font-weight: 600;",
        "}",
        "#templateCardDesc {",
        "  color: %s;" % secondary,
        "  font-size: 13px;",
        "}",
        "#templateCardBadge {",
        "  background-color: %s;" % accent_light_c,
        "  color: %s;" % accent,
        "  border-radius: 10px;",
        "  font-size: 11px;",
        "  font-weight: 600;",
        "  padding: 2px 10px;",
        "}",
        "",
        "#fieldLabel {",
        "  color: %s;" % text,
        "  font-size: 13px;",
        "  font-weight: 500;",
        "}",
        "#fieldHint {",
        "  color: %s;" % disabled,
        "  font-size: 12px;",
        "}",
        edits + " {",
        "  background-color: %s;" % input_bg,
        "  border: 1.5px solid %s;" % input_border,
        "  border-radius: 10px;",
        "  padding: 9px 12px;",
        "  color: %s;" % text,
        "  selection-background-color: %s;" % accent,
        "}",
        edits_focus + " {",
        "  border-color: %s;" % accent,
        "  background-color: %s;" % input_bg_focus,
        "}",
        "QLineEdit#projectNameEdit[error=\"true\"] {",
        "  border: 1.5px solid #D13438;",
        "}",
        "",
        "#configGroup {",
        "  border-bottom: 1px solid %s;" % sep,
        "}",
        "#configGroupTitle {",
        "  color: %s;" % text,
        "  font-size: 14px;",
        "  font-weight: 500;",
        "}",
        "#configGroupDesc {",
        "  color: %s;" % secondary,
        "  font-size: 13px;",
        "}",
        "",
        "#summaryGrid {",
        "  background-color: %s;" % grid_bg,
        "  border-radius: 10px;",
        "}",
        "#summaryLabel {",
        "  color: %s;" % disabled,
        "  font-size: 12px;",
        "  font-weight: 500;",
        "}",
        "#summaryValue {",
        "  color: %s;" % text,
        "  font-size: 15px;",
        "  font-weight: 500;",
        "}",
        "",
        "#wizardBackBtn, #wizardCancelBtn {",
        "  background-color: transparent;",
        "  color: %s;" % secondary,
        "  border: none;",
        "  border-radius: 10px;",
        "  padding: 8px 22px;",
        "  font-size: 14px;",
        "  font-weight: 500;",
        "}",
        "#wizardBackBtn:hover, #wizardCancelBtn:hover {",
        "  background-color: %s;" % ghost_hover,
        "}",
        "#wizardBackBtn:disabled {",
        "  color: %s;" % disabled,
        "  background-color: transparent;",
        "}",
        "",
        "#wizardNextBtn {",
        "  background-color: %s;" % accent,
        "  color: #FFFFFF;",
        "  border: none;",
        "  border-radius: 10px;",
        "  padding: 8px 22px;",
        "  font-size: 14px;",
        "  font-weight: 500;",
        "}",
        "#wizardNextBtn:hover {",
        "  background-color: %s;" % accent_hover,
        "}",
        "#wizardNextBtn:disabled {",
        "  background-color: %s;" % accent,
        "  color: rgba(255, 255, 255, 0.5);",
        "}",
        "",
        "#wizardCreateBtn {",
        "  background-color: %s;" % success,
        "  color: #FFFFFF;",
        "  border: none;",
        "  border-radius: 10px;",
        "  padding: 8px 22px;",
        "  font-size: 14px;",
        "  font-weight: 500;",
        "}",
        "#wizardCreateBtn:hover {",
        "  background-color: %s;" % success_hover,
        "}",
        NEW_PROJECT_END,
    ]
    return "\n".join(lines) + "\n"


def build_test_program_block(theme):
    ctx = theme_context(theme)
    accent = ctx["accent"]
    accent_hover = ctx["accent_hover"]
    accent_light = ctx["accent_light"]
    text = ctx["text"]
    secondary = ctx["secondary"]
    disabled = ctx["disabled"]
    card_bg = ctx["card_bg"]
    card_border = ctx["card_border"]
    ghost_hover = ctx["ghost_hover"]
    card_default = ctx["card_default"]
    card_hover = ctx["card_hover"]
    grid_bg = ctx["grid_bg"]
    sep = ctx["sep"]
    input_bg = ctx["input_bg"]
    input_bg_focus = ctx["input_bg_focus"]
    input_border = ctx["input_border"]
    close_icon = ctx["close_icon"]

    # 信息页输入框（QLineEdit + QTextEdit 各自选择器）
    line_names = ["progNameEdit", "progVersionEdit", "progAuthorEdit"]
    edit_names = line_names + ["progDescEdit", "progPrecondEdit"]
    line_edits = ",\n".join("QLineEdit#%s" % n for n in line_names)
    text_edits = ",\n".join("QTextEdit#%s" % n for n in edit_names[3:])
    line_edits_focus = ",\n".join("QLineEdit#%s:focus" % n for n in line_names)
    text_edits_focus = ",\n".join("QTextEdit#%s:focus" % n for n in edit_names[3:])
    line_edits_error = ",\n".join("QLineEdit#%s[error=\"true\"]" % n
                                  for n in line_names)
    text_edits_error = ",\n".join("QTextEdit#%s[error=\"true\"]" % n
                                  for n in edit_names[3:])

    lines = [
        TEST_PROGRAM_START,
        "#tpWizardCard {",
        "  background-color: %s;" % card_bg,
        "  border: 1px solid %s;" % card_border,
        "  border-radius: 20px;",
        "}",
        "",
        "#tpInfoIntro {",
        "  color: %s;" % secondary,
        "  font-size: 14px;",
        "}",
        "",
        # 信息页输入框
        line_edits + " {",
        "  background-color: %s;" % input_bg,
        "  border: 1.5px solid %s;" % input_border,
        "  border-radius: 10px;",
        "  padding: 9px 12px;",
        "  color: %s;" % text,
        "  selection-background-color: %s;" % accent,
        "}",
        line_edits_focus + " {",
        "  border-color: %s;" % accent,
        "  background-color: %s;" % input_bg_focus,
        "}",
        text_edits + " {",
        "  background-color: %s;" % input_bg,
        "  border: 1.5px solid %s;" % input_border,
        "  border-radius: 10px;",
        "  padding: 8px 10px;",
        "  color: %s;" % text,
        "  selection-background-color: %s;" % accent,
        "}",
        text_edits_focus + " {",
        "  border-color: %s;" % accent,
        "  background-color: %s;" % input_bg_focus,
        "}",
        line_edits_error + " {",
        "  border: 1.5px solid #D13438;",
        "}",
        text_edits_error + " {",
        "  border: 1.5px solid #D13438;",
        "}",
        "",
        "#tpFieldLabel {",
        "  color: %s;" % text,
        "  font-size: 13px;",
        "  font-weight: 500;",
        "}",
        "",
        # 用例 tab
        "#casesTabWidget::pane {",
        "  border: none;",
        "  background: transparent;",
        "}",
        "#casesTabWidget QTabBar::tab {",
        "  background-color: %s;" % card_default,
        "  color: %s;" % secondary,
        "  border: 1px solid transparent;",
        "  border-radius: 8px;",
        "  padding: 6px 14px;",
        "  margin: 2px;",
        "  font-size: 13px;",
        "}",
        "#casesTabWidget QTabBar::tab:hover {",
        "  background-color: %s;" % card_hover,
        "}",
        "#casesTabWidget QTabBar::tab:selected {",
        "  background-color: %s;" % accent_light,
        "  color: %s;" % accent,
        "  border: 1px solid %s;" % accent,
        "  font-weight: 600;",
        "}",
        "#casesTabWidget QTabBar::close-button {",
        "  image: url(:/resources/icons/svg/%s);" % close_icon,
        "  subcontrol-position: right;",
        "  width: 14px;",
        "  height: 14px;",
        "}",
        "#casesTabWidget QTabBar::close-button:hover {",
        "  background-color: %s;" % ghost_hover,
        "  border-radius: 3px;",
        "}",
        "#addCaseBtn {",
        "  background-color: transparent;",
        "  color: %s;" % accent,
        "  border: 1px dashed %s;" % accent,
        "  border-radius: 8px;",
        "  padding: 4px 12px;",
        "  font-size: 13px;",
        "}",
        "#addCaseBtn:hover {",
        "  background-color: %s;" % accent_light,
        "}",
        "",
        # 步骤工具栏按钮
        "#stepToolbarBtn {",
        "  background-color: transparent;",
        "  color: %s;" % secondary,
        "  border: none;",
        "  border-radius: 8px;",
        "  padding: 6px 14px;",
        "  font-size: 13px;",
        "}",
        "#stepToolbarBtn:hover {",
        "  background-color: %s;" % ghost_hover,
        "}",
        "",
        # 步骤表格
        "#stepTable {",
        "  background-color: transparent;",
        "  border: none;",
        "  gridline-color: transparent;",
        "  color: %s;" % text,
        "  selection-background-color: %s;" % accent_light,
        "}",
        "#stepTable::item {",
        "  padding: 8px 6px;",
        "}",
        "#stepTable QHeaderView::section {",
        "  background-color: transparent;",
        "  color: %s;" % disabled,
        "  font-weight: 500;",
        "  border: none;",
        "  border-bottom: 1px solid %s;" % sep,
        "  padding: 6px 8px;",
        "}",
        "#stepTable QTableCornerButton::section {",
        "  background-color: transparent;",
        "  border: none;",
        "}",
        "",
        # StepEditDialog 模态框
        "#stepModal {",
        "  background-color: %s;" % card_bg,
        "  border: 1px solid %s;" % card_border,
        "  border-radius: 16px;",
        "}",
        "#stepModalTitle {",
        "  color: %s;" % text,
        "  font-size: 16px;",
        "  font-weight: 600;",
        "}",
        "#stepModal QLabel {",
        "  color: %s;" % text,
        "  font-size: 13px;",
        "}",
        "#stepModal QLineEdit, #stepModal QComboBox {",
        "  background-color: %s;" % input_bg,
        "  border: 1.5px solid %s;" % input_border,
        "  border-radius: 8px;",
        "  padding: 6px 10px;",
        "  color: %s;" % text,
        "  selection-background-color: %s;" % accent,
        "}",
        "#stepModal QLineEdit:focus, #stepModal QComboBox:focus {",
        "  border-color: %s;" % accent,
        "  background-color: %s;" % input_bg_focus,
        "}",
        "#stepModal QComboBox::drop-down {",
        "  border: none;",
        "  width: 22px;",
        "}",
        "#stepModal QCheckBox {",
        "  color: %s;" % text,
        "  font-size: 13px;",
        "  spacing: 6px;",
        "}",
        "#stepModal QCheckBox::indicator {",
        "  width: 16px;",
        "  height: 16px;",
        "  border-radius: 4px;",
        "  border: 1px solid %s;" % input_border,
        "  background-color: %s;" % input_bg,
        "}",
        "#stepModal QCheckBox::indicator:checked {",
        "  background-color: %s;" % accent,
        "  border-color: %s;" % accent,
        "}",
        "",
        "#stepConfirmBtn {",
        "  background-color: %s;" % accent,
        "  color: #FFFFFF;",
        "  border: none;",
        "  border-radius: 8px;",
        "  padding: 8px 22px;",
        "  font-size: 14px;",
        "  font-weight: 500;",
        "}",
        "#stepConfirmBtn:hover {",
        "  background-color: %s;" % accent_hover,
        "}",
        "#stepCancelBtn {",
        "  background-color: transparent;",
        "  color: %s;" % secondary,
        "  border: none;",
        "  border-radius: 8px;",
        "  padding: 8px 22px;",
        "  font-size: 14px;",
        "}",
        "#stepCancelBtn:hover {",
        "  background-color: %s;" % ghost_hover,
        "}",
        "",
        # 摘要步骤概览
        "#summaryStepPreview {",
        "  background-color: %s;" % grid_bg,
        "  border-radius: 10px;",
        "  padding: 10px 14px;",
        "  color: %s;" % secondary,
        "  font-size: 13px;",
        "}",
        TEST_PROGRAM_END,
    ]
    return "\n".join(lines) + "\n"


def upsert(path, block, start, end):
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    if start in content:
        head = content.split(start, 1)[0]
        if end in content:
            # 去掉 END 后的换行，避免重复注入时尾部空行逐次累积
            tail = content.split(end, 1)[1].lstrip("\n")
        else:
            # 半损坏状态：有 START 无 END，块延伸到文件尾，整体替换修复
            tail = ""
        content = head + block + tail
    else:
        if not content.endswith("\n"):
            content += "\n"
        content += "\n" + block
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


def main():
    if not os.path.isdir(THEMES_DIR):
        sys.stderr.write("主题目录不存在: %s\n" % THEMES_DIR)
        return 1
    if not os.path.isdir(STYLES_DIR):
        sys.stderr.write("样式目录不存在: %s\n" % STYLES_DIR)
        return 1

    blocks = [
        (NEW_PROJECT_START, NEW_PROJECT_END, build_new_project_block),
        (TEST_PROGRAM_START, TEST_PROGRAM_END, build_test_program_block),
    ]

    updated = 0
    for name in sorted(os.listdir(THEMES_DIR)):
        if not name.endswith(".json"):
            continue
        theme_id = name[:-5]
        qss_path = os.path.join(STYLES_DIR, theme_id + ".qss")
        if not os.path.exists(qss_path):
            print("[跳过] %s: 无对应 qss" % theme_id)
            continue
        with open(os.path.join(THEMES_DIR, name), "r", encoding="utf-8") as f:
            theme = json.load(f)
        for start, end, build in blocks:
            upsert(qss_path, build(theme), start, end)
        updated += 1
        print("[写入] %s.qss" % theme_id)
    print("完成，共更新 %d 个主题" % updated)
    return 0


if __name__ == "__main__":
    sys.exit(main())
