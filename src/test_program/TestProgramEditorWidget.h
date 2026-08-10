#ifndef ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_
#define ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_

#include <QMainWindow>
#include <QVector>

#include "api/IEditor.h"
#include "api/IEditorCommands.h"
#include "TestProgramData.h"

#include "StepDetailPanel.h"
#include "StepTableWidget.h"

#include <QDockWidget>

class QAction;
class QDockWidget;
class QShowEvent;
class QLabel;
class QLineEdit;
class QListView;
class QStandardItemModel;
class QTabWidget;
class QTextEdit;
class QToolBar;

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace etest::app {

class ISignalSelection;

class VerticalTabListDelegate;

class TestProgramEditorWidget : public QMainWindow,
                                public IEditor,
                                public IEditorCommandSource {
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

  // IEditorCommandSource
  QList<EditorCommand> editorCommands() override;
  QObject* commandStateObject() override;

  // Undo/Redo
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;

  // 只读模式
  void setReadOnly(bool readOnly) override;

  void openFile(const QString& filePath) override;
  void setEmbeddedMode(bool embedded);
  void newProgram();

  // ── M0: ISignalSelection 注入（传播到所有 StepTableWidget） ──
  void setSignalSelection(ISignalSelection* sel);
  // M5: SignalRegistry 绑定
  void setRegistry(etest::core::SignalRegistry* reg);

  // 获取当前程序数据（供 MainWindow 运行引擎使用）
  TestProgramData programData() const { return const_cast<TestProgramEditorWidget*>(this)->uiToProgram(); }

 signals:
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);
  void programSaved(const QString& path);
  void undoStateChanged();
  void commandsChanged();

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private slots:
  void onDataChanged();
  void onAddCase();
  void onRemoveCase();
  void onAddStep();
  void onRemoveStep();
  void onMoveUp();
  void onMoveDown();
  void onStepSelectionChanged();
  void reloadToolbarIcons();

 private:
  void initUi();
  void initSignals();
  bool loadFile(const QString& path);
  bool saveFile(const QString& path);
  void loadProgramToUi(const TestProgramData& suite);
  void setModified(bool modified);
  TestProgramData uiToProgram();
  void saveSnapshot();
  void restoreState(const TestProgramData& state);
  void resetSnapshots(bool clean);
  void updateActions();

  // 纵向标签栏（Edge 风格垂直标签）
  void rebuildVerticalTabs();
  void refreshCurrentTabStepCount();
  void applyTabOrientation(bool vertical);
  void removeCaseAt(int index, bool confirm);
  void renameCase(int index);
  void reloadTabIcons();

  // 校验
  void validateCurrentTable();
  void updateValidationLabel();

  // ── M0: 信号选择器（传播到所有 table） ──
  ISignalSelection* signal_selection_ = nullptr;
  // M5: UUID → 名称 resolve
  etest::core::SignalRegistry* registry_ = nullptr;

  // 把 StepTableWidget 信号接到 TestProgramEditorWidget 的槽
  void connectTable(StepTableWidget* table);

  // 从表格行读取 TestStepData（cmd/desc/target/value/delayMs/timeoutMs + ext）
  TestStepData readStepData(StepTableWidget* table, int row) const;

  QLineEdit* suite_name_edit_ = nullptr;
  QTextEdit* suite_desc_edit_ = nullptr;
  QTabWidget* tab_widget_ = nullptr;
  QDockWidget* info_dock_ = nullptr;
  QDockWidget* detail_dock_ = nullptr;
  StepDetailPanel* step_detail_panel_ = nullptr;
  QLabel* validation_label_ = nullptr;

  // 纵向标签栏
  QDockWidget* vertical_tabs_dock_ = nullptr;
  QListView* vertical_tabs_view_ = nullptr;
  QStandardItemModel* vertical_tabs_model_ = nullptr;
  VerticalTabListDelegate* vertical_tabs_delegate_ = nullptr;
  QAction* toggle_orientation_action_ = nullptr;
  bool syncing_vertical_tabs_ = false;
  bool applying_orientation_ = false;

  QToolBar* toolbar_ = nullptr;
  QAction* add_case_action_ = nullptr;
  QAction* remove_case_action_ = nullptr;
  QAction* add_step_action_ = nullptr;
  QAction* remove_step_action_ = nullptr;
  QAction* move_up_action_ = nullptr;
  QAction* move_down_action_ = nullptr;
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;

  // 固定索引: 0=setup, 1=teardown, 2+=cases
  StepTableWidget* setup_table_ = nullptr;
  StepTableWidget* teardown_table_ = nullptr;

  QString current_file_;
  bool modified_ = false;
  bool loading_ = false;
  bool embedded_ = false;
  bool undo_redo_in_progress_ = false;
  bool validating_ = false;
  // RAII 守护：validateCurrentTable 在 setCellData 信号链递归时不会重入
  struct ValidateGuard {
    TestProgramEditorWidget* self;
    bool prev;
    explicit ValidateGuard(TestProgramEditorWidget* s)
        : self(s), prev(s->validating_) {
      if (prev) {
        self = nullptr;  // 标记重入
      } else {
        self->validating_ = true;
      }
    }
    ~ValidateGuard() {
      if (self) self->validating_ = false;
    }
    bool shouldRun() const { return self != nullptr; }
  };

  // 快照式撤销/重做
  QVector<TestProgramData> snapshots_;
  int snapshot_index_ = -1;
  int clean_snapshot_index_ = -1;
  static constexpr int kMaxSnapshots = 100;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_TESTPROGRAM_EDITOR_WIDGET_H_
