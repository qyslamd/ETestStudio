#pragma once

#include <QUndoCommand>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QPair>
#include <functional>

#include "TopologyDocument.h"

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
  TopologyDocument* doc_;
  int index_;
  TopologyConnection conn_;
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

// ── ResizeItemCommand ──
class ResizeItemCommand : public QUndoCommand {
 public:
  enum Type { Product, Device };

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

}  // namespace etest::topology
