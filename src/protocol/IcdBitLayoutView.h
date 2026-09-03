#pragma once

#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <QColor>
#include <QFont>
#include <QPair>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>

#include <icd/frame.hpp>

class QComboBox;

namespace etest::protocol {

class ChildFieldItem;
class ContainerFieldItem;

// ============================================================
// 字段语义辅助函数
// ============================================================
// 构建节点 tooltip，包含描述、bit 范围、单位、缩放、枚举、LinkTo 等
QString buildNodeTooltip(const icd::Node& node);
// 构建节点 badge 列表，例如 "帧头"/"校验"/"信号值"/"大端"/"缩放"/"枚举"
QStringList buildNodeBadges(const icd::Node& node);
// 判断节点是否为大端字段（Tag::big_endian_value）
bool isNodeBigEndian(const icd::Node& node);

// ============================================================
// LayoutNodeItem — 位布局项基类
// ============================================================
class LayoutNodeItem : public QGraphicsObject {
  Q_OBJECT
 public:
  explicit LayoutNodeItem(QGraphicsItem* parent = nullptr);

  virtual const icd::Node* node() const = 0;
  virtual void setHighlightedNode(const icd::Node* node) = 0;
  virtual void setHoveredNode(const icd::Node* node, bool on) = 0;
  virtual int totalHeight() const = 0;
  virtual int sectionWidth() const = 0;

 signals:
  void clicked(const icd::Node* node);
  void contextMenuAction(const icd::Node* node, const QString& action);
  void hovered(const icd::Node* node, bool on);
};

// ============================================================
// FieldSectionItem — 每个普通根字段对应的竖排区块
// ============================================================
class FieldSectionItem : public LayoutNodeItem {
  Q_OBJECT
 public:
  FieldSectionItem(const icd::Node* node, const QString& value_type,
                   const QColor& color, int cell_size, int bits_per_row = 8,
                   QGraphicsItem* parent = nullptr);

  // 兼容测试和临时手工添加区块的构造接口。
  FieldSectionItem(const QString& name, const QString& value_type,
                   int byte_offset, int start_bit, int bit_width,
                   const QColor& color, int cell_size, int bits_per_row = 8,
                   QGraphicsItem* parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  const icd::Node* node() const override { return node_; }
  QString name() const { return name_; }
  int cellSize() const { return cell_size_; }
  void setHighlighted(bool on);
  void setHovered(bool on);
  void setHighlightedNode(const icd::Node* node) override;
  void setHoveredNode(const icd::Node* node, bool on) override;

  int totalHeight() const override;
  int sectionWidth() const override;

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

 private:
  const icd::Node* node_ = nullptr;
  QString name_;
  QString value_type_;
  int byte_offset_;
  int start_bit_;
  int bit_width_;
  QColor color_;
  int cell_size_;
  int bits_per_row_;
  bool hovered_ = false;
  bool highlighted_ = false;
  static constexpr int kHeaderHeight = 28;
  static constexpr int kMinSectionWidth = 420;
  QFont header_font_;
  QFont cell_font_;
};

// ============================================================
// ChildFieldItem — 容器字段内部的子字段行（树形缩进）
// ============================================================
class ChildFieldItem : public QGraphicsObject {
  Q_OBJECT
 public:
  ChildFieldItem(const icd::Node* node, int relative_start, int relative_end,
                 const QString& value_type, const QColor& color, int cell_size,
                 QGraphicsItem* parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  const icd::Node* node() const { return node_; }
  int relativeStart() const { return relative_start_; }
  int relativeEnd() const { return relative_end_; }
  void setHighlighted(bool on);
  void setHovered(bool on);
  void setRowWidth(int w) { row_width_ = w; }

  static constexpr int kRowHeight = 26;

 signals:
  void clicked(const icd::Node* node);
  void contextMenuAction(const icd::Node* node, const QString& action);
  void hovered(const icd::Node* node, bool on);

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

 private:
  const icd::Node* node_ = nullptr;
  int relative_start_ = 0;
  int relative_end_ = 0;
  QString value_type_;
  QColor color_;
  int cell_size_ = 38;
  int row_width_ = 200;
  bool hovered_ = false;
  bool highlighted_ = false;
  bool is_last_ = false;
  QFont row_font_;

  friend class ContainerFieldItem;  // allow container to set is_last_
};

// ============================================================
// ContainerFieldItem — 有 children 的根字段容器
// ============================================================
class ContainerFieldItem : public LayoutNodeItem {
  Q_OBJECT
 public:
  ContainerFieldItem(const icd::Node* node, const QString& value_type,
                     const QColor& color, int cell_size,
                     QGraphicsItem* parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  const icd::Node* node() const override { return node_; }
  void setHighlightedNode(const icd::Node* node) override;
  void setHoveredNode(const icd::Node* node, bool on) override;
  int totalHeight() const override;
  int sectionWidth() const override;

  int childFieldCount() const { return child_items_.size(); }
  bool containsChildNode(const icd::Node* node) const;
  QPair<int, int> childRelativeRange(const icd::Node* node) const;
  ChildFieldItem* childFieldItem(const icd::Node* node) const;

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

 private:
  void initChildren();
  int parentStartBit() const;

  const icd::Node* node_ = nullptr;
  QString name_;
  QString value_type_;
  QColor color_;
  int cell_size_ = 38;
  bool hovered_ = false;
  bool highlighted_ = false;
  static constexpr int kHeaderHeight = 30;
  static constexpr int kRowHeight = 26;
  static constexpr int kChildIndent = 24;
  static constexpr int kMinSectionWidth = 520;
  static constexpr int kContainerPadding = 12;
  QFont header_font_;
  QFont row_font_;
  QVector<ChildFieldItem*> child_items_;
};

// ============================================================
// IcdBitLayoutScene — 管理所有 LayoutNodeItem，纵向排列
// ============================================================
class IcdBitLayoutScene : public QGraphicsScene {
  Q_OBJECT
 public:
  explicit IcdBitLayoutScene(QObject* parent = nullptr);

