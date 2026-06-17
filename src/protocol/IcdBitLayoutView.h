#pragma once

#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QWidget>

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QString>

#include <icd/frame.hpp>

class QComboBox;

namespace etest::protocol {

// ============================================================
// FieldSectionItem — 每个信号对应的竖排区块
// ============================================================
class FieldSectionItem : public QGraphicsObject {
  Q_OBJECT
 public:
  FieldSectionItem(const QString& name, const QString& value_type,
                   int byte_offset, int start_bit,
                   int bit_width, const QColor& color, int cell_size,
                   int bits_per_row = 8,
                   QGraphicsItem* parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  QString name() const { return name_; }
  int cellSize() const { return cell_size_; }
  void setHighlighted(bool on);
  void setHovered(bool on);

  int totalHeight() const;
  int sectionWidth() const;

 signals:
  void clicked(const QString& name);
  void contextMenuAction(const QString& name, const QString& action);
  void hovered(const QString& name, bool on);

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

 private:
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
  QFont header_font_;
  QFont cell_font_;
};

// ============================================================
// IcdBitLayoutScene — 管理所有 FieldSectionItem，纵向排列
// ============================================================
 class IcdBitLayoutScene : public QGraphicsScene {
  Q_OBJECT
 public:
  explicit IcdBitLayoutScene(QObject* parent = nullptr);

  FieldSectionItem* addBlock(const QString& name, const QString& value_type,
                             int byte_offset, int start_bit, int bit_width,
                             const QColor& color);
  void clearBlocks();
  void highlightBlock(const QString& name);

  int cellSize() const { return cell_size_; }

 public slots:
  void onBlockHovered(const QString& name, bool on);

 signals:
  void blockClicked(const QString& name);
  void contextMenuAction(const QString& name, const QString& action);
  void blockHovered(const QString& name, bool on);

 private:
  int cell_size_ = 38;
  int left_margin_ = 16;
  int section_spacing_ = 12;
  int next_y_ = 16;
  QString selected_name_;
  QVector<FieldSectionItem*> blocks_;
};

// ============================================================
// IcdBitLayoutView — 外层 QWidget + toolbar + QGraphicsView
// ============================================================
class IcdBitLayoutView : public QWidget {
  Q_OBJECT
 public:
  explicit IcdBitLayoutView(QWidget* parent = nullptr);

  void loadFromFrame(const icd::Frame& frame);
  FieldSectionItem* addBlock(const QString& name, const QString& value_type,
                             int byte_offset, int start_bit, int bit_width,
                             const QColor& color);
  void clearBlocks();
  void highlightBlock(const QString& name);

 signals:
  void blockClicked(const QString& name);
  void contextMenuAction(const QString& name, const QString& action);
  void blockHovered(const QString& name, bool on);

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void initUi();
  void collectAllNodes(const icd::Node& node, QVector<const icd::Node*>& out);

  IcdBitLayoutScene* scene_ = nullptr;
  QGraphicsView* view_ = nullptr;
  const icd::Frame* last_frame_ = nullptr;
};

}  // namespace etest::protocol
