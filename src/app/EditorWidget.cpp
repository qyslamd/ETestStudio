#include "EditorWidget.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVBoxLayout>

#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexeryaml.h>

EditorWidget::EditorWidget(const QString& filePath, QWidget* parent)
    : QWidget(parent), file_path_(filePath) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  editor_ = new QsciScintilla(this);
  layout->addWidget(editor_);

  setupEditor();

  QFileInfo fi(filePath);
  applyLexer(fi.suffix().toLower());

  loadFile();

  connect(editor_, &QsciScintilla::modificationChanged, this,
          [this](bool m) { emit modificationChanged(m); });
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
  if (file_path_.isEmpty()) return false;
  QFile file(file_path_);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream out(&file);
  out.setCodec("UTF-8");
  out << editor_->text();
  editor_->setModified(false);
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

void EditorWidget::applyColorScheme(QsciLexer* lexer) {
  // VS风格配色
  QColor blue(0, 0, 255);
  QColor green(0, 128, 0);
  QColor darkRed(163, 21, 21);
  QColor teal(43, 145, 175);
  QColor gray(128, 128, 128);

  // 通用样式：关键字、注释、字符串
  lexer->setColor(blue, QsciLexerCPP::Keyword);
  lexer->setColor(green, QsciLexerCPP::Comment);
  lexer->setColor(green, QsciLexerCPP::CommentLine);
  lexer->setColor(darkRed, QsciLexerCPP::DoubleQuotedString);
  lexer->setColor(darkRed, QsciLexerCPP::SingleQuotedString);
  lexer->setColor(teal, QsciLexerCPP::Number);
  lexer->setColor(gray, QsciLexerCPP::Operator);
}
