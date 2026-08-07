# -*- coding: utf-8 -*-
"""为所有主题注入「新建项目向导」QSS 块。

读取 src/app/resources/themes/<id>.json 的语义色，推导向导所需的派生色
（accent-hover / accent-light / success / 玻璃卡片 / 幽灵按钮等），
幂等写入 src/app/resources/styles/<id>.qss。

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

START = "/* ===== NewProjectWizard 向导 (自动生成, 勿手改) START ===== */"
END = "/* ===== NewProjectWizard 向导 (自动生成, 勿手改) END ===== */"

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


def build_block(theme):
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
        START,
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
        END,
    ]
    return "\n".join(lines) + "\n"


def upsert(path, block):
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    if START in content:
        head = content.split(START, 1)[0]
        if END in content:
            # 去掉 END 后的换行，避免重复注入时尾部空行逐次累积
            tail = content.split(END, 1)[1].lstrip("\n")
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
        block = build_block(theme)
        upsert(qss_path, block)
        updated += 1
        print("[写入] %s.qss" % theme_id)
    print("完成，共更新 %d 个主题" % updated)
    return 0


if __name__ == "__main__":
    sys.exit(main())
