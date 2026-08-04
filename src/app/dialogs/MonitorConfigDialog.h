#pragma once

#include <QDialog>
#include <QGroupBox>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>

#include "engine/MonitorManager.h"

class QGridLayout;
class QLineEdit;
class QListView;
class QModelIndex;
class QMouseEvent;
class QPushButton;
class QStandardItem;
class QStandardItemModel;

namespace etest::app {

class SignalVisualizer;

// ══════════════════════════════════════════════════════════════════════════════
// MonitorTypeTile — 右栏可点击的 visualizer 类型瓦片
// ══════════════════════════════════════════════════════════════════════════════
// 用 QGroupBox 做结构（标题 = 类型名）包一个真实 visualizer 空态实例；点击事件
// 在瓦片本身上处理，避免 QCustomPlot 等子控件吞掉鼠标事件（瓦片内 visualizer
// 全部对鼠标透明）。不用 checkable（未勾选会禁用子控件，预览会变灰）；
// 选中态走 QSS property selected=true 描边。
// ══════════════════════════════════════════════════════════════════════════════
class MonitorTypeTile : public QGroupBox {
  Q_OBJECT

 public:
  explicit MonitorTypeTile(const QString& displayMode, const QString& title,
                           QWidget* parent = nullptr);

  QString displayMode() const { return display_mode_; }
  void setSelectedHighlight(bool selected);

 signals:
  void clicked(const QString& displayMode);

 protected:
  void mousePressEvent(QMouseEvent* event) override;

 private:
  QString display_mode_;
};

// ══════════════════════════════════════════════════════════════════════════════
// MonitorConfigDialog — 监听器配置双栏对话框（决策 15/16/18）
// ══════════════════════════════════════════════════════════════════════════════
// 左栏：搜索框 + 全部拓扑连接列表（QStandardItemModel，行 data(UserRole)=connectionId）
//   - checkbox 仅对已配置连接显示（未配置无 checkbox，配置后出现并默认勾选）
//   - 已配置行绿色 + 粗体；底部固定「失效监听器」分组灰显可选中
// 右栏：QScrollArea + QGridLayout 平铺 5 种 visualizer 真实空态实例（创建一次复用），
//   点类型 = 配置/切换（创建即所见），再点已选类型 = 取消配置（删除监听器）
// 非模态（决策 18）：由 controller 用 show() 打开；编排逻辑全部留在 controller。
// ══════════════════════════════════════════════════════════════════════════════
class MonitorConfigDialog : public QDialog {
  Q_OBJECT

 public:
  explicit MonitorConfigDialog(QWidget* parent = nullptr);

  // connectionId → 连接描述（device.port ↔ UUT.port）
  void setConnections(const QList<QPair<QString, QString>>& connections);
  // 含失效监听器（MonitorTreeEntry.invalid=true）
  void setMonitors(
      const QList<etest::engine::MonitorManager::MonitorTreeEntry>& monitors);
  // 当前执行页活跃通道（勾选态，会话内有效，不落盘）
  void setChecked(const QList<QString>& checkedIds);

 signals:
  void channelSelected(const QString& connectionId);
  void visualizerChosen(const QString& connectionId, const QString& displayMode);
  void checkToggled(const QString& connectionId, bool checked);
  void renameRequested(const QString& connectionId, const QString& name);
  void deleteRequested(const QString& connectionId);

 private:
  // 模型行种类
  enum RowKind { kRowSeparator = 0, kRowConnection = 1, kRowInvalid = 2 };

  void initUi();
  void buildRightPanel();
  void rebuildModel();
  void updateCheckStates();
  void updateTileHighlights();
  void updateTilesEnabled();
  void addSeparatorRow(const QString& text);
  SignalVisualizer* createPreviewVisualizer(const QString& displayMode);
  void makeTransparentToMouse(QWidget* widget);

  void onFilterChanged(const QString& text);
  void onRowSelected(const QModelIndex& index);
  void onItemCheckToggled(QStandardItem* item);
  void onTileClicked(const QString& displayMode);
  void onRenameCurrent();

  const etest::engine::MonitorManager::MonitorTreeEntry* monitorOf(
      const QString& connectionId) const;

  QLineEdit* search_box_ = nullptr;
  QListView* list_view_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QGridLayout* tiles_grid_ = nullptr;
  QPushButton* delete_button_ = nullptr;

  // displayMode -> tile（右栏）
  QHash<QString, MonitorTypeTile*> tiles_;

  // 数据
  QList<QPair<QString, QString>> conn_map_;  // connectionId -> 描述
  QHash<QString, etest::engine::MonitorManager::MonitorTreeEntry> monitor_map_;
  QSet<QString> checked_ids_;
  QString selected_connection_id_;
};

}  // namespace etest::app
