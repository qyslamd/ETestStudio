#pragma once

#include <QPair>
#include <QSet>
#include <QWidget>

#include "RunConfig.h"

class QComboBox;
class QLineEdit;
class QListView;
class QPushButton;
class QStandardItemModel;

namespace etest::runconfig {

// MonitorPropertyWidget — 监听器属性面板（dock 内嵌，QScrollArea 包裹）
// 选中场景卡片后加载其属性：名称、类型（displayMode）切换、绑定连线
// （搜索 + 连接列表，已绑定连接禁用）、删除。纯展示+发信号，落盘由宿主。
class MonitorPropertyWidget : public QWidget {
  Q_OBJECT

 public:
  explicit MonitorPropertyWidget(QWidget* parent = nullptr);

  // 加载选中卡片；空 monitor.id 表示未选中（清空并禁用控件）
  void setMonitor(const RunConfig::Monitor& monitor,
                  const QList<QPair<QString, QString>>& connections,
                  const QSet<QString>& boundConnectionIds);

  void clear();

 signals:
  void nameChanged(const QString& id, const QString& name);
  void connectionBound(const QString& id, const QString& connectionId);
  void typeChanged(const QString& id, const QString& displayMode);
  void deleteRequested(const QString& id);

 private:
  void rebuildConnectionList(const QList<QPair<QString, QString>>& connections,
                             const QString& currentConnectionId,
                             const QSet<QString>& boundConnectionIds);

  QLineEdit* name_edit_ = nullptr;
  QComboBox* type_combo_ = nullptr;
  QLineEdit* search_box_ = nullptr;
  QListView* conn_list_ = nullptr;
  QStandardItemModel* conn_model_ = nullptr;
  QPushButton* delete_btn_ = nullptr;

  QString monitor_id_;        // 当前加载的卡片 id（空 = 未选中）
  QString current_connection_;  // 当前绑定的连接（列表高亮 + 不标禁用）
  bool loading_ = false;      // setMonitor 加载中抑制信号
  QList<QPair<QString, QString>> conns_;  // 连接列表缓存（搜索重建用）
  QSet<QString> bound_;                   // 已绑定连接集合缓存
};

}  // namespace etest::runconfig
