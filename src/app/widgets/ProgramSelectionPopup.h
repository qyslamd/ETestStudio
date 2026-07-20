#ifndef ETEST_APP_PROGRAM_SELECTION_POPUP_H_
#define ETEST_APP_PROGRAM_SELECTION_POPUP_H_

#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QToolButton>
#include <QWidget>

namespace etest::app {

/// 程序选择 popup（ribbon 执行 category 用）
/// QToolButton + popup QMenu（内含 checkbox 多选列表），
/// 由 ExecutionPanelController 构造并持有，MainWindow 通过 getter 放入 ribbon。
class ProgramSelectionPopup : public QWidget {
  Q_OBJECT

 public:
  explicit ProgramSelectionPopup(QWidget* parent = nullptr);

  /// 当前选中集合
  QStringList selectedPaths() const;

  /// 全集非空（供验证/全部运行 enable）
  bool hasAnyProgram() const;

  /// 已选数目（摘要用）
  int selectedCount() const;

  /// 全量程序路径列表（与选中状态无关，供全部运行使用）
  QStringList allPaths() const { return all_paths_; }

  /// 重扫 cases/*.etprog
  void refreshList();

 signals:
  /// 勾选变化时触发，驱动 ExecutionPanelController::updateRunControls
  void selectionChanged();

 private slots:
  void onItemChanged(QListWidgetItem* item);

 private:
  QToolButton* button_ = nullptr;
  QMenu* menu_ = nullptr;
  QListWidget* list_widget_ = nullptr;

  /// 全量程序路径列表（按 refreshList 时的扫描结果）
  QStringList all_paths_;
  /// 当前选中的路径集合
  QSet<QString> selected_;

  void scanPrograms();
  void updateButtonText();
};

}  // namespace etest::app

#endif  // ETEST_APP_PROGRAM_SELECTION_POPUP_H_
