#pragma once

#include <QStyledItemDelegate>

namespace etest::app {

/// HintPopup QListView 的自定义 delegate。
/// 每行绘制：左侧色块 + 消息文本 + 可选操作按钮 + 关闭按钮。
class HintMessageDelegate : public QStyledItemDelegate {
  Q_OBJECT
 public:
  explicit HintMessageDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

  enum class ClickRegion { None, Action, Close };

  /// 判断坐标落在哪个可点击区域
  static ClickRegion hitTest(const QRect& itemRect, const QPoint& pos,
                              bool hasAction);

  /// 设置当前 hover 的行和区域（供 HintPopup 在 mouseMove 中调用）
  void setHoveredRegion(int row, ClickRegion region);

  static constexpr int kItemHeight = 36;
  static constexpr int kIndicatorWidth = 4;
  static constexpr int kActionBtnWidth = 48;
  static constexpr int kCloseBtnWidth = 20;
  static constexpr int kMargin = 8;

 signals:
  void actionTriggered(int row);
  void closeRequested(int row);

 private:
  static QRect actionRect(const QRect& itemRect);
  static QRect closeRect(const QRect& itemRect);

  int hovered_row_ = -1;
  ClickRegion hovered_region_ = ClickRegion::None;
};

}  // namespace etest::app
