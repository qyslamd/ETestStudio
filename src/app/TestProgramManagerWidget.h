#ifndef ETEST_APP_TESTPROGRAM_MANAGER_WIDGET_H_
#define ETEST_APP_TESTPROGRAM_MANAGER_WIDGET_H_

#include <QPushButton>
#include <QStringList>
#include <QTreeWidget>
#include <QWidget>

#include "test_program/TestProgramData.h"

namespace etest::app {

class TestProgramManagerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit TestProgramManagerWidget(QWidget* parent = nullptr);

  /// 返回当前选中项的完整路径，无选中返回空
  QString selectedProgramPath() const;
  /// 树中是否有测试程序文件（至少有一个 .etprog 条目）
  bool hasAnyProgram() const;

 public slots:
  void refreshList();

 signals:
  void openFileRequested(const QString& filePath);
  /// 选中项变化时发出，filePath 为空表示取消选中
  void programSelectionChanged(const QString& filePath);

 private:
  void initUi();
  void initSignals();

  void onItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onCustomContextMenu(const QPoint& pos);
  void onNewTestProgram();
  bool renameTestProgramFile(const QString& oldPath);
  bool removeTestProgramFile(const QString& filePath);

  QTreeWidget* tree_ = nullptr;
  QPushButton* new_btn_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_TESTPROGRAM_MANAGER_WIDGET_H_
