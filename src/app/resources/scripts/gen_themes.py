# -*- coding: utf-8 -*-
import glob
import json
import os
import re
import sys

themes_dir = 'src/app/resources/themes'
styles_dir = 'src/app/resources/styles'

def accent_lighter(hex_color, amount=0.7):
    """Mix hex color with white"""
    r = int(hex_color[1:3], 16)
    g = int(hex_color[3:5], 16)
    b = int(hex_color[5:7], 16)
    r = int(r + (255 - r) * amount)
    g = int(g + (255 - g) * amount)
    b = int(b + (255 - b) * amount)
    return '#{:02X}{:02X}{:02X}'.format(r, g, b)

def accent_darker(hex_color, amount=0.15):
    """Mix hex color with black"""
    r = int(hex_color[1:3], 16)
    g = int(hex_color[3:5], 16)
    b = int(hex_color[5:7], 16)
    r = int(r * (1 - amount))
    g = int(g * (1 - amount))
    b = int(b * (1 - amount))
    return '#{:02X}{:02X}{:02X}'.format(r, g, b)

def text_color(hex_color):
    """Return white or dark text based on background luma"""
    r = int(hex_color[1:3], 16) / 255.0
    g = int(hex_color[3:5], 16) / 255.0
    b = int(hex_color[5:7], 16) / 255.0
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return '#2A2A2A' if luma > 0.5 else '#D0D0D0'

# ---- QSpinBox/QComboBox 主题适配样式注入 ----
# 参考: qt_ui_prototype/resources/styles/QSpinBox 与 QComboBox
# 结构照搬（渐变底/圆角/右侧按钮/弹出列表/状态/尺寸/圆角/frameless），
# 颜色适配各主题语义色。幂等：检测到标记块则先移除再追加。
MARK_SPIN_START = '/* ===== QSpinBox 主题适配样式 START ===== */'
MARK_SPIN_END = '/* ===== QSpinBox 主题适配样式 END ===== */'
MARK_COMBO_START = '/* ===== QComboBox 主题适配样式 START ===== */'
MARK_COMBO_END = '/* ===== QComboBox 主题适配样式 END ===== */'
MARK_SCROLL_START = '/* ===== QScrollArea 主题适配样式 START ===== */'
MARK_SCROLL_END = '/* ===== QScrollArea 主题适配样式 END ===== */'
MARK_GROUPBOX_START = '/* ===== MonitorTypeTile QGroupBox 主题适配样式 START ===== */'
MARK_GROUPBOX_END = '/* ===== MonitorTypeTile QGroupBox 主题适配样式 END ===== */'
MARK_SUBTITLE_START = '/* ===== visualizer 副标题状态配色 START ===== */'
MARK_SUBTITLE_END = '/* ===== visualizer 副标题状态配色 END ===== */'


def spin_hex_to_rgb(hex_color):
    hex_color = hex_color.strip().lstrip('#')
    return (int(hex_color[0:2], 16), int(hex_color[2:4], 16),
            int(hex_color[4:6], 16))


def spin_lighten(hex_color, amount):
    """Mix hex color with white"""
    r, g, b = spin_hex_to_rgb(hex_color)
    r = int(r + (255 - r) * amount)
    g = int(g + (255 - g) * amount)
    b = int(b + (255 - b) * amount)
    return '#{:02X}{:02X}{:02X}'.format(r, g, b)


def spin_build_blocks(c, icon):
    toolbar = c['toolbarBackground']
    panel = c['panelBackground']
    border = c['borderColor']
    text = c['textColor']
    selection = c['selectionBackground']
    accent = c['accentColor']
    disabled = c['disabledTextColor']
    ar, ag, ab = spin_hex_to_rgb(accent)
    hover_top = spin_lighten(toolbar, 0.08)
    hover_bot = spin_lighten(panel, 0.08)

    with open(os.path.join(styles_dir, 'spinbox_template.qss'),
              encoding='utf-8') as f:
        template = f.read()
    return (template
            .replace('@TOOLBAR@', toolbar)
            .replace('@PANEL@', panel)
            .replace('@HOVER_TOP@', hover_top)
            .replace('@HOVER_BOT@', hover_bot)
            .replace('@BORDER@', border)
            .replace('@TEXT@', text)
            .replace('@SELECTION@', selection)
            .replace('@ACCENT@', accent)
            .replace('@DISABLED@', disabled)
            .replace('@AR@', str(ar))
            .replace('@AG@', str(ag))
            .replace('@AB@', str(ab))
            .replace('@ICON@', icon))

