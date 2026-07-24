#pragma once

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QString>
#include <QVector>

#include "TopologyBlockItem.h"
#include "TopologyPathRouter.h"

namespace etest::topology {

class TopologyDocument;
class TopologyScene;
class UutItem;
class DeviceItem;
class MonitorItem;

enum class PortStyle { Circle, Triangle };

// ── AbstractPortItem ── common base for UUT and device port nodes ──

class AbstractPortItem : public QGraphicsItem {
 public:
  int portIndex() const { return port_index_; }
  bool isHovered() const { return hovered_; }
  PortStyle portStyle() const { return port_style_; }
  virtual void setPortStyle(PortStyle s) { port_style_ = s; update(); }
  virtual QPointF sceneCenter() const = 0;

 protected:
  AbstractPortItem(int portIndex,
                   TopologyDocument* doc,
                   QGraphicsItem* parent = nullptr);

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  int port_index_;
  TopologyDocument* doc_;
  QPointF press_pos_;
  bool hovered_ = false;
  PortStyle port_style_ = PortStyle::Circle;
  static constexpr qreal kRadius = 6.0;
};

// ── UutPortItem ── pin node on UUT edge ────────────────────────────

class UutPortItem : public AbstractPortItem {
 public:
  enum { Type = UserType + 1 };
  int type() const override { return Type; }

  UutPortItem(int productIndex,
              int portIndex,
              TopologyDocument* doc,
              UutItem* parent);

  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  void setPortStyle(PortStyle s) override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;
  QPointF sceneCenter() const override;

  int productIndex() const { return product_index_; }

 private:
  int product_index_;
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
  void clearPorts();
  UutPortItem* portItem(int portIndex) const;
  QPointF portScenePos(int portIndex) const;

 protected:
  QColor blockFillColor() const override;
  QColor blockBorderColor() const override;
  void paintContent(QPainter* painter,
                    const QStyleOptionGraphicsItem* option,
                    const QRectF& rect) override;
  qreal calcContentHeight() const override;
  bool hasChildHovered() const override;
  void onResizeFinished(const QSizeF& oldSize, const QPointF& oldPos) override;

 private:
  int product_index_;
  QVector<UutPortItem*> ports_;

  static constexpr qreal kWidth = 140.0;
  static constexpr qreal kBaseHeight = 60.0;
  static constexpr qreal kPortMargin = 8.0;
  static constexpr qreal kCornerRadius = 8.0;
};

// ── DevicePortItem ── connection point on device left edge ───────

class DevicePortItem : public AbstractPortItem {
 public:
  enum { Type = UserType + 5 };
  int type() const override { return Type; }

  DevicePortItem(int deviceIndex,
                 int portIndex,
                 TopologyDocument* doc,
                 DeviceItem* parent);

  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  void setPortStyle(PortStyle s) override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;
  QPointF sceneCenter() const override;

  int deviceIndex() const { return device_index_; }
  DeviceItem* parentDeviceItem() const;

 private:
  int device_index_;
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
  void onResizeFinished(const QSizeF& oldSize, const QPointF& oldPos) override;

 private:
  int device_index_;
  QVector<DevicePortItem*> device_port_items_;

  static constexpr qreal kWidth = 120.0;
  static constexpr qreal kBaseHeight = 50.0;
  static constexpr qreal kPortMargin = 10.0;
  static constexpr qreal kCornerRadius = 10.0;
};

// ── ConnectionItem ── line from UUT port to device port ──

class ConnectionItem : public QGraphicsPathItem {
 public:
  enum { Type = UserType + 4 };
  int type() const override { return Type; }

  ConnectionItem(UutPortItem* source,
                 DevicePortItem* target,
                 const QString& devicePort,
                 TopologyDocument* doc,
                 QGraphicsItem* parent = nullptr);
  ~ConnectionItem() override;

  void updatePath();
  void setStyle(PathStyle s);
  PathStyle style() const { return style_; }

  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  UutPortItem* sourcePort() const { return source_; }
  DevicePortItem* targetDevicePort() const { return target_port_; }
  DeviceItem* targetDevice() const;
  QString devicePort() const { return device_port_; }

  void setConnectionIndex(int idx) { conn_index_ = idx; }
  int connectionIndex() const { return conn_index_; }

  /// 设置该连线上是否有监听器及其索引（供 badge 显示）
  void setMonitorState(bool hasMonitor, int monitorIndex);

 protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

 private:
  UutPortItem* source_;
  DevicePortItem* target_port_;
  QString device_port_;
  TopologyDocument* doc_;
  QPainterPath arrow_path_;
  PathStyle style_ = PathStyle::Bezier;
  int conn_index_ = -1;
  bool has_monitor_ = false;
  int monitor_index_ = -1;
  static constexpr qreal kBadgeRadius = 8.0;
};

class MonitorPortItem;

// ── MonitorItem ── passive monitoring device ────────────────────

class MonitorItem : public TopologyBlockItem {
 public:
  enum { Type = UserType + 6 };
  int type() const override { return Type; }

  MonitorItem(int monitorIndex,
              TopologyDocument* doc,
              QGraphicsItem* parent = nullptr);

  int monitorIndex() const { return monitor_index_; }
  MonitorPortItem* monitorPortItem() const { return port_; }

  int tapCount() const;

  // Per-tap hover state (for channel dots)
  int hoveredTapIndex() const { return hovered_tap_index_; }

 protected:
  QColor blockFillColor() const override;
  QColor blockBorderColor() const override;
  QPen blockBorderPen(qreal penWidth) const override;
  void paintContent(QPainter* painter,
                    const QStyleOptionGraphicsItem* option,
                    const QRectF& rect) override;
  qreal calcContentHeight() const override;
  void onResizeFinished(const QSizeF& oldSize, const QPointF& oldPos) override;

  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

 private:
  void layoutPort();
  void updateDotHover(const QPointF& localPos);

  int monitor_index_;
  MonitorPortItem* port_ = nullptr;

  int hovered_tap_index_ = -1;

  static constexpr qreal kWidth = 120.0;
  static constexpr qreal kBaseHeight = 80.0;
  static constexpr qreal kCornerRadius = 10.0;
};

// ── MonitorPortItem ── drag anchor on MonitorItem left edge ────────

class MonitorPortItem : public AbstractPortItem {
 public:
  enum { Type = UserType + 7 };
  int type() const override { return Type; }

  MonitorPortItem(int monitorIndex,
                  TopologyDocument* doc,
                  MonitorItem* parent);

  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;
  QPointF sceneCenter() const override;

  int monitorIndex() const { return monitor_index_; }

 private:
  int monitor_index_;
};

}  // namespace etest::topology
