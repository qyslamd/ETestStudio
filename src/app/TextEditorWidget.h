#ifndef ETEST_APP_TEXT_EDITOR_WIDGET_H_
#define ETEST_APP_TEXT_EDITOR_WIDGET_H_

#include <Qsci/qsciscintilla.h>
#include <QWidget>
#include "config/ConfigManager.h"
#include "api/IEditor.h"

namespace etest::app {

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
};

}  // namespace etest::app

#endif  // ETEST_APP_TEXT_EDITOR_WIDGET_H_
