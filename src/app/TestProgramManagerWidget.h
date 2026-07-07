#ifndef ETEST_APP_TESTPROGRAM_MANAGER_WIDGET_H_
#define ETEST_APP_TESTPROGRAM_MANAGER_WIDGET_H_

#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

namespace etest::app {

class TestProgramManagerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit TestProgramManagerWidget(QWidget* parent = nullptr);

 public slots:
  void refreshList();

 signals:
  void openFileRequested(const QString& filePath);

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
