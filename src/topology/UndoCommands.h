#pragma once

#include <QUndoCommand>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QPair>
#include <functional>

#include "TopologyDocument.h"
#include "topology_items.h"

namespace etest::topology {

// ── AddProductCommand ──
class AddProductCommand : public QUndoCommand {
 public:
  AddProductCommand(TopologyDocument* doc, const TopologyProduct& product,
                    QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;
  int productIndex() const { return index_; }

 private:
  TopologyDocument* doc_;
  TopologyProduct product_;
  int index_ = -1;
};

// ── RemoveProductCommand ──
class RemoveProductCommand : public QUndoCommand {
 public:
  RemoveProductCommand(TopologyDocument* doc, int productIndex,
                       QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int index_;
  TopologyProduct product_;
  struct ConnEntry {
    QString productName;
    QString portName;
    QString deviceName;
    QString devicePort;
    PathStyle style = PathStyle::Bezier;
  };
  QVector<ConnEntry> saved_connections_;
};

// ── AddDeviceCommand ──
class AddDeviceCommand : public QUndoCommand {
 public:
  AddDeviceCommand(TopologyDocument* doc, const TopologyDevice& device,
                   QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;
  int deviceIndex() const { return index_; }

 private:
  TopologyDocument* doc_;
  TopologyDevice device_;
  int index_ = -1;
};

// ── RemoveDeviceCommand ──
class RemoveDeviceCommand : public QUndoCommand {
 public:
  RemoveDeviceCommand(TopologyDocument* doc, int deviceIndex,
                      QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int index_;
  TopologyDevice device_;
  struct ConnEntry {
    QString productName;
    QString portName;
    QString deviceName;
    QString devicePort;
    PathStyle style = PathStyle::Bezier;
  };
  QVector<ConnEntry> saved_connections_;
};

// ── AddConnectionCommand ──
class AddConnectionCommand : public QUndoCommand {
 public:
  AddConnectionCommand(TopologyDocument* doc, const TopologyConnection& conn,
                       QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  TopologyConnection conn_;
  int index_ = -1;
};

// ── RemoveConnectionCommand ──
class RemoveConnectionCommand : public QUndoCommand {
 public:
  RemoveConnectionCommand(TopologyDocument* doc, int connectionIndex,
                          QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  struct SavedTap {
    int monitorIndex;
    TopologyMonitorTap tap;
  };
  TopologyDocument* doc_;
  int index_;
  TopologyConnection conn_;
  QVector<SavedTap> saved_taps_;
};

// ── MoveProductCommand ──
class MoveProductCommand : public QUndoCommand {
 public:
  MoveProductCommand(TopologyDocument* doc, int productIndex,
                     const QPointF& oldPos, const QPointF& newPos,
                     QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int index_;
  QPointF old_pos_;
  QPointF new_pos_;
};

// ── MoveDeviceCommand ──
class MoveDeviceCommand : public QUndoCommand {
 public:
  MoveDeviceCommand(TopologyDocument* doc, int deviceIndex,
                    const QPointF& oldPos, const QPointF& newPos,
                    QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int index_;
  QPointF old_pos_;
  QPointF new_pos_;
};

// ── MoveMonitorCommand ──
class MoveMonitorCommand : public QUndoCommand {
 public:
  MoveMonitorCommand(TopologyDocument* doc, int monitorIndex,
                     const QPointF& oldPos, const QPointF& newPos,
                     QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int index_;
  QPointF old_pos_;
  QPointF new_pos_;
};

// ── PropertyCommand ── generic undoable property change
class PropertyCommand : public QUndoCommand {
 public:
  using ApplyFn = std::function<void()>;
  PropertyCommand(TopologyDocument* doc, ApplyFn undoFn, ApplyFn redoFn,
                  const QString& text, QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  ApplyFn undo_fn_;
  ApplyFn redo_fn_;
};

// ── SetProductPortStyleCommand ──
class SetProductPortStyleCommand : public QUndoCommand {
 public:
  SetProductPortStyleCommand(TopologyDocument* doc, int productIndex,
                             int portIndex, PortStyle newStyle,
                             QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int product_index_;
  int port_index_;
  int old_style_;
  int new_style_;
};

// ── SetDevicePortStyleCommand ──
class SetDevicePortStyleCommand : public QUndoCommand {
 public:
  SetDevicePortStyleCommand(TopologyDocument* doc, int deviceIndex,
                            int portIndex, PortStyle newStyle,
                            QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int device_index_;
  int port_index_;
  int old_style_;
  int new_style_;
};

// ── SetConnectionStyleCommand ──
class SetConnectionStyleCommand : public QUndoCommand {
 public:
  SetConnectionStyleCommand(TopologyDocument* doc, int connectionIndex,
                            PathStyle newStyle,
                            QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int connection_index_;
  PathStyle old_style_;
  PathStyle new_style_;
};

// ── ResizeItemCommand ──
class ResizeItemCommand : public QUndoCommand {
 public:
  enum Type { Product, Device, Monitor };

