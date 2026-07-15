#include "TextEditorWidget.h"
#include "config/ConfigDefs.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QTextStream>
#include <QVBoxLayout>

#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexeryaml.h>

namespace etest::app {

TextEditorWidget::TextEditorWidget(const QString& filePath, QWidget* parent)
    : QWidget(parent),
      file_path_(filePath),
      theme_(EditorTheme::loadFromConfig()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  editor_ = new QsciScintilla(this);
  layout->addWidget(editor_);

  editor_->installEventFilter(this);
  editor_->viewport()->installEventFilter(this);

  setupEditor();

  QFileInfo fi(filePath);
  applyLexer(fi.suffix().toLower());

  loadFile();

  connect(editor_, &QsciScintilla::modificationChanged, this,
          &TextEditorWidget::modificationChanged);

  connect(editor_, &QsciScintilla::textChanged, this, [this]() {
    if (editor_->isModified()) {
      emit modificationChanged(true);
    }
  });

  connect(editor_, &QsciScintilla::textChanged, this,
          &TextEditorWidget::editorStateChanged);
}

// ── IEditor interface ──────────────────────────────────────────

QString TextEditorWidget::displayName() const {
  return fileName();
}

bool TextEditorWidget::isModified() const {
  return editor_->isModified();
}

bool TextEditorWidget::save() {
  return saveFile();
}

bool TextEditorWidget::saveAs(const QString& path) {
  return saveFileAs(path);
}

QString TextEditorWidget::filePath() const {
  return file_path_;
}

QString TextEditorWidget::editorId() const {
  return file_path_;
}

QWidget* TextEditorWidget::widget() {
  return this;
}

QString TextEditorWidget::editorType() const {
  return QStringLiteral("text");
}

QObject* TextEditorWidget::signalObject() {
  return this;
}

bool TextEditorWidget::canUndo() const {
  return editor_->isUndoAvailable();
}

bool TextEditorWidget::canRedo() const {
  return editor_->isRedoAvailable();
}

void TextEditorWidget::undo() {
  editor_->undo();
}

void TextEditorWidget::redo() {
  editor_->redo();
}

void TextEditorWidget::setReadOnly(bool readOnly) {
  editor_->setReadOnly(readOnly);
}

// ── Text editor specific ───────────────────────────────────────

QString TextEditorWidget::fileName() const {
  return QFileInfo(file_path_).fileName();
}

void TextEditorWidget::setFilePath(const QString& newPath) {
  file_path_ = newPath;
  QFileInfo fi(newPath);
  applyLexer(fi.suffix().toLower());
  emit modificationChanged(editor_->isModified());
}

bool TextEditorWidget::loadFile() {
  QFile file(file_path_);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  QTextStream in(&file);
  in.setCodec("UTF-8");
  editor_->setText(in.readAll());
  editor_->setModified(false);
  return true;
}

bool TextEditorWidget::saveFile() {
  if (file_path_.isEmpty())
    return false;
  QFile file(file_path_);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  QTextStream out(&file);
  out.setCodec("UTF-8");
  out << editor_->text();
  editor_->setModified(false);
  emit modificationChanged(false);
  return true;
}

bool TextEditorWidget::saveFileAs(const QString& newPath) {
  QString oldPath = file_path_;
  file_path_ = newPath;
  if (!saveFile()) {
    file_path_ = oldPath;
    return false;
  }
  QFileInfo fi(newPath);
  applyLexer(fi.suffix().toLower());
  return true;
}

QsciScintilla* TextEditorWidget::editor() const {
  return editor_;
}

void TextEditorWidget::setupEditor() {
  using namespace etest::core::config;

  ConfigManager& config = ConfigManager::instance();

  bool showLineNumber = config.get<bool>(
      CONFIG_EDITOR_SHOW_LINE_NUMBER, CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER);
  editor_->setMarginType(0, QsciScintilla::NumberMargin);
  editor_->setMarginWidth(0, "0000");
  editor_->setMarginLineNumbers(0, showLineNumber);

  editor_->setFolding(QsciScintilla::PlainFoldStyle);

  bool autoIndent = config.get<bool>(CONFIG_EDITOR_AUTO_INDENT,
                                     CONFIG_EDITOR_DEFAULT_AUTO_INDENT);
  editor_->setAutoIndent(autoIndent);
  editor_->setIndentationGuides(true);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                         QsciScintilla::STYLE_INDENTGUIDE,
                         theme_.chrome.indentGuide);

  int tabWidth =
      config.get<int>(CONFIG_EDITOR_TAB_WIDTH, CONFIG_EDITOR_DEFAULT_TAB_WIDTH);
  editor_->setTabWidth(tabWidth);

