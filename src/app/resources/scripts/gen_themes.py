# -*- coding: utf-8 -*-
import json, os, re

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
]

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

print('\nAll done!')