def strip_marked(qss, start_mark, end_mark):
    start = qss.find(start_mark)
    end = qss.find(end_mark)
    if start == -1 or end == -1:
        return qss
    end = qss.find('\n', end)
    if end == -1:
        end = len(qss)
    return qss[:start].rstrip() + '\n' + qss[end:]


def combo_build_blocks(c, icon):
    toolbar = c['toolbarBackground']
    panel = c['panelBackground']
    border = c['borderColor']
    text = c['textColor']
    selection = c['selectionBackground']
    accent = c['accentColor']
    disabled = c['disabledTextColor']
    ar, ag, ab = spin_hex_to_rgb(accent)
    hover_top = spin_lighten(toolbar, 0.08)
    hover_bot = spin_lighten(panel, 0.08)

    with open(os.path.join(styles_dir, 'combobox_template.qss'),
              encoding='utf-8') as f:
        template = f.read()
    return (template
            .replace('@TOOLBAR@', toolbar)
            .replace('@PANEL@', panel)
            .replace('@HOVER_TOP@', hover_top)
            .replace('@HOVER_BOT@', hover_bot)
            .replace('@BORDER@', border)
            .replace('@TEXT@', text)
            .replace('@SELECTION@', selection)
            .replace('@ACCENT@', accent)
            .replace('@DISABLED@', disabled)
            .replace('@AR@', str(ar))
            .replace('@AG@', str(ag))
            .replace('@AB@', str(ab))
            .replace('@ICON@', icon))


def scroll_build_blocks():
    """QScrollArea 通用样式（无色，无需按主题替换）：
    滚动内容容器统一 objectName=ScrollAreaContent，背景透明让主题透出。
    另加 viewport 透明兜底——只设内容透明的话，背后 viewport 仍可能是白底。"""
    return '''QScrollArea {
    border: none;
    background: transparent;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}
#ScrollAreaContent {
    background: transparent;
}
'''


def groupbox_build_blocks(c):
    """全局 QGroupBox 样式：标题 pill（纯色 accent）+ hover/选中描边。
    所有 background 用纯色（不用 qlineargradient）：框体 panelBackground
    （与其它容器一致），标题 accentColor。标题放框内顶部——不能用
    subcontrol-origin:margin + 负 top，QSS margin 不占真实布局空间，负偏移
    会把标题画到控件矩形上方被父级裁剪。"""
    panel = c['panelBackground']
    border = c['borderColor']
    accent = c['accentColor']
    accent_dk = accent_darker(accent, 0.15)
    text = c['textColor']
    return ('''QGroupBox {
    background-color: ''' + panel + ''';
    border: 1px solid ''' + border + ''';
    border-radius: 8px;
    margin-top: 0px;
    padding: 22px 12px 12px 12px;
    color: ''' + text + ''';
}
QGroupBox::title {
    subcontrol-origin: padding;
    subcontrol-position: top left;
    left: 12px;
    top: 6px;
    padding: 2px 8px;
    background-color: ''' + accent + ''';
    border-radius: 4px;
    color: #FFFFFF;
    font-size: 12px;
    font-weight: 700;
}
QGroupBox:hover {
    border: 1px solid ''' + accent + ''';
}
QGroupBox[selected="true"] {
    border: 2px solid ''' + accent + ''';
}
QGroupBox[selected="true"]::title {
    background-color: ''' + accent_dk + ''';
}
''')


def subtitle_build_blocks(colors):
    """visualizer 副标题 [state=] 警示色（未绑定 / 连接已删除），读 colors['warning']"""
    warn = colors.get('warning', '#D56224')
    sel_warning = ('#WaveformSubtitle[state="warning"], #MeterSubtitle[state="warning"], '
                   '#GaugeSubtitle[state="warning"], #ValueLabelSubtitle[state="warning"], '
                   '#LedSubtitle[state="warning"]')
    sel_deleted = sel_warning.replace('"warning"', '"deleted"')
    return ('\n' + sel_warning + ' {\n  color: ' + warn + ';\n}\n'
            + sel_deleted + ' {\n  color: ' + warn + ';\n}\n')


