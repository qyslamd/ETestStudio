#include "EditorPanelController.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>

#include "EditorManager.h"
#include "api/IEditor.h"
#include "editors/TextEditorWidget.h"
#include "logger/Logger.h"

namespace etest::app {

EditorPanelController::EditorPanelController(EditorManager* editor_mgr,
                                             QObject* parent)
    : QObject(parent), editor_mgr_(editor_mgr) {}

void EditorPanelController::saveCurrent() {
  LOG_INFO("MAIN_UI", "点击「保存」");
  auto* editor = editor_mgr_->currentEditor();
  if (!editor)
    return;
  if (!editor->save()) {
    QMessageBox::warning(
        nullptr, QStringLiteral("保存失败"),
        QStringLiteral("无法保存文件：%1").arg(editor->filePath()));
  }
}

void EditorPanelController::saveCurrentAs() {
  LOG_INFO("MAIN_UI", "点击「另存为」");
  auto* editor = editor_mgr_->currentEditor();
  if (!editor)
    return;

  QString newPath = QFileDialog::getSaveFileName(
      nullptr, QStringLiteral("另存为"), editor->filePath(),
      QStringLiteral("所有文件 (*)"));
  if (!newPath.isEmpty()) {
    if (!editor->saveAs(newPath)) {
      QMessageBox::warning(nullptr, QStringLiteral("保存失败"),
                           QStringLiteral("无法保存文件：%1").arg(newPath));
    } else {
      editor_mgr_->updateEditorId(editor, newPath);
    }
  }
}

void EditorPanelController::saveAll() {
  LOG_INFO("MAIN_UI", "点击「保存所有」");
  editor_mgr_->saveAllFiles();
}

void EditorPanelController::closeCurrent() {
  LOG_INFO("MAIN_UI", "点击「关闭文件」");
  auto* editor = editor_mgr_->currentEditor();
  if (!editor)
    return;
  editor_mgr_->closeFile(editor->editorId());
}

void EditorPanelController::closeAll() {
  LOG_INFO("MAIN_UI", "点击「关闭所有文件」");
  editor_mgr_->closeAllFiles();
}

void EditorPanelController::undo() {
  LOG_INFO("MAIN_UI", "点击「撤销」");
  if (auto* editor = editor_mgr_->currentEditor()) {
    editor->undo();
  }
}

void EditorPanelController::redo() {
  LOG_INFO("MAIN_UI", "点击「重做」");
  if (auto* editor = editor_mgr_->currentEditor()) {
    editor->redo();
  }
}

void EditorPanelController::cut() {
  LOG_INFO("MAIN_UI", "点击「剪切」");
  if (auto* editor = editor_mgr_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->cut();
    }
  }
}

void EditorPanelController::copy() {
  LOG_INFO("MAIN_UI", "点击「复制」");
  if (auto* editor = editor_mgr_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->copy();
    }
  }
}

void EditorPanelController::paste() {
  LOG_INFO("MAIN_UI", "点击「粘贴」");
  if (auto* editor = editor_mgr_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->paste();
    }
  }
}

void EditorPanelController::find() {
  LOG_INFO("MAIN_UI", "点击「查找」");
  auto* editor = editor_mgr_->currentEditor();
  auto* textEditor = dynamic_cast<TextEditorWidget*>(editor);
  if (!textEditor)
    return;

  bool ok;
  QString searchText = QInputDialog::getText(
      nullptr, QStringLiteral("查找"), QStringLiteral("查找内容:"),
      QLineEdit::Normal, QString(), &ok);
  if (ok && !searchText.isEmpty()) {
    int line, column;
    textEditor->editor()->getCursorPosition(&line, &column);

    bool found = textEditor->editor()->findFirst(
        searchText, false, false, false, true, true, line, column, true);
    if (!found) {
      QMessageBox::information(nullptr, QStringLiteral("查找"),
                               QStringLiteral("找不到指定内容"));
    }
  }
}

void EditorPanelController::replace() {
  auto* editor = editor_mgr_->currentEditor();
  auto* textEditor = dynamic_cast<TextEditorWidget*>(editor);
  if (!textEditor)
    return;

  bool ok;
  QString searchText = QInputDialog::getText(
      nullptr, QStringLiteral("替换"), QStringLiteral("查找内容:"),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || searchText.isEmpty())
    return;

  QString replaceText = QInputDialog::getText(
      nullptr, QStringLiteral("替换"), QStringLiteral("替换为:"),
      QLineEdit::Normal, QString(), &ok);
  if (!ok)
    return;

  int line, column;
  textEditor->editor()->getCursorPosition(&line, &column);

  bool found = textEditor->editor()->findFirst(searchText, false, false, false,
                                               true, true, line, column, true);
  if (found) {
    while (textEditor->editor()->findNext()) {
      bool replaceOk;
      QString prompt = QStringLiteral("将＂%1＂替换为＂%2＂？")
                           .arg(searchText)
                           .arg(replaceText);
      QMessageBox msgBox(nullptr);
      msgBox.setWindowTitle(QStringLiteral("替换"));
      msgBox.setText(prompt);
      auto* yesAllBtn =
          msgBox.addButton(QStringLiteral("全部替换"), QMessageBox::YesRole);
      msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No |
                                QMessageBox::Cancel);
      msgBox.setDefaultButton(QMessageBox::Yes);
      replaceOk = (msgBox.exec() != QMessageBox::Cancel);

      if (!replaceOk)
        break;
      if (msgBox.clickedButton() == static_cast<QAbstractButton*>(yesAllBtn)) {
        textEditor->editor()->replace(replaceText);
        while (textEditor->editor()->findNext()) {
          textEditor->editor()->replaceSelectedText(replaceText);
        }
        break;
      } else {
        textEditor->editor()->replace(replaceText);
      }
    }

    QMessageBox::information(nullptr, QStringLiteral("替换"),
                             QStringLiteral("替换完成"));
  } else {
    QMessageBox::information(nullptr, QStringLiteral("替换"),
                             QStringLiteral("找不到指定内容"));
  }
}

void EditorPanelController::goToLine() {
  LOG_INFO("MAIN_UI", "点击「转到行」");
  auto* editor = editor_mgr_->currentEditor();
  auto* textEditor = dynamic_cast<TextEditorWidget*>(editor);
  if (!textEditor)
    return;

  int lineCount = textEditor->editor()->lines();
  bool ok;

  int currentLine, currentColumn;
  textEditor->editor()->getCursorPosition(&currentLine, &currentColumn);

  int lineNumber =
      QInputDialog::getInt(nullptr, QStringLiteral("跳转到行"),
                           QStringLiteral("行号 (1-%1):").arg(lineCount),
                           currentLine + 1, 1, lineCount, 1, &ok);
  if (ok) {
    textEditor->editor()->setCursorPosition(lineNumber - 1, 0);
    textEditor->editor()->ensureLineVisible(lineNumber - 1);
  }
}

}  // namespace etest::app
