#pragma once

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QListView;
class QStandardItemModel;

namespace etest::runconfig {

// ProgramChecklistWidget — 测试程序多选清单
// 顶部"全选"QCheckBox（tristate：全勾/半勾/全不勾）+ QListView（checkable item），
// 列出项目 cases/*.etprog。勾选结果以相对项目根路径（cases/xxx.etprog）暴露给宿主。
// 监听绑拓扑连线，与测试程序正交；本组件只负责"选哪些测试程序跑"这一独立配置。
class ProgramChecklistWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ProgramChecklistWidget(QWidget* parent = nullptr);

  // 设置项目根（含 cases/ 的目录），空则清空列表；同根不重建
  void setProjectRoot(const QString& root);
  // 设置勾选（相对项目根路径），供加载/撤销恢复
  void setSelectedPrograms(const QStringList& paths);
  // 当前勾选（相对项目根路径），按列表顺序
  QStringList selectedPrograms() const;

 signals:
  // 勾选集合变化（用户勾选/全选），供宿主压快照 + 置脏
  void programsChanged();

 private:
  void refreshList();
  void updateSelectAllState();

  QCheckBox* select_all_ = nullptr;
  QListView* list_view_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QString project_root_;
  bool updating_ = false;  // 程序性刷新中，抑制 programsChanged
};

}  // namespace etest::runconfig