def inject_widget_styles():
    """为所有主题 QSS 注入 QSpinBox/QComboBox/QScrollArea 主题适配样式"""
    for json_path in sorted(glob.glob(os.path.join(themes_dir, '*.json'))):
        theme_id = os.path.basename(json_path)[:-5]
        qss_path = os.path.join(styles_dir, theme_id + '.qss')
        if not os.path.exists(qss_path):
            print('  SKIP (no qss): {}'.format(theme_id))
            continue
        with open(json_path, encoding='utf-8') as f:
            data = json.load(f)
        colors = data['colors']
        icon = 'light' if data.get('isDark', False) else 'dark'

        with open(qss_path, encoding='utf-8') as f:
            qss = f.read()
        qss = strip_marked(qss, MARK_SPIN_START, MARK_SPIN_END)
        qss = strip_marked(qss, MARK_COMBO_START, MARK_COMBO_END)
        qss = strip_marked(qss, MARK_SCROLL_START, MARK_SCROLL_END)
        qss = strip_marked(qss, MARK_GROUPBOX_START, MARK_GROUPBOX_END)
        qss = strip_marked(qss, MARK_SUBTITLE_START, MARK_SUBTITLE_END)

        spin_block = ('\n' + MARK_SPIN_START + '\n'
                      + spin_build_blocks(colors, icon) + MARK_SPIN_END + '\n')
        combo_block = ('\n' + MARK_COMBO_START + '\n'
                       + combo_build_blocks(colors, icon) + MARK_COMBO_END + '\n')
        scroll_block = ('\n' + MARK_SCROLL_START + '\n'
                        + scroll_build_blocks() + MARK_SCROLL_END + '\n')
        groupbox_block = ('\n' + MARK_GROUPBOX_START + '\n'
                          + groupbox_build_blocks(colors) + MARK_GROUPBOX_END + '\n')
        subtitle_block = ('\n' + MARK_SUBTITLE_START + '\n'
                          + subtitle_build_blocks(colors) + MARK_SUBTITLE_END + '\n')
        qss = (qss.rstrip() + '\n' + spin_block + combo_block
               + scroll_block + groupbox_block + subtitle_block)

        with open(qss_path, 'w', encoding='utf-8') as f:
            f.write(qss)
        print('  INJECT: {}'.format(theme_id))


themes = [
    {
        'id': 'avocado_green',
        'displayName': 'Niuyouguo Green Avocado Green',
        'accent': '#8A9A5B',
    },
    {
        'id': 'mocha_brown',
        'displayName': 'Moka Brown Mocha Brown',
        'accent': '#967259',
    },
    {
        'id': 'hermes_orange',
        'displayName': 'Aimashicheng Hermes Orange',
        'accent': '#F3702A',
    },
    {
        'id': 'bright_yellow',
        'displayName': 'Minghuang Bright Yellow',
        'accent': '#FFD700',
    },
    {
        'id': 'cyan',
        'displayName': '青色',
        'accent': '#008080',
    },
]

# --widgets 参数：仅注入 QSpinBox/QComboBox 样式，不重新生成主题
if '--widgets' in sys.argv:
    inject_widget_styles()
    print('\nAll done!')
    sys.exit(0)