  FieldSectionItem* addBlock(const QString& name, const QString& value_type,
                             int byte_offset, int start_bit, int bit_width,
                             const QColor& color);
  FieldSectionItem* addField(const icd::Node* node, const QString& value_type,
                             const QColor& color);
  ContainerFieldItem* addContainer(const icd::Node* node,
                                   const QString& value_type,
                                   const QColor& color);
  void clearBlocks();
  void highlightNode(const icd::Node* node);

  int cellSize() const { return cell_size_; }
  int topLevelLayoutItemCount() const { return items_.size(); }
  int fieldItemCount() const;
  int containerItemCount() const;
  QVector<ContainerFieldItem*> containerItems() const;

 public slots:
  void onNodeHovered(const icd::Node* node, bool on);

 signals:
  void nodeClicked(const icd::Node* node);
  void nodeContextMenuAction(const icd::Node* node, const QString& action);
  void nodeHovered(const icd::Node* node, bool on);

 private:
  void appendLayoutItem(LayoutNodeItem* item);

  int cell_size_ = 38;
  int left_margin_ = 16;
  int section_spacing_ = 12;
  int next_y_ = 16;
  const icd::Node* selected_node_ = nullptr;
  QVector<LayoutNodeItem*> items_;
};

// ============================================================
// IcdBitLayoutView — 直接派生 QGraphicsView，见名知意
// ============================================================
class IcdBitLayoutView : public QGraphicsView {
  Q_OBJECT
 public:
  explicit IcdBitLayoutView(QWidget* parent = nullptr);

  void loadFromFrame(const icd::Frame& frame);
  FieldSectionItem* addBlock(const QString& name, const QString& value_type,
                             int byte_offset, int start_bit, int bit_width,
                             const QColor& color);
  void clearBlocks();
  void highlightNode(const icd::Node* node);

 signals:
  void nodeClicked(const icd::Node* node);
  void nodeContextMenuAction(const icd::Node* node, const QString& action);
  void nodeHovered(const icd::Node* node, bool on);

 private:
  void initUi();

  IcdBitLayoutScene* scene_ = nullptr;
  const icd::Frame* last_frame_ = nullptr;
};

}  // namespace etest::protocol
