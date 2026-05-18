#pragma once

#include <QWidget>

#include "editor/IEditor.h"

class QComboBox;
class QLabel;
class QSplitter;

namespace etest::protocal {

class IcdNodeTreeWidget;
class IcdBitLayoutView;
class IcdPropertyPanel;

class ProtocalEditorWidget : public QWidget, public etest::app::IEditor {
  Q_OBJECT
 public:
  explicit ProtocalEditorWidget(QWidget* parent = nullptr);

  // IEditor interface
  QString displayName() const override;
  bool isModified() const override;
  bool save() override;
  bool saveAs(const QString& path) override;
  QString filePath() const override;
  QString editorId() const override;
  QWidget* widget() override;
  QString editorType() const override;
  QObject* signalObject() override;

  // Undo/Redo
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;

  void setEditorId(const QString& id);

 Q_SIGNALS:
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);

 private:
  void initUi();
  void initSignals();

  QSplitter* splitter_ = nullptr;
  IcdNodeTreeWidget* node_tree_ = nullptr;
  IcdBitLayoutView* bit_view_ = nullptr;
  IcdPropertyPanel* property_panel_ = nullptr;
  QLabel* status_label_ = nullptr;
  QLabel* frame_name_label_ = nullptr;
  QComboBox* frame_type_combo_ = nullptr;
  QComboBox* byte_order_combo_ = nullptr;

  QString current_file_;
  bool modified_ = false;
};

}  // namespace etest::protocal
