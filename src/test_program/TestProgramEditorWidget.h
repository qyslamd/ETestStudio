#ifndef ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_
#define ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_

#include <QMainWindow>
#include <QVector>

#include "api/IEditor.h"
#include "TestProgramData.h"

#include "CommandTypeDelegate.h"
#include "StepDetailPanel.h"

class QAction;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class QTabWidget;
class QTextEdit;
class QSplitter;

namespace etest::app {

class TestProgramEditorWidget : public QMainWindow, public IEditor {
  Q_OBJECT

 public:
  explicit TestProgramEditorWidget(const QString& filePath,
                                   QWidget* parent = nullptr);

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

  void openFile(const QString& filePath) override;
  void setEmbeddedMode(bool embedded);
  void newProgram();

 signals:
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

 private slots:
  void onDataChanged();
  void onAddCase();
  void onRemoveCase();
  void onAddStep();
  void onRemoveStep();
  void onMoveUp();
  void onMoveDown();
  void onStepSelectionChanged();

 private:
  void initUi();
  void initSignals();
  bool loadFile(const QString& path);
  bool saveFile(const QString& path);
  void loadProgramToUi(const TestProgramData& suite);
  TestProgramData uiToProgram();
  QTableWidget* createStepTable(CommandTypeDelegate::Mode delegateMode);
  void connectTableSignals(QTableWidget* table);
  void setModified(bool modified);
  void saveSnapshot();
  void restoreState(const TestProgramData& state);
  void resetSnapshots(bool clean);
  void updateActions();
  void renumberSteps(QTableWidget* table);

  // 动态列头
  void updateColumnHeadersForCommand(QTableWidget* table, int row);

  // 校验
  void validateCurrentStep();
  void updateValidationLabel();

  // 扩展数据存取
  void storeStepExtData(QTableWidgetItem* item, const TestStepData& step);
  TestStepData loadStepExtData(const QTableWidgetItem* item) const;
  static constexpr int kStepDataRole = Qt::UserRole + 1;

  QLineEdit* suite_name_edit_ = nullptr;
  QTextEdit* suite_desc_edit_ = nullptr;
  QTabWidget* tab_widget_ = nullptr;
  QSplitter* splitter_ = nullptr;
  StepDetailPanel* step_detail_panel_ = nullptr;
  QLabel* validation_label_ = nullptr;

  QAction* add_case_action_ = nullptr;
  QAction* remove_case_action_ = nullptr;
  QAction* add_step_action_ = nullptr;
  QAction* remove_step_action_ = nullptr;
  QAction* move_up_action_ = nullptr;
  QAction* move_down_action_ = nullptr;
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;

  // 固定索引: 0=setup, 1=teardown, 2+=cases
  QTableWidget* setup_table_ = nullptr;
  QTableWidget* teardown_table_ = nullptr;

  QString current_file_;
  bool modified_ = false;
  bool loading_ = false;
  bool embedded_ = false;
  bool undo_redo_in_progress_ = false;

  // 快照式撤销/重做
  QVector<TestProgramData> snapshots_;
  int snapshot_index_ = -1;
  int clean_snapshot_index_ = -1;
  static constexpr int kMaxSnapshots = 100;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_
