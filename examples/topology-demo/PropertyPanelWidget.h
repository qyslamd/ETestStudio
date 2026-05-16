#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>

class QGraphicsItem;

namespace topology {

class TopologyDocument;

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

    void onUutNameChanged();
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

    // Index mapping: 0=empty, 1=uut, 2=port, 3=device, 4=connection, 5=deviceport
    enum Page { PageEmpty = 0, PageUut, PagePort, PageDevice, PageConnection, PageDevicePort };

    // UUT page widgets
    QLineEdit* uut_name_edit_ = nullptr;
    int editing_uut_index_ = -1;

    // Port page widgets
    QLineEdit* port_name_edit_ = nullptr;
    QComboBox* port_direction_combo_ = nullptr;
    QComboBox* port_allowed_types_edit_ = nullptr;
    QComboBox* port_function_combo_ = nullptr;
    int editing_port_product_ = -1;
    int editing_port_index_ = -1;

    // Device page widgets
    QLineEdit* device_name_edit_ = nullptr;
    QLineEdit* device_type_edit_ = nullptr;
    QTableWidget* device_props_table_ = nullptr;
    QPushButton* add_prop_btn_ = nullptr;
    QPushButton* remove_prop_btn_ = nullptr;
    QTableWidget* device_port_table_ = nullptr;
    QPushButton* add_port_btn_ = nullptr;
    QPushButton* remove_port_btn_ = nullptr;
    int editing_device_index_ = -1;

    // Connection page widgets
    QLabel* conn_source_label_ = nullptr;
    QLabel* conn_target_label_ = nullptr;
    QLabel* conn_device_port_label_ = nullptr;

    // DevicePort page widgets
    QLineEdit* devport_name_edit_ = nullptr;
    QComboBox* devport_direction_combo_ = nullptr;
    QComboBox* devport_function_combo_ = nullptr;
    int editing_device_port_device_ = -1;
    int editing_device_port_index_ = -1;
};

}  // namespace topology