  bool spacesForTab = config.get<bool>(CONFIG_EDITOR_SPACES_FOR_TAB,
                                       CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB);
  editor_->setIndentationsUseTabs(!spacesForTab);

  editor_->setWrapMode(QsciScintilla::WrapNone);

  editor_->setCaretLineVisible(true);
  editor_->setCaretLineBackgroundColor(theme_.chrome.caretLine);

  editor_->setPaper(theme_.chrome.paper);
  editor_->setColor(theme_.chrome.text);
  editor_->setCaretForegroundColor(theme_.chrome.caret);
  editor_->setSelectionBackgroundColor(theme_.chrome.selectionBg);
  editor_->setSelectionForegroundColor(theme_.chrome.selectionFg);

  editor_->setMarginBackgroundColor(0, theme_.chrome.marginBg);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
                         QsciScintilla::STYLE_LINENUMBER,
                         theme_.chrome.marginBg);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                         QsciScintilla::STYLE_LINENUMBER,
                         theme_.chrome.lineNumber);
  editor_->setMarginType(1, QsciScintilla::SymbolMargin);
  editor_->setMarginWidth(1, 16);
  editor_->setMarginBackgroundColor(1, theme_.chrome.marginBg);
  editor_->setMarginBackgroundColor(2, theme_.chrome.marginBg);
  editor_->setMarginsForegroundColor(theme_.chrome.lineNumber);
  editor_->setFoldMarginColors(theme_.chrome.foldMargin,
                               theme_.chrome.marginBg);

  editor_->setBraceMatching(QsciScintilla::SloppyBraceMatch);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
                         QsciScintilla::STYLE_BRACELIGHT,
                         theme_.chrome.braceLightBg);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                         QsciScintilla::STYLE_BRACELIGHT,
                         theme_.chrome.braceLightFg);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
                         QsciScintilla::STYLE_BRACEBAD,
                         theme_.chrome.braceBadBg);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                         QsciScintilla::STYLE_BRACEBAD,
                         theme_.chrome.braceBadFg);

  editor_->setUtf8(true);

  int fontSize =
      config.get<int>(CONFIG_EDITOR_FONT_SIZE, CONFIG_EDITOR_DEFAULT_FONT_SIZE);
  QFont font = editor_->font();
  font.setFamily("Consolas");
  font.setPointSize(fontSize);
  editor_->setFont(font);
  editor_->setMarginsFont(font);

  connect(&config, &ConfigManager::configChanged, this,
          &TextEditorWidget::onConfigChanged);
}

void TextEditorWidget::applyLexer(const QString& suffix) {
  // 删除旧的 lexer 防止内存泄漏
  QsciLexer* oldLexer = editor_->lexer();
  if (oldLexer) {
    editor_->setLexer(nullptr);
    delete oldLexer;
  }

  QsciLexer* lexer = nullptr;

  if (suffix == "cpp" || suffix == "h" || suffix == "c" || suffix == "hpp" ||
      suffix == "cc" || suffix == "cxx") {
    lexer = new QsciLexerCPP(this);
  } else if (suffix == "lua") {
    lexer = new QsciLexerLua(this);
  } else if (suffix == "json" || suffix == "etproj" || suffix == "etopo" ||
             suffix == "eproto" || suffix == "etprog") {
    lexer = new QsciLexerJSON(this);
  } else if (suffix == "xml" || suffix == "html" || suffix == "htm" ||
             suffix == "svg" || suffix == "eprotox") {
    lexer = new QsciLexerXML(this);
  } else if (suffix == "py" || suffix == "pyw") {
    lexer = new QsciLexerPython(this);
  } else if (suffix == "yaml" || suffix == "yml") {
    lexer = new QsciLexerYAML(this);
  } else if (suffix == "md") {
    lexer = new QsciLexerMarkdown(this);
  } else if (suffix == "cmake") {
    lexer = new QsciLexerCMake(this);
  } else if (suffix == "js") {
    lexer = new QsciLexerJavaScript(this);
  }

  if (lexer) {
    applyColorScheme(lexer);
    editor_->setLexer(lexer);
  } else {
    editor_->setLexer(nullptr);
  }
}

