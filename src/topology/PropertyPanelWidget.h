#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableView>
#include <QWidget>

#include "TopologyDocument.h"

class QGraphicsItem;
class QStandardItemModel;
class ComboBoxDelegate;

namespace etest::topology {
class PropertyPanelWidget : public QWidget {
  Q_OBJECT
 public:
  explicit PropertyPanelWidget(TopologyDocument* doc,
                               QWidget* parent = nullptr);

  void showPropertiesFor(QGraphicsItem* item);
  void clearPanel();

 signals:
  void documentChanged();

 private:
  void buildEmptyPage();
  void buildUutPage();
  void buildPortPage();
  void buildDevicePage();
  void buildConnectionPage();
  void buildDevicePortPage();
  void buildMonitorPage();

  void onUutNameChanged();
  void onUutAddPort();
  void onUutRemovePort();
  void applyUutPorts(int productIndex);
  void onPortNameChanged();
  void onPortDirectionChanged();
  void onPortAllowedTypesChanged();
  void onPortFunctionTypeChanged();
  void onDeviceNameChanged();
  void onDeviceTypeChanged();
  void onAddPropertyRow();
  void onRemovePropertyRow();

  void onAddDevicePortRow();
  void onRemoveDevicePortRow();
  void onDevicePortFunctionTypeChanged(int row);
  void onDevicePortDirectionChanged(int row);

  void onDevicePortNameChanged();
  void onDevicePortFunctionTypeChanged();
  void onDevicePortDirectionChanged();

  void applyDeviceProperties(int deviceIndex);
  void applyDevicePorts(int deviceIndex);

  TopologyDocument* doc_;
  QStackedWidget* stack_;

  // Saved state for undo support (device page table edits)
  QVector<QPair<QString, QString>> saved_device_properties_;
  QVector<TopologyDevicePort> saved_device_ports_;

  // Index mapping: 0=empty, 1=uut, 2=port, 3=device, 4=connection, 5=deviceport, 6=monitor
  enum Page {
    PageEmpty = 0,
    PageUut,
    PagePort,
    PageDevice,
    PageConnection,
    PageDevicePort,
    PageMonitor
  };

  // UUT page widgets
  QLineEdit* uut_name_edit_ = nullptr;
  QTableWidget* uut_port_table_ = nullptr;
  QPushButton* uut_add_port_btn_ = nullptr;
  QPushButton* uut_remove_port_btn_ = nullptr;
  int editing_uut_index_ = -1;
  bool uut_dirty_ = false;
  QVector<TopologyPort> saved_uut_ports_;

  // Port page widgets
  QLineEdit* port_name_edit_ = nullptr;
  QComboBox* port_direction_combo_ = nullptr;
  QComboBox* port_allowed_types_edit_ = nullptr;
  QComboBox* port_function_combo_ = nullptr;
  QComboBox* port_style_combo_ = nullptr;
  int editing_port_product_ = -1;
  int editing_port_index_ = -1;
  void onPortStyleChanged();

  // Device page widgets
  QLineEdit* device_name_edit_ = nullptr;
  QLineEdit* device_type_edit_ = nullptr;
  QTableWidget* device_props_table_ = nullptr;
  QPushButton* add_prop_btn_ = nullptr;
  QPushButton* remove_prop_btn_ = nullptr;
  QTableView* device_port_view_ = nullptr;
  QStandardItemModel* device_port_model_ = nullptr;
  ComboBoxDelegate* direction_delegate_ = nullptr;
  ComboBoxDelegate* function_delegate_ = nullptr;
  QPushButton* add_port_btn_ = nullptr;
  QPushButton* remove_port_btn_ = nullptr;
  int editing_device_index_ = -1;
  bool device_dirty_ = false;

  // Connection page widgets
  QLabel* conn_source_label_ = nullptr;
  QLabel* conn_target_label_ = nullptr;
  QLabel* conn_device_port_label_ = nullptr;
  QComboBox* conn_style_combo_ = nullptr;
  int editing_conn_index_ = -1;
  void onConnStyleChanged();

  // DevicePort page widgets
  QLineEdit* devport_name_edit_ = nullptr;
  QComboBox* devport_direction_combo_ = nullptr;
  QComboBox* devport_function_combo_ = nullptr;
  QComboBox* devport_style_combo_ = nullptr;
  int editing_device_port_device_ = -1;
  int editing_device_port_index_ = -1;
  void onDevicePortStyleChanged();

  // Monitor page widgets
  QLineEdit* monitor_name_edit_ = nullptr;
  QLabel* monitor_type_label_ = nullptr;
  QTableWidget* monitor_taps_table_ = nullptr;
  int editing_monitor_index_ = -1;
  void onTapTableContextMenu(const QPoint& pos);
};

}  // namespace etest::topology
