#include "EditorWidget.h"

#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
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
  // 行号
  editor_->setMarginType(0, QsciScintilla::NumberMargin);
  editor_->setMarginWidth(0, "0000");
  editor_->setMarginLineNumbers(0, true);

  // 代码折叠
  editor_->setFolding(QsciScintilla::PlainFoldStyle);

  // 自动缩进
  editor_->setAutoIndent(true);
  editor_->setIndentationGuides(true);
  editor_->setTabWidth(4);
  editor_->setIndentationsUseTabs(false);

  // 换行模式
  editor_->setWrapMode(QsciScintilla::WrapNone);

  // 光标行高亮
  editor_->setCaretLineVisible(true);
  editor_->setCaretLineBackgroundColor(QColor(240, 240, 240));

  // UTF-8
  editor_->setUtf8(true);
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
  // VS风格配色
  QColor blue(0, 0, 255);
  QColor green(0, 128, 0);
  QColor darkRed(163, 21, 21);
  QColor teal(43, 145, 175);
  QColor gray(128, 128, 128);

  // 根据lexer类型使用各自的样式枚举
  if (auto* cpp = qobject_cast<QsciLexerCPP*>(lexer)) {
    cpp->setColor(blue, QsciLexerCPP::Keyword);
    cpp->setColor(green, QsciLexerCPP::Comment);
    cpp->setColor(green, QsciLexerCPP::CommentLine);
    cpp->setColor(darkRed, QsciLexerCPP::DoubleQuotedString);
    cpp->setColor(darkRed, QsciLexerCPP::SingleQuotedString);
    cpp->setColor(teal, QsciLexerCPP::Number);
    cpp->setColor(gray, QsciLexerCPP::Operator);
  } else if (auto* lua = qobject_cast<QsciLexerLua*>(lexer)) {
    lua->setColor(blue, QsciLexerLua::Keyword);
    lua->setColor(green, QsciLexerLua::Comment);
    lua->setColor(green, QsciLexerLua::LineComment);
    lua->setColor(darkRed, QsciLexerLua::String);
    lua->setColor(darkRed, QsciLexerLua::Character);
    lua->setColor(teal, QsciLexerLua::Number);
  } else if (auto* json = qobject_cast<QsciLexerJSON*>(lexer)) {
    json->setColor(blue, QsciLexerJSON::Keyword);
    json->setColor(green, QsciLexerJSON::CommentLine);
    json->setColor(green, QsciLexerJSON::CommentBlock);
    json->setColor(darkRed, QsciLexerJSON::String);
    json->setColor(teal, QsciLexerJSON::Number);
  } else if (auto* xml = qobject_cast<QsciLexerXML*>(lexer)) {
    xml->setColor(blue, QsciLexerXML::Tag);
    xml->setColor(green, QsciLexerXML::HTMLComment);
    xml->setColor(darkRed, QsciLexerXML::HTMLDoubleQuotedString);
    xml->setColor(darkRed, QsciLexerXML::HTMLSingleQuotedString);
    xml->setColor(teal, QsciLexerXML::HTMLNumber);
  } else if (auto* py = qobject_cast<QsciLexerPython*>(lexer)) {
    py->setColor(blue, QsciLexerPython::Keyword);
    py->setColor(green, QsciLexerPython::Comment);
    py->setColor(darkRed, QsciLexerPython::DoubleQuotedString);
    py->setColor(darkRed, QsciLexerPython::SingleQuotedString);
    py->setColor(teal, QsciLexerPython::Number);
  } else if (auto* yaml = qobject_cast<QsciLexerYAML*>(lexer)) {
    yaml->setColor(blue, QsciLexerYAML::Keyword);
    yaml->setColor(green, QsciLexerYAML::Comment);
    yaml->setColor(teal, QsciLexerYAML::Number);
  } else if (auto* cmake = qobject_cast<QsciLexerCMake*>(lexer)) {
    cmake->setColor(blue, QsciLexerCMake::Function);
    cmake->setColor(green, QsciLexerCMake::Comment);
    cmake->setColor(darkRed, QsciLexerCMake::String);
    cmake->setColor(teal, QsciLexerCMake::Number);
  }
  // QsciLexerJavaScript 继承自 QsciLexerCPP，qobject_cast<QsciLexerCPP*> 可匹配
  // QsciLexerMarkdown 无传统关键字/注释/字符串概念，不配色
}

}  // namespace etest::app