  ResizeItemCommand(TopologyDocument* doc, int index, Type type,
                    const QSizeF& oldSize, const QSizeF& newSize,
                    const QPointF& oldPos, const QPointF& newPos,
                    QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int index_;
  Type type_;
  QSizeF old_size_;
  QSizeF new_size_;
  QPointF old_pos_;
  QPointF new_pos_;
};

// ── AddMonitorCommand ──
class AddMonitorCommand : public QUndoCommand {
 public:
  AddMonitorCommand(TopologyDocument* doc, const TopologyMonitor& monitor,
                    QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;
  int monitorIndex() const { return index_; }

 private:
  TopologyDocument* doc_;
  TopologyMonitor monitor_;
  int index_ = -1;
};

// ── RemoveMonitorCommand ──
class RemoveMonitorCommand : public QUndoCommand {
 public:
  RemoveMonitorCommand(TopologyDocument* doc, int monitorIndex,
                       QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int index_;
  TopologyMonitor monitor_;
};

// ── RemoveDevicePortCommand ──
class RemoveDevicePortCommand : public QUndoCommand {
 public:
  RemoveDevicePortCommand(TopologyDocument* doc, int deviceIndex, int portIndex,
                          QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int device_index_;
  int port_index_;
  TopologyDevicePort port_;
};

// ── AddProductPortCommand ──
class AddProductPortCommand : public QUndoCommand {
 public:
  AddProductPortCommand(TopologyDocument* doc, int productIndex,
                        const TopologyPort& port,
                        QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int product_index_;
  TopologyPort port_;
  int port_index_ = -1;
};

// ── RemoveProductPortCommand ──
class RemoveProductPortCommand : public QUndoCommand {
 public:
  RemoveProductPortCommand(TopologyDocument* doc, int productIndex,
                           int portIndex, QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int product_index_;
  int port_index_;
  TopologyPort port_;
  struct ConnEntry {
    QString productName;
    QString portName;
    QString deviceName;
    QString devicePort;
    PathStyle style = PathStyle::Bezier;
  };
  QVector<ConnEntry> saved_connections_;
};

// ── TapConnectionCommand ──
class TapConnectionCommand : public QUndoCommand {
 public:
  TapConnectionCommand(TopologyDocument* doc, int monitorIndex,
                       const TopologyMonitorTap& tap,
                       QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int monitor_index_;
  TopologyMonitorTap tap_;
};

// ── UnTapConnectionCommand ──
class UnTapConnectionCommand : public QUndoCommand {
 public:
  UnTapConnectionCommand(TopologyDocument* doc, int monitorIndex, int tapIndex,
                         QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;

 private:
  TopologyDocument* doc_;
  int monitor_index_;
  int tap_index_;
  TopologyMonitorTap tap_;
};

}  // namespace etest::topology
