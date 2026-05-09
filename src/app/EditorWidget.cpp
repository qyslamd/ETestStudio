#include "EditorWidget.h"
#include "config/ConfigDefs.h"

#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QTextStream>
#include <QVBoxLayout>
#include <QMenu>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QClipboard>

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

EditorWidget::EditorWidget(const QString& filePath, QWidget* parent)
    : QWidget(parent), file_path_(filePath) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  editor_ = new QsciScintilla(this);
  layout->addWidget(editor_);

  // 拦截QsciScintilla的快捷键，防止被Scintilla引擎消费
  editor_->installEventFilter(this);
  editor_->viewport()->installEventFilter(this);

  setupEditor();

  QFileInfo fi(filePath);
  applyLexer(fi.suffix().toLower());

  loadFile();

  // 监听Scintilla的修改状态变化信号
  connect(editor_, &QsciScintilla::modificationChanged, this,
          &EditorWidget::modificationChanged);

  // 同时监听textChanged信号作为备用，确保修改状态被检测到
  connect(editor_, &QsciScintilla::textChanged, this, [this]() {
    // 当文本改变时，如果编辑器被标记为已修改，发射信号
    if (editor_->isModified()) {
      emit modificationChanged(true);
    }
  });

  // 文本变化时发射状态变化信号
  connect(editor_, &QsciScintilla::textChanged, this, &EditorWidget::editorStateChanged);
}

QString EditorWidget::filePath() const {
  return file_path_;
}

QString EditorWidget::fileName() const {
  return QFileInfo(file_path_).fileName();
}

bool EditorWidget::isModified() const {
  return editor_->isModified();
}

void EditorWidget::setFilePath(const QString& newPath) {
  file_path_ = newPath;
  // 如果后缀变了，重新应用语法高亮
  QFileInfo fi(newPath);
  applyLexer(fi.suffix().toLower());
  emit modificationChanged(editor_->isModified());
}

bool EditorWidget::loadFile() {
  QFile file(file_path_);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream in(&file);
  in.setCodec("UTF-8");
  editor_->setText(in.readAll());
  editor_->setModified(false);
  return true;
}

bool EditorWidget::saveFile() {
  if (file_path_.isEmpty())
    return false;
  QFile file(file_path_);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream out(&file);
  out.setCodec("UTF-8");
  out << editor_->text();
  editor_->setModified(false);
  emit modificationChanged(false);
  return true;
}

bool EditorWidget::saveFileAs(const QString& newPath) {
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

QsciScintilla* EditorWidget::editor() const {
  return editor_;
}

void EditorWidget::setupEditor() {
  using namespace etest::core::config;

  ConfigManager& config = ConfigManager::instance();

  // 行号
  bool showLineNumber = config.get<bool>(CONFIG_EDITOR_SHOW_LINE_NUMBER, CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER);
  editor_->setMarginType(0, QsciScintilla::NumberMargin);
  editor_->setMarginWidth(0, "0000");
  editor_->setMarginLineNumbers(0, showLineNumber);

  // 代码折叠
  editor_->setFolding(QsciScintilla::PlainFoldStyle);

  // 自动缩进
  bool autoIndent = config.get<bool>(CONFIG_EDITOR_AUTO_INDENT, CONFIG_EDITOR_DEFAULT_AUTO_INDENT);
  editor_->setAutoIndent(autoIndent);
  editor_->setIndentationGuides(true);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
      QsciScintilla::STYLE_INDENTGUIDE, QColor(67, 67, 67));

  int tabWidth = config.get<int>(CONFIG_EDITOR_TAB_WIDTH, CONFIG_EDITOR_DEFAULT_TAB_WIDTH);
  editor_->setTabWidth(tabWidth);

  bool spacesForTab = config.get<bool>(CONFIG_EDITOR_SPACES_FOR_TAB, CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB);
  editor_->setIndentationsUseTabs(!spacesForTab);

  // 换行模式
  editor_->setWrapMode(QsciScintilla::WrapNone);

  // 光标行高亮
  editor_->setCaretLineVisible(true);
  editor_->setCaretLineBackgroundColor(QColor(46, 46, 46));

  // 深色主题编辑器背景和基础颜色
  editor_->setPaper(QColor(30, 30, 30));
  editor_->setColor(QColor(204, 204, 204));
  editor_->setCaretForegroundColor(QColor(204, 204, 204));
  editor_->setSelectionBackgroundColor(QColor(38, 79, 120));
  editor_->setSelectionForegroundColor(QColor(255, 255, 255));

  // 行号区深色主题
  editor_->setMarginBackgroundColor(0, QColor(37, 37, 37));
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
      QsciScintilla::STYLE_LINENUMBER, QColor(37, 37, 37));
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
      QsciScintilla::STYLE_LINENUMBER, QColor(133, 133, 133));
  // 符号标记区 (margin 1) 深色主题
  editor_->setMarginType(1, QsciScintilla::SymbolMargin);
  editor_->setMarginWidth(1, 16);
  editor_->setMarginBackgroundColor(1, QColor(37, 37, 37));
  // 折叠区 (margin 2) 深色主题
  editor_->setMarginBackgroundColor(2, QColor(37, 37, 37));
  editor_->setMarginsForegroundColor(QColor(133, 133, 133));
  editor_->setFoldMarginColors(QColor(133, 133, 133), QColor(37, 37, 37));

  // 括号匹配
  editor_->setBraceMatching(QsciScintilla::SloppyBraceMatch);
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
      QsciScintilla::STYLE_BRACELIGHT, QColor(38, 79, 120));
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
      QsciScintilla::STYLE_BRACELIGHT, QColor(255, 255, 255));
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETBACK,
      QsciScintilla::STYLE_BRACEBAD, QColor(139, 0, 0));
  editor_->SendScintilla(QsciScintilla::SCI_STYLESETFORE,
      QsciScintilla::STYLE_BRACEBAD, QColor(255, 255, 255));

  // UTF-8
  editor_->setUtf8(true);

  // 字体大小
  int fontSize = config.get<int>(CONFIG_EDITOR_FONT_SIZE, CONFIG_EDITOR_DEFAULT_FONT_SIZE);
  QFont font = editor_->font();
  font.setFamily("Consolas");
  font.setPointSize(fontSize);
  editor_->setFont(font);
  editor_->setMarginsFont(font);

  // 连接配置变化信号
  connect(&config, &ConfigManager::configChanged, this, &EditorWidget::onConfigChanged);
}

