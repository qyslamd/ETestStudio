#ifndef ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_
#define ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_

#include <QVector>
#include <QWidget>

#include "api/IEditor.h"
#include "TestProgramData.h"

class QLineEdit;
class QTableWidget;
class QTabWidget;
class QTextEdit;
class QToolButton;

namespace etest::app {

class TestProgramEditorWidget : public QWidget, public IEditor {
  Q_OBJECT

 public:
  explicit TestProgramEditorWidget(const QString& id, QWidget* parent = nullptr);

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

  void setEditorId(const QString& id);

 signals:
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);

 private slots:
  void onDataChanged();
  void onAddCase();
  void onRemoveCase();
  void onAddStep();
  void onRemoveStep();

 private:
  void initUi();
  void initSignals();
  bool loadFile(const QString& path);
  bool saveFile(const QString& path);
  void loadProgramToUi(const TestProgramData& suite);
  TestProgramData uiToProgram() const;
  QTableWidget* createStepTable();
  void connectTableSignals(QTableWidget* table);
  void setModified(bool modified);
  void saveSnapshot();
  void restoreState(const TestProgramData& state);

  QLineEdit* suite_name_edit_ = nullptr;
  QTextEdit* suite_desc_edit_ = nullptr;
  QTabWidget* tab_widget_ = nullptr;
  QToolButton* add_case_btn_ = nullptr;
  QToolButton* remove_case_btn_ = nullptr;
  QToolButton* add_step_btn_ = nullptr;
  QToolButton* remove_step_btn_ = nullptr;

  // 固定索引: 0=setup, 1=teardown, 2+=cases
  QTableWidget* setup_table_ = nullptr;
  QTableWidget* teardown_table_ = nullptr;

  QString current_file_;
  bool modified_ = false;
  bool loading_ = false;
  bool undo_redo_in_progress_ = false;

  // 快照式撤销/重做
  QVector<TestProgramData> snapshots_;
  int snapshot_index_ = -1;
  int clean_snapshot_index_ = -1;  // 与保存文件一致时的快照索引
  static constexpr int kMaxSnapshots = 100;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_