for t in themes:
    a = t['accent']  # accent color
    p1 = accent_lighter(a, 0.92)  # window bg (very light)
    p2 = accent_lighter(a, 0.85)  # panel bg
    p3 = accent_lighter(a, 0.75)  # toolbar bg
    p4 = accent_lighter(a, 0.60)  # hover bg
    accent_dk = accent_darker(a, 0.12)  # accent for UI
    acc_st = accent_darker(a, 0.08)  # status bar
    txt = '#2A2015'
    sec = accent_darker(a, 0.55)
    dis = accent_lighter(accent_darker(a, 0.40), 0.3)

    colors = {
        'windowBackground': p1,
        'panelBackground': p2,
        'toolbarBackground': p3,
        'hoverBackground': p4,
        'selectionBackground': a,
        'tabSelectedBackground': '#FFFFFF',
        'borderColor': p4,
        'textColor': txt,
        'secondaryTextColor': sec,
        'disabledTextColor': dis,
        'accentColor': accent_dk,
        'statusBarBackground': acc_st,
        'clockFaceBackground': p2,
        'clockHandColor': txt,
        'clockSecondaryColor': sec,
        'clockAccentColor': a,
    }

    data = {
        'themeId': t['id'],
        'displayName': t['displayName'],
        'isDark': False,
        'ribbonBaseTheme': 2,
        'colors': colors,
        'editorColors': {
            'editor/theme/paper': p1,
            'editor/theme/text': txt,
            'editor/theme/caret_line': p2,
            'editor/theme/caret': txt,
            'editor/theme/selection_bg': p4,
            'editor/theme/selection_fg': '#000000',
            'editor/theme/margin_bg': p2,
            'editor/theme/line_number': sec,
            'editor/theme/indent_guide': p4,
            'editor/theme/brace_light_bg': p4,
            'editor/theme/brace_light_fg': '#000000',
            'editor/theme/brace_bad_bg': '#E57373',
            'editor/theme/brace_bad_fg': '#FFFFFF',
            'editor/theme/fold_margin': sec,
            'editor/syntax/keyword': accent_dk,
            'editor/syntax/comment': sec,
            'editor/syntax/string': '#A31515',
            'editor/syntax/number': '#098658',
            'editor/syntax/function': '#795E26',
            'editor/syntax/tag': accent_dk,
            'editor/syntax/preprocessor': sec,
            'editor/syntax/global_class': '#267F99',
            'editor/syntax/escape_seq': '#E57373',
            'editor/syntax/property': '#795E26',
            'editor/syntax/operator': txt,
        }
    }

    fp = '{}/{}.json'.format(themes_dir, t['id'])
    with open(fp, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print('JSON: {}'.format(fp))

    # ---- QSS file based on vscode.qss ----
    with open('{}/vscode.qss'.format(styles_dir), 'r', encoding='utf-8') as f:
        qss = f.read()

    qss = qss.replace('VSCode Dark Theme for ETest Demo', '{} Light Theme for ETest Studio'.format(t['displayName']))

    # Color replacements: vscode dark -> light with accent
    # Handle both uppercase and lowercase hex in the source
    def repl(old_upper, new_val):
        global qss
        qss = qss.replace(old_upper, new_val)
        qss = qss.replace(old_upper.lower(), new_val)
        return qss

    # Main backgrounds
    qss = repl('#1E1E1E', p1)
    qss = repl('#252526', p2)
    qss = repl('#2D2D2D', p3)
    qss = repl('#3C3C3C', p3)
    qss = repl('#333333', p3)
    qss = repl('#2A2D2E', p4)
    qss = repl('#2A2A2B', p2)
    qss = repl('#252525', p2)

    # Hover/press/selection
    qss = qss.replace('#505050', p4)
    qss = qss.replace('#383838', p3)
    qss = qss.replace('#404040', p4)
    qss = qss.replace('#094771', a)
    qss = qss.replace('#264F78', p4)
    qss = qss.replace('#434343', p4)
    qss = qss.replace('#454545', p4)
    qss = qss.replace('#424242', p4)
    qss = qss.replace('#4F4F4F', p4)
    qss = qss.replace('#3F3F3F', p3)

    # Text
    qss = qss.replace('#CCCCCC', txt)
    qss = qss.replace('#858585', sec)
    qss = qss.replace('#5A5A5A', dis)
    qss = qss.replace('#888888', sec)
    qss = qss.replace('#969696', sec)
    qss = qss.replace('#999999', sec)
    qss = qss.replace('#aaaaaa', sec)
    qss = qss.replace('#BBBBBB', sec)
    qss = qss.replace('#E0E0E0', txt)
    qss = qss.replace('#666666', sec)
    qss = qss.replace('#555555', dis)

    # Accent
    qss = qss.replace('#007ACC', accent_dk)
    qss = qss.replace('#006BAB', accent_darker(accent_dk, 0.15))
    qss = qss.replace('#0E639C', accent_dk)
    qss = qss.replace('#1177BB', a)
    qss = qss.replace('#0D5A8F', accent_darker(accent_dk, 0.10))
    qss = qss.replace('#0A4A75', accent_darker(accent_dk, 0.20))
    qss = qss.replace('#1A6BB5', a)
    qss = qss.replace('#75BEFF', a)
    qss = qss.replace('#4A9EFF', accent_dk)
    qss = qss.replace('#2196F3', accent_dk)
    qss = qss.replace('#2B6CB0', accent_darker(accent_dk, 0.10))
    qss = qss.replace('#F0F4FA', p2)
    qss = qss.replace('#E0E8F5', p3)
    qss = qss.replace('#F8F9FA', p2)

    # Special
    qss = qss.replace('#1A8FE3', accent_dk)
    qss = qss.replace('#0A5CA8', accent_darker(accent_dk, 0.15))
    qss = qss.replace('#1565C0', accent_darker(accent_dk, 0.10))
    qss = qss.replace('#0D47A1', accent_darker(accent_dk, 0.20))
    qss = qss.replace('#42A5F5', a)
    qss = qss.replace('#1E88E5', accent_dk)
    qss = qss.replace('#4527A0', accent_darker(accent_dk, 0.25))
    qss = qss.replace('#4FC3F7', a)
    qss = qss.replace('#0A3A5C', accent_darker(accent_dk, 0.40))
    qss = qss.replace('#40B0EE', accent_dk)
    qss = qss.replace('#90DDFF', a)
    qss = qss.replace('#ff8800', a)
    qss = qss.replace('#808080', sec)
    qss = qss.replace('#4ec9b0', '#267F99')
    qss = qss.replace('#4EC9B0', '#267F99')
    qss = qss.replace('#F48771', '#E53935')
    qss = qss.replace('#3C2020', '#4A2020')
    qss = qss.replace('#F44747', '#E53935')
    qss = qss.replace('#888', sec)
    qss = qss.replace('#aaa', sec)
    qss = qss.replace('#666', sec)
    qss = qss.replace('#555', dis)
    qss = qss.replace('#ffffff', '#FFFFFF')
    qss = qss.replace('#FFFFFF', '#FFFFFF')

    # Fix disabled RibbonSearchEdit border

    # Fix disabled RibbonSearchEdit border
    # (background and border are same color in disabled state)
    qss = qss.replace('border: 1px solid {};'.format(p3), 'border: 1px solid {};'.format(p4))

    # 主 QSS 从 vscode.qss 复制的 QADS 注释同步修正（正则通配，vscode 注释改为 ads_vscode 后仍可命中）
    qss = re.sub(r'已迁移至 ads_\w+\.qss',
                 '已迁移至 ads_{}.qss'.format(t['id']), qss)

    fp = '{}/{}.qss'.format(styles_dir, t['id'])
    with open(fp, 'w', encoding='utf-8') as f:
        f.write(qss)
    print('  QSS: {}'.format(fp))

    # ---- Ribbon QSS based on theme-office2021-blue.qss + QToolButton block ----
    with open('3rdparty/SARibbon-2.5.7/src/SARibbonBar/resource/theme-office2021-blue.qss', 'r', encoding='utf-8') as f:
        ribbon = f.read()

    ribbon = ribbon.replace('#e5e3e5', p3)
    ribbon = ribbon.replace('#242424', txt)
    ribbon = ribbon.replace('#bec0c2', p4)
    ribbon = ribbon.replace('#ffffff', '#FFFFFF')
    ribbon = ribbon.replace('#FFFFFF', '#FFFFFF')
    ribbon = ribbon.replace('#666666', sec)
    ribbon = ribbon.replace('#606060', p4)
    ribbon = ribbon.replace('#333', txt)
    ribbon = ribbon.replace('c5c5c5', p4)
    ribbon = ribbon.replace('#f1f1f1', p3)
    ribbon = ribbon.replace('#d0ced1', p4)
    ribbon = ribbon.replace('#2760a7', accent_dk)
    ribbon = ribbon.replace('#269bf4', accent_dk)
    ribbon = ribbon.replace('#5f5f5f', sec)
    ribbon = ribbon.replace('#ebebeb', p3)
    ribbon = ribbon.replace('#e1e1e1', p4)
    ribbon = ribbon.replace('#f5f6f6', p3)
    ribbon = ribbon.replace('#C0C2C4', p4)
    ribbon = ribbon.replace('#9BBBF7', p4)
    ribbon = ribbon.replace('#c2d0df', p4)
    ribbon = ribbon.replace('#FDEEB3', p4)
    ribbon = ribbon.replace('#9ed2f9', a)
    ribbon = ribbon.replace('#c6c6c6', p4)
    ribbon = ribbon.replace('#FCFCFC', p1)
    ribbon = ribbon.replace('#e81123', '#CC3333')
    ribbon = ribbon.replace('#f1707a', '#E55555')
    ribbon = ribbon.replace('#cacacb', p4)

    # Fix double hashes from c5c5c5 replacement
    ribbon = ribbon.replace('##', '#')

    # Fix SARibbonPanelLabel: give it a visible background (not transparent)
    # so the label area is visually distinct from the white panel
    ribbon = ribbon.replace(
        'SARibbonPanelLabel {\n    background-color: transparent;',
        'SARibbonPanelLabel {\n    background-color: ' + p3 + ';')

    # Insert QToolButton block
    qtool_block = '''
/* SARibbonButtonGroupWidget\xe4\xb8\x8b\xe6\x8c\x89\xe9\x92\xae\xe8\xae\xbe\xe7\xbd\xae*/
SARibbonButtonGroupWidget > QToolButton {
    border: none;
    color: ''' + txt + ''';
    background-color: transparent;
    padding: 0 2px;
}
SARibbonButtonGroupWidget > QToolButton:hover {
    background-color: ''' + p4 + ''';
}
SARibbonButtonGroupWidget > QToolButton:pressed{
    border: 1px solid ''' + sec + ''';
    background-color: ''' + a + ''';
}
SARibbonButtonGroupWidget > QToolButton:checked {
    background-color: ''' + a + ''';
    border:1px solid ''' + accent_dk + ''';
}
SARibbonButtonGroupWidget > QToolButton[popupMode="1"]{
    padding-right: 12px;
}
SARibbonButtonGroupWidget > QToolButton[popupMode="1"]::menu-button:hover {
    background-color: ''' + a + ''';
}
SARibbonButtonGroupWidget > QToolButton[popupMode="1"]::menu-button:pressed {
    background-color: ''' + accent_dk + ''';
}
SARibbonButtonGroupWidget > QToolButton[popupMode="2"] {
    padding-right: 8px;
}
SARibbonButtonGroupWidget > QToolButton[popupMode="0"]{
    padding: 0 2px;
}
SARibbonQuickAccessBar { background-color: transparent; }

'''

    ribbon = ribbon.replace(
        '/*SARibbonButtonGroupWidget*/\nSARibbonButtonGroupWidget {\n  background-color: transparent;\n}\n\n\n\n/*SARibbonCtrlContainer*/',
        '/*SARibbonButtonGroupWidget*/\nSARibbonButtonGroupWidget {\n  background-color: transparent;\n}\n\n' + qtool_block + '/*SARibbonCtrlContainer*/')

    fp = '{}/ribbon_{}.qss'.format(styles_dir, t['id'])
    with open(fp, 'w', encoding='utf-8') as f:
        f.write(ribbon)
    print('  Ribbon: {}'.format(fp))

    # ---- QADS QSS（每主题 dock 样式，亮色模板）----
    with open('{}/ads_template.qss'.format(styles_dir), 'r', encoding='utf-8') as f:
        ads = f.read()
    ads = ads.replace('@WINDOW_BG@', p1)
    ads = ads.replace('@PANEL_BG@', p2)
    ads = ads.replace('@HOVER_BG@', p4)
    ads = ads.replace('@TEXT@', txt)
    ads = ads.replace('@SECONDARY_TEXT@', sec)
    ads = ads.replace('@ACCENT@', accent_dk)  # = JSON accentColor，与手写亮色主题的 @ACCENT@ 语义一致
    ads = ads.replace('@ICON_VARIANT@', 'dark')

    fp = '{}/ads_{}.qss'.format(styles_dir, t['id'])
    with open(fp, 'w', encoding='utf-8') as f:
        f.write(ads)
    print('  ADS: {}'.format(fp))

# 主题生成后注入 QSpinBox/QComboBox 主题适配样式
inject_widget_styles()

print('\nAll done!')