void EditorWidget::applyLexer(const QString& suffix) {
  QsciLexer* lexer = nullptr;

  if (suffix == "cpp" || suffix == "h" || suffix == "c" || suffix == "hpp" ||
      suffix == "cc" || suffix == "cxx") {
    lexer = new QsciLexerCPP(this);
  } else if (suffix == "lua") {
    lexer = new QsciLexerLua(this);
  } else if (suffix == "json") {
    lexer = new QsciLexerJSON(this);
  } else if (suffix == "xml" || suffix == "html" || suffix == "htm" ||
             suffix == "svg") {
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

bool EditorWidget::eventFilter(QObject* obj, QEvent* event) {
  if (event->type() == QEvent::ShortcutOverride) {
    auto* ke = static_cast<QKeyEvent*>(event);
    // 让应用级快捷键（Ctrl+S/Ctrl+W等）冒泡到菜单栏，不被Scintilla消费
    if (ke->modifiers() == Qt::ControlModifier &&
        (ke->key() == Qt::Key_S || ke->key() == Qt::Key_W)) {
      ke->ignore();  // 告诉Qt这个控件不处理该快捷键
      return true;   // 拦截事件，不让QsciScintilla收到
    }
  }
  return QWidget::eventFilter(obj, event);
}

void EditorWidget::applyColorScheme(QsciLexer* lexer) {
  // VSCode Dark+ 风格配色
  QColor keyword(86, 156, 214);       // 蓝色关键字
  QColor comment(106, 153, 85);       // 绿色注释
  QColor string(206, 145, 120);       // 橙色字符串
  QColor number(181, 206, 168);       // 浅绿色数字
  QColor func(220, 220, 170);         // 黄色函数名
  QColor tag(86, 156, 214);           // 蓝色标签/XML标签

  // 设置所有lexer的默认背景色和前景色
  if (lexer) {
    lexer->setDefaultPaper(QColor(30, 30, 30));
    lexer->setDefaultColor(QColor(204, 204, 204));
    // 设置所有样式的背景色
    for (int i = 0; i <= 127; ++i) {
      lexer->setPaper(QColor(30, 30, 30), i);
    }
  }

  if (auto* cpp = qobject_cast<QsciLexerCPP*>(lexer)) {
    cpp->setColor(keyword, QsciLexerCPP::Keyword);
    cpp->setColor(comment, QsciLexerCPP::Comment);
    cpp->setColor(comment, QsciLexerCPP::CommentLine);
    cpp->setColor(string, QsciLexerCPP::DoubleQuotedString);
    cpp->setColor(string, QsciLexerCPP::SingleQuotedString);
    cpp->setColor(number, QsciLexerCPP::Number);
  } else if (auto* lua = qobject_cast<QsciLexerLua*>(lexer)) {
    lua->setColor(keyword, QsciLexerLua::Keyword);
    lua->setColor(comment, QsciLexerLua::Comment);
    lua->setColor(comment, QsciLexerLua::LineComment);
    lua->setColor(string, QsciLexerLua::String);
    lua->setColor(string, QsciLexerLua::Character);
    lua->setColor(number, QsciLexerLua::Number);
    lua->setColor(func, QsciLexerLua::Identifier);
  } else if (auto* json = qobject_cast<QsciLexerJSON*>(lexer)) {
    json->setColor(keyword, QsciLexerJSON::Keyword);
    json->setColor(comment, QsciLexerJSON::CommentLine);
    json->setColor(comment, QsciLexerJSON::CommentBlock);
    json->setColor(string, QsciLexerJSON::String);
    json->setColor(number, QsciLexerJSON::Number);
  } else if (auto* xml = qobject_cast<QsciLexerXML*>(lexer)) {
    xml->setColor(tag, QsciLexerXML::Tag);
    xml->setColor(comment, QsciLexerXML::HTMLComment);
    xml->setColor(string, QsciLexerXML::HTMLDoubleQuotedString);
    xml->setColor(string, QsciLexerXML::HTMLSingleQuotedString);
    xml->setColor(number, QsciLexerXML::HTMLNumber);
  } else if (auto* py = qobject_cast<QsciLexerPython*>(lexer)) {
    py->setColor(keyword, QsciLexerPython::Keyword);
    py->setColor(comment, QsciLexerPython::Comment);
    py->setColor(string, QsciLexerPython::DoubleQuotedString);
    py->setColor(string, QsciLexerPython::SingleQuotedString);
    py->setColor(number, QsciLexerPython::Number);
    py->setColor(func, QsciLexerPython::FunctionMethodName);
  } else if (auto* yaml = qobject_cast<QsciLexerYAML*>(lexer)) {
    yaml->setColor(keyword, QsciLexerYAML::Keyword);
    yaml->setColor(comment, QsciLexerYAML::Comment);
    yaml->setColor(number, QsciLexerYAML::Number);
  } else if (auto* cmake = qobject_cast<QsciLexerCMake*>(lexer)) {
    cmake->setColor(func, QsciLexerCMake::Function);
    cmake->setColor(comment, QsciLexerCMake::Comment);
    cmake->setColor(string, QsciLexerCMake::String);
    cmake->setColor(number, QsciLexerCMake::Number);
  }
  // QsciLexerJavaScript 继承自 QsciLexerCPP，qobject_cast<QsciLexerCPP*> 可匹配
  // QsciLexerMarkdown 无传统关键字/注释/字符串概念，不配色
}

void EditorWidget::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);

  // 撤销/重做
  QAction* undoAction = menu.addAction(QStringLiteral("撤销"), editor_, &QsciScintilla::undo);
  undoAction->setEnabled(editor_->isUndoAvailable());
  undoAction->setShortcut(QKeySequence::Undo);

  QAction* redoAction = menu.addAction(QStringLiteral("重做"), editor_, &QsciScintilla::redo);
  redoAction->setEnabled(editor_->isRedoAvailable());
  redoAction->setShortcut(QKeySequence::Redo);

  menu.addSeparator();

  // 剪切/复制/粘贴/删除
  QAction* cutAction = menu.addAction(QStringLiteral("剪切"), editor_, &QsciScintilla::cut);
  cutAction->setEnabled(editor_->hasSelectedText());
  cutAction->setShortcut(QKeySequence::Cut);

  QAction* copyAction = menu.addAction(QStringLiteral("复制"), editor_, &QsciScintilla::copy);
  copyAction->setEnabled(editor_->hasSelectedText());
  copyAction->setShortcut(QKeySequence::Copy);

  QAction* pasteAction = menu.addAction(QStringLiteral("粘贴"), editor_, &QsciScintilla::paste);
  pasteAction->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());
  pasteAction->setShortcut(QKeySequence::Paste);

  QAction* deleteAction = menu.addAction(QStringLiteral("删除"), editor_, &QsciScintilla::removeSelectedText);
  deleteAction->setEnabled(editor_->hasSelectedText());

  menu.addSeparator();

  // 全选
  QAction* selectAllAction = menu.addAction(QStringLiteral("全选"), editor_, &QsciScintilla::selectAll);
  selectAllAction->setShortcut(QKeySequence::SelectAll);

  // 显示菜单
  menu.exec(event->globalPos());
}

