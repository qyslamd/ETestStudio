#ifndef ETEST_APP_SIGNAL_SELECTION_DIALOG_H_
#define ETEST_APP_SIGNAL_SELECTION_DIALOG_H_

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QTreeView;
class QStandardItemModel;

namespace icd {
class Repository;
}  // namespace icd

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace etest::app {

// 信号选择对话框：设备 → 端口 → ICD 帧 → 节点树，四步联动选择信号。
class SignalSelectionDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SignalSelectionDialog(const etest::core::SignalRegistry* registry,
                                 const icd::Repository* repository,
                                 QWidget* parent = nullptr);

  QString selectedUuid() const { return current_uuid_; }

 private slots:
  void onDeviceChanged(int index);
  void onPortChanged(int index);
  void onFrameChanged(int index);
  void onNodeSelected(const QModelIndex& index);

 private:
  void populateDevices();
  void populatePorts(const QString& deviceId);
  void populateFrames(const QString& deviceId, const QString& portName);
  void populateNodes(const QString& frameName);
  void updateInfoLabel();
  void updateOkButton();

  const etest::core::SignalRegistry* registry_;
  const icd::Repository* repository_;
  QString current_uuid_;

  QComboBox* device_combo_ = nullptr;
  QComboBox* port_combo_ = nullptr;
  QComboBox* frame_combo_ = nullptr;
  QTreeView* node_tree_ = nullptr;
  QStandardItemModel* node_model_ = nullptr;
  QLabel* info_label_ = nullptr;
  QLabel* uuid_label_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_SIGNAL_SELECTION_DIALOG_H_
