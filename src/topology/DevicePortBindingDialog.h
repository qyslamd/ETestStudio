#ifndef ETEST_TOPOLOGY_DEVICE_PORT_BINDING_DIALOG_H_
#define ETEST_TOPOLOGY_DEVICE_PORT_BINDING_DIALOG_H_

#include <QDialog>
#include <QStringList>

class QListWidget;
class QPushButton;
class QComboBox;

namespace etest::topology {

class TopologyDocument;

// 设备端口 ↔ ICD 帧绑定对话框。
// 左侧显示所有可用 ICD 帧，右侧显示当前端口已绑定的帧。
class DevicePortBindingDialog : public QDialog {
  Q_OBJECT

 public:
  // @param doc        拓扑文档（用于读写绑帧数据）
  // @param deviceIndex 设备索引
  // @param portIndex   端口索引
  // @param allFrames   所有可用的 ICD 帧名列表
  // @param parent      父窗口
  DevicePortBindingDialog(TopologyDocument* doc, int deviceIndex,
                          int portIndex, const QStringList& allFrames,
                          QWidget* parent = nullptr);

  // 获取对话框提交时用户选择的帧名（确认按钮触发后有效）
  QStringList selectedFrames() const { return selected_frames_; }

 private slots:
  void onAddFrames();
  void onRemoveFrames();
  void onFilterChanged(int index);
  void onAccept();

 private:
  void initUi();
  void populateAvailableFrames(const QStringList& allFrames);
  void populateBoundFrames();

  TopologyDocument* doc_;
  int device_index_;
  int port_index_;

  QListWidget* available_list_ = nullptr;
  QListWidget* bound_list_ = nullptr;
  QPushButton* add_btn_ = nullptr;
  QPushButton* remove_btn_ = nullptr;
  QComboBox* filter_combo_ = nullptr;

  QStringList all_frames_;  // 全部可用帧（不过滤）
  QStringList selected_frames_;
};

}  // namespace etest::topology

#endif  // ETEST_TOPOLOGY_DEVICE_PORT_BINDING_DIALOG_H_