bool TextEditorWidget::eventFilter(QObject* obj, QEvent* event) {
  if (event->type() == QEvent::ShortcutOverride) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->modifiers() == Qt::ControlModifier &&
        (ke->key() == Qt::Key_S || ke->key() == Qt::Key_W)) {
      ke->ignore();
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void TextEditorWidget::applyColorScheme(QsciLexer* lexer) {
  auto& s = theme_.syntax;

  if (lexer) {
    lexer->setDefaultPaper(theme_.chrome.paper);
    lexer->setColor(theme_.chrome.text, 0);
    for (int i = 0; i <= 127; ++i) {
      lexer->setPaper(theme_.chrome.paper, i);
    }
  }

  if (auto* cpp = qobject_cast<QsciLexerCPP*>(lexer)) {
    cpp->setColor(s.keyword, QsciLexerCPP::Keyword);
    cpp->setColor(s.comment, QsciLexerCPP::Comment);
    cpp->setColor(s.comment, QsciLexerCPP::CommentLine);
    cpp->setColor(s.string, QsciLexerCPP::DoubleQuotedString);
    cpp->setColor(s.string, QsciLexerCPP::SingleQuotedString);
    cpp->setColor(s.number, QsciLexerCPP::Number);
    cpp->setColor(s.oper, QsciLexerCPP::Operator);
    cpp->setColor(s.preprocessor, QsciLexerCPP::PreProcessor);
    cpp->setColor(s.globalClass, QsciLexerCPP::GlobalClass);
    cpp->setColor(s.escapeSeq, QsciLexerCPP::EscapeSequence);
    cpp->setColor(s.comment, QsciLexerCPP::CommentDoc);
  } else if (auto* lua = qobject_cast<QsciLexerLua*>(lexer)) {
    lua->setColor(s.keyword, QsciLexerLua::Keyword);
    lua->setColor(s.comment, QsciLexerLua::Comment);
    lua->setColor(s.comment, QsciLexerLua::LineComment);
    lua->setColor(s.string, QsciLexerLua::String);
    lua->setColor(s.string, QsciLexerLua::Character);
    lua->setColor(s.number, QsciLexerLua::Number);
    lua->setColor(s.function, QsciLexerLua::Identifier);
  } else if (auto* json = qobject_cast<QsciLexerJSON*>(lexer)) {
    json->setColor(s.keyword, QsciLexerJSON::Keyword);
    json->setColor(s.comment, QsciLexerJSON::CommentLine);
    json->setColor(s.comment, QsciLexerJSON::CommentBlock);
    json->setColor(s.string, QsciLexerJSON::String);
    json->setColor(s.number, QsciLexerJSON::Number);
    json->setColor(s.oper, QsciLexerJSON::Operator);
    json->setColor(s.escapeSeq, QsciLexerJSON::EscapeSequence);
    json->setColor(s.property, QsciLexerJSON::Property);
  } else if (auto* xml = qobject_cast<QsciLexerXML*>(lexer)) {
    xml->setColor(s.tag, QsciLexerXML::Tag);
    xml->setColor(s.comment, QsciLexerXML::HTMLComment);
    xml->setColor(s.string, QsciLexerXML::HTMLDoubleQuotedString);
    xml->setColor(s.string, QsciLexerXML::HTMLSingleQuotedString);
    xml->setColor(s.number, QsciLexerXML::HTMLNumber);
    xml->setColor(theme_.chrome.text, QsciLexerHTML::Default);
  } else if (auto* py = qobject_cast<QsciLexerPython*>(lexer)) {
    py->setColor(s.keyword, QsciLexerPython::Keyword);
    py->setColor(s.comment, QsciLexerPython::Comment);
    py->setColor(s.string, QsciLexerPython::DoubleQuotedString);
    py->setColor(s.string, QsciLexerPython::SingleQuotedString);
    py->setColor(s.number, QsciLexerPython::Number);
    py->setColor(s.function, QsciLexerPython::FunctionMethodName);
  } else if (auto* yaml = qobject_cast<QsciLexerYAML*>(lexer)) {
    yaml->setColor(s.keyword, QsciLexerYAML::Keyword);
    yaml->setColor(s.comment, QsciLexerYAML::Comment);
    yaml->setColor(s.number, QsciLexerYAML::Number);
  } else if (auto* cmake = qobject_cast<QsciLexerCMake*>(lexer)) {
    cmake->setColor(s.function, QsciLexerCMake::Function);
    cmake->setColor(s.comment, QsciLexerCMake::Comment);
    cmake->setColor(s.string, QsciLexerCMake::String);
    cmake->setColor(s.number, QsciLexerCMake::Number);
  }
}

void TextEditorWidget::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);

  QAction* undoAction =
      menu.addAction(QStringLiteral("撤销"), editor_, &QsciScintilla::undo);
  undoAction->setEnabled(editor_->isUndoAvailable());
  undoAction->setShortcut(QKeySequence::Undo);

  QAction* redoAction =
      menu.addAction(QStringLiteral("重做"), editor_, &QsciScintilla::redo);
  redoAction->setEnabled(editor_->isRedoAvailable());
  redoAction->setShortcut(QKeySequence::Redo);

  menu.addSeparator();

  QAction* cutAction =
      menu.addAction(QStringLiteral("剪切"), editor_, &QsciScintilla::cut);
  cutAction->setEnabled(editor_->hasSelectedText());
  cutAction->setShortcut(QKeySequence::Cut);

  QAction* copyAction =
      menu.addAction(QStringLiteral("复制"), editor_, &QsciScintilla::copy);
  copyAction->setEnabled(editor_->hasSelectedText());
  copyAction->setShortcut(QKeySequence::Copy);

  QAction* pasteAction =
      menu.addAction(QStringLiteral("粘贴"), editor_, &QsciScintilla::paste);
  pasteAction->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());
  pasteAction->setShortcut(QKeySequence::Paste);

  QAction* deleteAction = menu.addAction(QStringLiteral("删除"), editor_,
                                         &QsciScintilla::removeSelectedText);
  deleteAction->setEnabled(editor_->hasSelectedText());

  menu.addSeparator();

  QAction* selectAllAction = menu.addAction(QStringLiteral("全选"), editor_,
                                            &QsciScintilla::selectAll);
  selectAllAction->setShortcut(QKeySequence::SelectAll);

  menu.exec(event->globalPos());
}

