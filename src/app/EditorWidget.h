#ifndef ETEST_APP_EDITOR_WIDGET_H_
#define ETEST_APP_EDITOR_WIDGET_H_

#include <Qsci/qsciscintilla.h>
#include <QWidget>

class EditorWidget : public QWidget {
  Q_OBJECT

 public:
  explicit EditorWidget(const QString& filePath, QWidget* parent = nullptr);

  QString filePath() const;
  QString fileName() const;
  bool isModified() const;

  bool loadFile();
  bool saveFile();
  bool saveFileAs(const QString& newPath);

  QsciScintilla* editor() const;

 Q_SIGNALS:
  void modificationChanged(bool modified);

 private:
  void setupEditor();
  void applyLexer(const QString& suffix);
  void applyColorScheme(QsciLexer* lexer);

  QsciScintilla* editor_;
  QString file_path_;
};

#endif  // ETEST_APP_EDITOR_WIDGET_H_
