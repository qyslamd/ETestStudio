#ifndef ETEST_APP_TEXT_EDITOR_WIDGET_H_
#define ETEST_APP_TEXT_EDITOR_WIDGET_H_

#include <Qsci/qsciscintilla.h>
#include <QColor>
#include <QWidget>
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "api/IEditor.h"

namespace etest::app {

struct EditorTheme {
  struct Chrome {
    QColor paper{"#1E1E1E"};
    QColor text{"#CCCCCC"};
    QColor caretLine{"#2E2E2E"};
    QColor caret{"#CCCCCC"};
    QColor selectionBg{"#264F78"};
    QColor selectionFg{"#FFFFFF"};
    QColor marginBg{"#252525"};
    QColor lineNumber{"#858585"};
    QColor indentGuide{"#434343"};
    QColor braceLightBg{"#264F78"};
    QColor braceLightFg{"#FFFFFF"};
    QColor braceBadBg{"#8B0000"};
    QColor braceBadFg{"#FFFFFF"};
    QColor foldMargin{"#858585"};
  } chrome;

  struct Syntax {
    QColor keyword{"#569CD6"};
    QColor comment{"#6A9955"};
    QColor string{"#CE9178"};
    QColor number{"#B5CEA8"};
    QColor function{"#DCDCAA"};
    QColor tag{"#569CD6"};
    QColor preprocessor{"#9B9B9B"};
    QColor globalClass{"#4EC9B0"};
    QColor escapeSeq{"#D7BA7D"};
    QColor property{"#DCDCAA"};
    QColor oper{"#CCCCCC"};
  } syntax;

  static EditorTheme loadFromConfig() {
    using namespace core::config;
    EditorTheme theme;
    auto& cfg = ConfigManager::instance();

    auto load = [&](const char* key, QColor& color) {
      QString val = cfg.get<QString>(key, QString());
      if (!val.isEmpty()) color = QColor(val);
    };

    load(CONFIG_EDITOR_THEME_PAPER,         theme.chrome.paper);
    load(CONFIG_EDITOR_THEME_TEXT,          theme.chrome.text);
    load(CONFIG_EDITOR_THEME_CARET_LINE,    theme.chrome.caretLine);
    load(CONFIG_EDITOR_THEME_CARET,         theme.chrome.caret);
    load(CONFIG_EDITOR_THEME_SELECTION_BG,  theme.chrome.selectionBg);
    load(CONFIG_EDITOR_THEME_SELECTION_FG,  theme.chrome.selectionFg);
    load(CONFIG_EDITOR_THEME_MARGIN_BG,     theme.chrome.marginBg);
    load(CONFIG_EDITOR_THEME_LINE_NUMBER,   theme.chrome.lineNumber);
    load(CONFIG_EDITOR_THEME_INDENT_GUIDE,  theme.chrome.indentGuide);
    load(CONFIG_EDITOR_THEME_BRACE_LIGHT_BG, theme.chrome.braceLightBg);
    load(CONFIG_EDITOR_THEME_BRACE_LIGHT_FG, theme.chrome.braceLightFg);
    load(CONFIG_EDITOR_THEME_BRACE_BAD_BG,  theme.chrome.braceBadBg);
    load(CONFIG_EDITOR_THEME_BRACE_BAD_FG,  theme.chrome.braceBadFg);
    load(CONFIG_EDITOR_THEME_FOLD_MARGIN,   theme.chrome.foldMargin);

    load(CONFIG_EDITOR_SYNTAX_KEYWORD,      theme.syntax.keyword);
    load(CONFIG_EDITOR_SYNTAX_COMMENT,      theme.syntax.comment);
    load(CONFIG_EDITOR_SYNTAX_STRING,       theme.syntax.string);
    load(CONFIG_EDITOR_SYNTAX_NUMBER,       theme.syntax.number);
    load(CONFIG_EDITOR_SYNTAX_FUNCTION,     theme.syntax.function);
    load(CONFIG_EDITOR_SYNTAX_TAG,          theme.syntax.tag);
    load(CONFIG_EDITOR_SYNTAX_PREPROCESSOR, theme.syntax.preprocessor);
    load(CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS, theme.syntax.globalClass);
    load(CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ,   theme.syntax.escapeSeq);
    load(CONFIG_EDITOR_SYNTAX_PROPERTY,     theme.syntax.property);
    load(CONFIG_EDITOR_SYNTAX_OPERATOR,     theme.syntax.oper);

    return theme;
  }
};

class TextEditorWidget : public QWidget, public IEditor {
  Q_OBJECT

 public:
  explicit TextEditorWidget(const QString& filePath, QWidget* parent = nullptr);

  // IEditor interface
  QString displayName() const override;
  bool isModified() const override;
  bool save() override;
  bool saveAs(const QString& path) override;
  QString filePath() const override;
  QString editorId() const override;
  QWidget* widget() override;
  QString editorType() const override;
  QObject* signalObject() override;

  // Undo/Redo
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;

  // Text editor specific
  QString fileName() const;
  void setFilePath(const QString& newPath);
  bool loadFile();
  bool saveFile();
  bool saveFileAs(const QString& newPath);
  QsciScintilla* editor() const;

 Q_SIGNALS:
  void modificationChanged(bool modified);
  void editorStateChanged();

 private slots:
  void onConfigChanged(const QString& key);

 private:
  void setupEditor();
  void applyLexer(const QString& suffix);
  void applyColorScheme(QsciLexer* lexer);

  bool eventFilter(QObject* obj, QEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

  QsciScintilla* editor_;
  QString file_path_;
  EditorTheme theme_;
};

}  // namespace etest::app

#endif  // ETEST_APP_TEXT_EDITOR_WIDGET_H_