void TextEditorWidget::onConfigChanged(const QString& key) {
  using namespace etest::core::config;

  // Reload full theme when any editor color config changes
  if (key.startsWith("editor/theme/") || key.startsWith("editor/syntax/")) {
    theme_ = EditorTheme::loadFromConfig();
    // Re-apply all editor chrome colors
    editor_->setCaretLineBackgroundColor(theme_.chrome.caretLine);
    editor_->setPaper(theme_.chrome.paper);
    editor_->setColor(theme_.chrome.text);
    editor_->setCaretForegroundColor(theme_.chrome.caret);
    editor_->setSelectionBackgroundColor(theme_.chrome.selectionBg);
    editor_->setSelectionForegroundColor(theme_.chrome.selectionFg);
    editor_->setMarginBackgroundColor(0, theme_.chrome.marginBg);
    editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
                           QsciScintilla::STYLE_LINENUMBER,
                           theme_.chrome.marginBg);
    editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                           QsciScintilla::STYLE_LINENUMBER,
                           theme_.chrome.lineNumber);
    editor_->setMarginBackgroundColor(1, theme_.chrome.marginBg);
    editor_->setMarginBackgroundColor(2, theme_.chrome.marginBg);
    editor_->setMarginsForegroundColor(theme_.chrome.lineNumber);
    editor_->setFoldMarginColors(theme_.chrome.foldMargin,
                                 theme_.chrome.marginBg);
    editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                           QsciScintilla::STYLE_INDENTGUIDE,
                           theme_.chrome.indentGuide);
    editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
                           QsciScintilla::STYLE_BRACELIGHT,
                           theme_.chrome.braceLightBg);
    editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                           QsciScintilla::STYLE_BRACELIGHT,
                           theme_.chrome.braceLightFg);
    editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
                           QsciScintilla::STYLE_BRACEBAD,
                           theme_.chrome.braceBadBg);
    editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
                           QsciScintilla::STYLE_BRACEBAD,
                           theme_.chrome.braceBadFg);
    // Re-apply syntax colors if a lexer is active
    if (editor_->lexer()) {
      applyColorScheme(editor_->lexer());
    }
    return;
  }

  if (key == CONFIG_EDITOR_SHOW_LINE_NUMBER) {
    bool show = ConfigManager::instance().get<bool>(
        key, CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER);
    editor_->setMarginLineNumbers(0, show);
  } else if (key == CONFIG_EDITOR_AUTO_INDENT) {
    bool autoIndent = ConfigManager::instance().get<bool>(
        key, CONFIG_EDITOR_DEFAULT_AUTO_INDENT);
    editor_->setAutoIndent(autoIndent);
  } else if (key == CONFIG_EDITOR_TAB_WIDTH) {
    int tabWidth = ConfigManager::instance().get<int>(
        key, CONFIG_EDITOR_DEFAULT_TAB_WIDTH);
    editor_->setTabWidth(tabWidth);
  } else if (key == CONFIG_EDITOR_SPACES_FOR_TAB) {
    bool spacesForTab = ConfigManager::instance().get<bool>(
        key, CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB);
    editor_->setIndentationsUseTabs(!spacesForTab);
  } else if (key == CONFIG_EDITOR_FONT_SIZE) {
    int fontSize = ConfigManager::instance().get<int>(
        key, CONFIG_EDITOR_DEFAULT_FONT_SIZE);
    QFont font = editor_->font();
    font.setPointSize(fontSize);
    editor_->setFont(font);
    editor_->setMarginsFont(font);
  }
}

}  // namespace etest::app
