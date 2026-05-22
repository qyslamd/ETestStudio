#pragma once

#include <QWidget>

#include "api/IEditor.h"

#include <icd/frame.hpp>
#include <icd/repository.hpp>

class QComboBox;
class QLabel;
class QSplitter;
class QToolButton;

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

 signals:
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);

 private:
  bool loadEproto(const QString& path);
  bool saveEproto(const QString& path);
  void initUi();
  void initSignals();
  void updateToolbar();
  void clearAll();
  void setModified(bool modified);
  void setCurrentFrame(const icd::Frame* frame);
  void populateFrames();

  QSplitter* splitter_ = nullptr;
  IcdNodeTreeWidget* node_tree_ = nullptr;
  IcdBitLayoutView* bit_view_ = nullptr;
  IcdPropertyPanel* property_panel_ = nullptr;
  QLabel* status_label_ = nullptr;
  QLabel* frame_name_label_ = nullptr;
  QLabel* frame_id_label_ = nullptr;
  QLabel* frame_length_label_ = nullptr;
  QComboBox* frame_type_combo_ = nullptr;
  QComboBox* byte_order_combo_ = nullptr;
  QToolButton* new_frame_btn_ = nullptr;
  QToolButton* delete_frame_btn_ = nullptr;

  icd::Repository repo_;
  const icd::Frame* current_frame_ = nullptr;

  QString current_file_;
  bool modified_ = false;
};

}  // namespace etest::protocal
