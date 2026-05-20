#pragma once

#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QWidget>

#include <QColor>
#include <QString>

class QComboBox;

namespace etest::protocal {

// ============================================================
// BitBlockItem — 每个信号对应的色块
// ============================================================
class BitBlockItem : public QGraphicsObject {
  Q_OBJECT
 public:
  BitBlockItem(const QString& name, int byte_offset, int start_bit,
               int bit_width, const QColor& color, int cell_size,
               QGraphicsItem* parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  QString name() const { return name_; }
  int cellSize() const { return cell_size_; }
  void setHighlighted(bool on);

 signals:
  void clicked(const QString& name);

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

 private:
  QString name_;
  int byte_offset_;
  int start_bit_;
  int bit_width_;
  QColor color_;
  int cell_size_;
  bool hovered_ = false;
  bool highlighted_ = false;
};

// ============================================================
// IcdBitLayoutScene — 管理网格和所有色块
// ============================================================
class IcdBitLayoutScene : public QGraphicsScene {
  Q_OBJECT
 public:
  explicit IcdBitLayoutScene(QObject* parent = nullptr);

  void setFrame(int length_bytes, int bits_per_row = 32);
  BitBlockItem* addBlock(const QString& name, int byte_offset,
                         int start_bit, int bit_width, const QColor& color);
  void clearBlocks();
  void highlightBlock(const QString& name);

  int cellSize() const { return cell_size_; }

 signals:
  void blockClicked(const QString& name);

 protected:
  void drawBackground(QPainter* painter, const QRectF& rect) override;

 private:
  int frame_length_ = 16;
  int bits_per_row_ = 32;
  int cell_size_ = 34;
  int margin_left_ = 52;
  int margin_top_ = 32;
};

// ============================================================
// IcdBitLayoutView — 外层 QWidget + toolbar + QGraphicsView
// ============================================================
class IcdBitLayoutView : public QWidget {
  Q_OBJECT
 public:
  explicit IcdBitLayoutView(QWidget* parent = nullptr);

  void setFrameData(int length_bytes, int bits_per_row = 32);
  BitBlockItem* addBlock(const QString& name, int byte_offset,
                         int start_bit, int bit_width, const QColor& color);
  void clearBlocks();
  void highlightBlock(const QString& name);

 signals:
  void blockClicked(const QString& name);

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void initUi();
  void initPlaceholderBlocks();
  void fitToContent();

  IcdBitLayoutScene* scene_ = nullptr;
  QGraphicsView* view_ = nullptr;
  QComboBox* mode_combo_ = nullptr;
};

}  // namespace etest::protocal
