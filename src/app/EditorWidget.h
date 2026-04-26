#ifndef ETEST_APP_EDITOR_WIDGET_H_
#define ETEST_APP_EDITOR_WIDGET_H_

#include <Qsci/qsciscintilla.h>
#include <QWidget>
#include "config/ConfigManager.h"

namespace etest::app {

class EditorWidget : public QWidget {
  Q_OBJECT

 public:
  explicit EditorWidget(const QString& filePath, QWidget* parent = nullptr);

  QString filePath() const;
  QString fileName() const;
  bool isModified() const;

  void setFilePath(const QString& newPath);

  bool loadFile();
  bool saveFile();
  bool saveFileAs(const QString& newPath);

  QsciScintilla* editor() const;

 Q_SIGNALS:
  void modificationChanged(bool modified);
  void editorStateChanged(); // 编辑器状态变化（文本修改、撤销栈变化等）

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
};

}  // namespace etest::app

#endif  // ETEST_APP_EDITOR_WIDGET_H_
