#pragma once

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QString>
#include <QVector>

#include "TopologyBlockItem.h"

namespace etest::topology {

class TopologyDocument;
class TopologyScene;
class UutItem;
class DeviceItem;
class PortItem;

// ── PortItem ── pin node on UUT edge ─────────────────────────────

class PortItem : public QGraphicsItem {
 public:
  enum { Type = UserType + 1 };
  int type() const override { return Type; }

  PortItem(int productIndex,
           int portIndex,
           TopologyDocument* doc,
           UutItem* parent);

  QRectF boundingRect() const override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;
  QPainterPath shape() const override;

  int productIndex() const { return product_index_; }
  int portIndex() const { return port_index_; }
  bool isHovered() const { return hovered_; }

  QPointF sceneCenter() const;

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

 private:
  int product_index_;
  int port_index_;
  TopologyDocument* doc_;
  QPointF press_pos_;
  bool hovered_ = false;
  static constexpr qreal kRadius = 6.0;
};

// ── UutItem ── product block ─────────────────────────────────────

class UutItem : public TopologyBlockItem {
 public:
  enum { Type = UserType + 2 };
  int type() const override { return Type; }

  UutItem(int productIndex,
          TopologyDocument* doc,
          QGraphicsItem* parent = nullptr);

  int productIndex() const { return product_index_; }

  void layoutPorts();
  PortItem* portItem(int portIndex) const;
  QPointF portScenePos(int portIndex) const;

 protected:
  QColor blockFillColor() const override;
  QColor blockBorderColor() const override;
  void paintContent(QPainter* painter,
                    const QStyleOptionGraphicsItem* option,
                    const QRectF& rect) override;
  qreal calcContentHeight() const override;
  bool hasChildHovered() const override;

 private:
  int product_index_;
  QVector<PortItem*> ports_;

  static constexpr qreal kWidth = 140.0;
  static constexpr qreal kBaseHeight = 60.0;
  static constexpr qreal kPortMargin = 8.0;
  static constexpr qreal kCornerRadius = 8.0;
};

// ── DevicePortItem ── connection point on device left edge ───────

class DevicePortItem : public QGraphicsItem {
 public:
  enum { Type = UserType + 5 };
  int type() const override { return Type; }

  DevicePortItem(int deviceIndex,
                 int portIndex,
                 TopologyDocument* doc,
                 DeviceItem* parent);

  QRectF boundingRect() const override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;
  QPainterPath shape() const override;

  int deviceIndex() const { return device_index_; }
  int portIndex() const { return port_index_; }
  bool isHovered() const { return hovered_; }
  DeviceItem* parentDeviceItem() const;
  QPointF sceneCenter() const;

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

 private:
  int device_index_;
  int port_index_;
  TopologyDocument* doc_;
  QPointF press_pos_;
  bool hovered_ = false;
  static constexpr qreal kRadius = 6.0;
};

// ── DeviceItem ── device block ───────────────────────────────────

class DeviceItem : public TopologyBlockItem {
 public:
  enum { Type = UserType + 3 };
  int type() const override { return Type; }

  DeviceItem(int deviceIndex,
             TopologyDocument* doc,
             QGraphicsItem* parent = nullptr);

  int deviceIndex() const { return device_index_; }
  QString deviceType() const;
  QPointF connectionPoint() const;

  void layoutDevicePorts();
  DevicePortItem* devicePortItem(int portIndex) const;

 protected:
  QColor blockFillColor() const override;
  QColor blockBorderColor() const override;
  void paintContent(QPainter* painter,
                    const QStyleOptionGraphicsItem* option,
                    const QRectF& rect) override;
  qreal calcContentHeight() const override;
  bool hasChildHovered() const override;

 private:
  int device_index_;
  QVector<DevicePortItem*> device_port_items_;

  static constexpr qreal kWidth = 120.0;
  static constexpr qreal kBaseHeight = 50.0;
  static constexpr qreal kPortMargin = 10.0;
  static constexpr qreal kCornerRadius = 10.0;
};

// ── ConnectionItem ── bezier line from UUT port to device port ──

class ConnectionItem : public QGraphicsPathItem {
 public:
  enum { Type = UserType + 4 };
  int type() const override { return Type; }

  ConnectionItem(PortItem* source,
                 DevicePortItem* target,
                 const QString& devicePort,
                 TopologyDocument* doc,
                 QGraphicsItem* parent = nullptr);
  ~ConnectionItem() override;

  void updatePath();

  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  PortItem* sourcePort() const { return source_; }
  DevicePortItem* targetDevicePort() const { return target_port_; }
  DeviceItem* targetDevice() const;
  QString devicePort() const { return device_port_; }

 private:
  PortItem* source_;
  DevicePortItem* target_port_;
  QString device_port_;
  TopologyDocument* doc_;
  QPainterPath arrow_path_;
};

// ── LegendItem ── color legend overlay ───────────────────────────

class LegendItem : public QGraphicsItem {
 public:
  LegendItem(QGraphicsItem* parent = nullptr);
  QRectF boundingRect() const override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;
};

}  // namespace etest::topology