void EditorWidget::onConfigChanged(const QString& key) {
  using namespace etest::core::config;

  if (key == CONFIG_EDITOR_SHOW_LINE_NUMBER) {
    bool show = ConfigManager::instance().get<bool>(key, CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER);
    editor_->setMarginLineNumbers(0, show);
  } else if (key == CONFIG_EDITOR_AUTO_INDENT) {
    bool autoIndent = ConfigManager::instance().get<bool>(key, CONFIG_EDITOR_DEFAULT_AUTO_INDENT);
    editor_->setAutoIndent(autoIndent);
  } else if (key == CONFIG_EDITOR_TAB_WIDTH) {
    int tabWidth = ConfigManager::instance().get<int>(key, CONFIG_EDITOR_DEFAULT_TAB_WIDTH);
    editor_->setTabWidth(tabWidth);
  } else if (key == CONFIG_EDITOR_SPACES_FOR_TAB) {
    bool spacesForTab = ConfigManager::instance().get<bool>(key, CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB);
    editor_->setIndentationsUseTabs(!spacesForTab);
  } else if (key == CONFIG_EDITOR_FONT_SIZE) {
    int fontSize = ConfigManager::instance().get<int>(key, CONFIG_EDITOR_DEFAULT_FONT_SIZE);
    QFont font = editor_->font();
    font.setPointSize(fontSize);
    editor_->setFont(font);
    editor_->setMarginsFont(font);
  }
}

}  // namespace etest::app