#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QWidget>

#include <memory>

#include "IEditor.h"
#include "SignalCodec.h"
#include "SignalRegistry.h"
#include "SignalResolver.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSplitter;
class QStackedWidget;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidgetItem;

namespace icd { class Repository; }

namespace etest::app {

// MockConfigEditor -- Mock 响应配置编辑器（独立编辑器，page0 tab）
// 编辑 MockResponses.emock：帧响应字段工程值 + AD 端口波形/序列 + DA 端口固定值
class MockConfigEditor : public QWidget, public IEditor {
  Q_OBJECT

 public:
  explicit MockConfigEditor(const QString& id, QWidget* parent = nullptr);
  ~MockConfigEditor() override = default;

  // ── IEditor ──
  QString displayName() const override;
  QString editorId() const override;
  QString editorType() const override;
  QString filePath() const override;
  QWidget* widget() override;
  QObject* signalObject() override;
  bool isModified() const override;
  bool save() override;
  bool saveAs(const QString& path) override;
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;
  void openFile(const QString& filePath) override;

  void setIcdRepository(icd::Repository* repo);

 signals:
  void modificationChanged(bool modified);

 private:
  void initUi();
  void initSignals();
  void loadTopologyAndResponses();
  void buildNavTree();

  QString file_path_;
  bool modified_ = false;

  // 拓扑 + Mock 响应数据
  QJsonObject topology_doc_;
  QJsonArray port_behaviors_;

  // 布局
  QSplitter* splitter_ = nullptr;
  QTreeWidget* nav_tree_ = nullptr;
  QStackedWidget* edit_area_ = nullptr;

  // DA 端口编辑页
  QWidget* da_port_page_ = nullptr;
  QLabel* da_port_label_ = nullptr;
  QDoubleSpinBox* da_value_spin_ = nullptr;

  // AD 端口编辑页（三模式切换）
  QWidget* ad_port_page_ = nullptr;
  QLabel* ad_port_label_ = nullptr;
  QComboBox* ad_mode_combo_ = nullptr;
  QStackedWidget* ad_mode_stack_ = nullptr;
  QDoubleSpinBox* ad_fixed_spin_ = nullptr;
  QComboBox* ad_wf_type_ = nullptr;
  QDoubleSpinBox* ad_wf_amplitude_ = nullptr;
  QDoubleSpinBox* ad_wf_frequency_ = nullptr;
  QDoubleSpinBox* ad_wf_offset_ = nullptr;
  QTableWidget* ad_series_table_ = nullptr;

  // 槽
  void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

  void updateHexPreview(const QString& replyFrameName);

  // ICD 编解码
  etest::core::SignalRegistry signal_registry_;
  std::unique_ptr<etest::engine::SignalResolver> signal_resolver_;
  etest::engine::SignalCodec signal_codec_;
  icd::Repository* icd_repo_ = nullptr;

  // 帧响应编辑页
  QWidget* frame_response_page_ = nullptr;
  QLabel* fr_info_label_ = nullptr;
  QComboBox* fr_reply_frame_ = nullptr;
  QTableWidget* fr_field_table_ = nullptr;
  QLabel* fr_hex_preview_ = nullptr;
  QPushButton* fr_add_row_btn_ = nullptr;
  QPushButton* fr_del_row_btn_ = nullptr;
  QPushButton* fr_del_resp_btn_ = nullptr;

  // 未配置提示页
  QWidget* unconfig_hint_page_ = nullptr;
  QPushButton* create_config_btn_ = nullptr;

  // serial/can/a429 端口编辑页
  QWidget* frame_port_page_ = nullptr;
  QLabel* frame_port_info_label_ = nullptr;
  QWidget* frame_port_resp_list_ = nullptr;
  QPushButton* fr_add_resp_btn_ = nullptr;
  QPushButton* fr_del_port_config_btn_ = nullptr;

  // UUT 概览页
  QWidget* uut_overview_page_ = nullptr;
  QTableWidget* uut_overview_table_ = nullptr;

  // 真实模式提示页
  QLabel* real_mode_hint_ = nullptr;

  bool isRealMode() const;

  // 当前编辑标识（用于回写）
  QString current_product_name_;
  QString current_device_id_;
  QString current_port_name_;
  QString current_frame_name_;
  QString current_reply_frame_name_;

  int findCurrentBehaviorIndex() const;
  int findCurrentResponseIndex() const;
  void onDaValueChanged();
  void onAdValueChanged();
  void onFrFieldChanged(QTableWidgetItem* item);
  void onReplyFrameChanged(const QString& text);
  void onFrAddRow();
  void onFrDeleteRow();
  void onCreateConfigClicked();
  void onFrDelPortConfigClicked();
  void onFrAddRespClicked();
  void onFrDelRespClicked();
  void onFrNodePathDoubleClicked(QTableWidgetItem* item);
  void rebuildFramePortRespList();
  void rebuildUutOverview();
  QString showIcdSignalPicker(const QString& currentPath);
  void markModified();
};

}  // namespace etest::app
