#include "MockConfigEditor.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QJsonDocument>
#include <QLabel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "logger/Logger.h"

#include <icd/repository.hpp>

#include "plugin_sdk/PluginManager.h"

namespace {

class DoubleSpinDelegate : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;
  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                        const QModelIndex&) const override {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(-999999, 999999);
    spin->setDecimals(4);
    return spin;
  }
};

}  // namespace

namespace etest::app {

MockConfigEditor::MockConfigEditor(const QString& id, QWidget* parent)
    : QWidget(parent), file_path_(id) {
  initUi();
  initSignals();
}

void MockConfigEditor::initUi() {
  splitter_ = new QSplitter(Qt::Horizontal, this);

  nav_tree_ = new QTreeWidget(splitter_);
  nav_tree_->setHeaderHidden(true);
  nav_tree_->setMinimumWidth(180);

  edit_area_ = new QStackedWidget(splitter_);

  // DA 端口编辑页（fixedValue）
  da_port_page_ = new QWidget();
  auto* daLayout = new QFormLayout(da_port_page_);
  da_port_label_ = new QLabel(da_port_page_);
  da_value_spin_ = new QDoubleSpinBox(da_port_page_);
  da_value_spin_->setRange(-999999, 999999);
  da_value_spin_->setSuffix(QStringLiteral(" V"));
  daLayout->addRow(QStringLiteral("端口:"), da_port_label_);
  daLayout->addRow(QStringLiteral("固定值:"), da_value_spin_);
  edit_area_->addWidget(da_port_page_);

  // AD 端口编辑页（三模式切换）
  ad_port_page_ = new QWidget();
  auto* adLayout = new QVBoxLayout(ad_port_page_);
  ad_port_label_ = new QLabel(ad_port_page_);
  ad_mode_combo_ = new QComboBox(ad_port_page_);
  ad_mode_combo_->addItems({QStringLiteral("固定值"), QStringLiteral("波形"), QStringLiteral("序列")});
  ad_mode_stack_ = new QStackedWidget(ad_port_page_);
  // 固定值页
  auto* fixedPage = new QWidget();
  auto* fixedLayout = new QFormLayout(fixedPage);
  ad_fixed_spin_ = new QDoubleSpinBox(fixedPage);
  ad_fixed_spin_->setRange(-999999, 999999);
  ad_fixed_spin_->setSuffix(QStringLiteral(" V"));
  fixedLayout->addRow(QStringLiteral("模拟值:"), ad_fixed_spin_);
  ad_mode_stack_->addWidget(fixedPage);
  // 波形页
  auto* wfPage = new QWidget();
  auto* wfLayout = new QFormLayout(wfPage);
  ad_wf_type_ = new QComboBox(wfPage);
  ad_wf_type_->addItems({QStringLiteral("正弦"), QStringLiteral("方波"), QStringLiteral("三角波")});
  ad_wf_amplitude_ = new QDoubleSpinBox(wfPage);
  ad_wf_amplitude_->setRange(0, 999999);
  ad_wf_frequency_ = new QDoubleSpinBox(wfPage);
  ad_wf_frequency_->setRange(0, 999999);
  ad_wf_frequency_->setSuffix(QStringLiteral(" Hz"));
  ad_wf_offset_ = new QDoubleSpinBox(wfPage);
  ad_wf_offset_->setRange(-999999, 999999);
  wfLayout->addRow(QStringLiteral("类型:"), ad_wf_type_);
  wfLayout->addRow(QStringLiteral("幅值:"), ad_wf_amplitude_);
  wfLayout->addRow(QStringLiteral("频率:"), ad_wf_frequency_);
  wfLayout->addRow(QStringLiteral("偏置:"), ad_wf_offset_);
  ad_mode_stack_->addWidget(wfPage);
  // 序列页
  auto* seriesPage = new QWidget();
  auto* seriesLayout = new QVBoxLayout(seriesPage);
  ad_series_table_ = new QTableWidget(seriesPage);
  ad_series_table_->setColumnCount(2);
  ad_series_table_->setHorizontalHeaderLabels({QStringLiteral("索引"), QStringLiteral("值(V)")});
  seriesLayout->addWidget(ad_series_table_);
  ad_mode_stack_->addWidget(seriesPage);
  adLayout->addWidget(ad_port_label_);
  adLayout->addWidget(ad_mode_combo_);
  adLayout->addWidget(ad_mode_stack_);
  adLayout->addStretch();
  edit_area_->addWidget(ad_port_page_);
  // 模式切换
  // 帧响应编辑页
  frame_response_page_ = new QWidget();
  auto* frLayout = new QVBoxLayout(frame_response_page_);
  fr_info_label_ = new QLabel(frame_response_page_);
  fr_reply_frame_ = new QComboBox(frame_response_page_);
  fr_field_table_ = new QTableWidget(frame_response_page_);
  fr_field_table_->setColumnCount(4);
  fr_field_table_->setItemDelegateForColumn(
      1, new DoubleSpinDelegate(fr_field_table_));
  fr_field_table_->setHorizontalHeaderLabels(
      {QStringLiteral("字段路径"), QStringLiteral("工程值"),
       QStringLiteral("单位"), QStringLiteral("hex")});
  fr_hex_preview_ = new QLabel(frame_response_page_);
  QFont monoFont = fr_hex_preview_->font();
  monoFont.setFamily(QStringLiteral("Consolas"));
  fr_hex_preview_->setFont(monoFont);
  frLayout->addWidget(fr_info_label_);
  frLayout->addWidget(new QLabel(QStringLiteral("回复帧:"), frame_response_page_));
  frLayout->addWidget(fr_reply_frame_);
  frLayout->addWidget(fr_field_table_);

  // 添加/删除行按钮
  auto* frBtnLayout = new QHBoxLayout();
  fr_add_row_btn_ = new QPushButton(QStringLiteral("添加行"));
  fr_del_row_btn_ = new QPushButton(QStringLiteral("删除行"));
  frBtnLayout->addWidget(fr_add_row_btn_);
  frBtnLayout->addWidget(fr_del_row_btn_);
  frBtnLayout->addStretch();
  frLayout->addLayout(frBtnLayout);

  frLayout->addWidget(new QLabel(QStringLiteral("帧预览:"), frame_response_page_));
  frLayout->addWidget(fr_hex_preview_);
  frLayout->addStretch();
  edit_area_->addWidget(frame_response_page_);

  // 真实模式提示页
  real_mode_hint_ = new QLabel(QStringLiteral("真实模式项目无需 Mock 配置"), splitter_);
  real_mode_hint_->setAlignment(Qt::AlignCenter);
  edit_area_->addWidget(real_mode_hint_);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(splitter_);

  splitter_->setSizes({220, 580});
}

void MockConfigEditor::initSignals() {
  connect(nav_tree_, &QTreeWidget::currentItemChanged,
          this, &MockConfigEditor::onCurrentItemChanged);
  connect(ad_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          ad_mode_stack_, &QStackedWidget::setCurrentIndex);
  // 编辑回写
  connect(da_value_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MockConfigEditor::onDaValueChanged);
  connect(ad_fixed_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MockConfigEditor::onAdValueChanged);
  connect(ad_wf_amplitude_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MockConfigEditor::onAdValueChanged);
  connect(ad_wf_frequency_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MockConfigEditor::onAdValueChanged);
  connect(ad_wf_offset_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MockConfigEditor::onAdValueChanged);
  connect(ad_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MockConfigEditor::onAdValueChanged);
  connect(fr_field_table_, &QTableWidget::itemChanged,
          this, &MockConfigEditor::onFrFieldChanged);
  connect(fr_reply_frame_, &QComboBox::currentTextChanged,
          this, &MockConfigEditor::onReplyFrameChanged);
  connect(fr_add_row_btn_, &QPushButton::clicked,
          this, &MockConfigEditor::onFrAddRow);
  connect(fr_del_row_btn_, &QPushButton::clicked,
          this, &MockConfigEditor::onFrDeleteRow);
}

void MockConfigEditor::setIcdRepository(icd::Repository* repo) {
  icd_repo_ = repo;
  if (repo) {
    signal_resolver_ = std::make_unique<etest::engine::SignalResolver>(
        &signal_registry_, repo);
  }
}

// ── IEditor 实现 ──

QString MockConfigEditor::displayName() const {
  return QStringLiteral("Mock 配置");
}

QString MockConfigEditor::editorId() const {
  return file_path_.isEmpty()
             ? QStringLiteral("editor://mockconfig/new")
             : file_path_;
}

QString MockConfigEditor::editorType() const {
  return QStringLiteral("mockconfig");
}

QString MockConfigEditor::filePath() const {
  return file_path_;
}

QWidget* MockConfigEditor::widget() {
  return this;
}

QObject* MockConfigEditor::signalObject() {
  return this;
}

bool MockConfigEditor::isModified() const {
  return modified_;
}

bool MockConfigEditor::save() {
  if (file_path_.isEmpty() || file_path_.startsWith("editor://")) {
    return false;
  }
  QJsonObject root;
  root["version"] = "1.0";
  root["portBehaviors"] = port_behaviors_;
  QJsonDocument doc(root);
  QFile file(file_path_);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  modified_ = false;
  emit modificationChanged(false);
  return true;
}

bool MockConfigEditor::saveAs(const QString& path) {
  file_path_ = path;
  return save();
}

bool MockConfigEditor::canUndo() const {
  return false;
}

bool MockConfigEditor::canRedo() const {
  return false;
}

void MockConfigEditor::undo() {}

void MockConfigEditor::redo() {}

void MockConfigEditor::openFile(const QString& filePath) {
  file_path_ = filePath;
  loadTopologyAndResponses();
  if (isRealMode()) {
    nav_tree_->clear();
    edit_area_->setCurrentWidget(real_mode_hint_);
    return;
  }
  buildNavTree();
}

void MockConfigEditor::loadTopologyAndResponses() {
  // 加载 MockResponses.emock
  QFile emockFile(file_path_);
  if (emockFile.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(emockFile.readAll());
    emockFile.close();
    if (doc.isObject()) {
      port_behaviors_ = doc.object()["portBehaviors"].toArray();
    }
  }

  // 加载同目录 topology.etopo
  QFileInfo fi(file_path_);
  QString topoPath = fi.absolutePath() + QStringLiteral("/topology.etopo");
  QFile topoFile(topoPath);
  if (topoFile.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(topoFile.readAll());
    topoFile.close();
    if (doc.isObject()) {
      topology_doc_ = doc.object();
    }
  }
}

void MockConfigEditor::buildNavTree() {
  nav_tree_->clear();

  QJsonArray products = topology_doc_["products"].toArray();
  QJsonArray devices = topology_doc_["devices"].toArray();
  QJsonArray connections = topology_doc_["connections"].toArray();

  // deviceName -> deviceObj
  QHash<QString, QJsonObject> deviceMap;
  for (const auto& devVal : devices) {
    QJsonObject dev = devVal.toObject();
    deviceMap.insert(dev["name"].toString(), dev);
  }

  for (const auto& prodVal : products) {
    QJsonObject product = prodVal.toObject();
    QString productName = product["name"].toString();

    auto* uutItem = new QTreeWidgetItem(nav_tree_);
    uutItem->setText(0, productName);
    uutItem->setData(0, Qt::UserRole, QStringLiteral("uut"));
    uutItem->setData(0, Qt::UserRole + 1, productName);

    QJsonArray ports = product["ports"].toArray();
    for (const auto& portVal : ports) {
      QJsonObject port = portVal.toObject();
      QString portName = port["name"].toString();

      // 找连接
      QJsonObject matchedConn;
      for (const auto& connVal : connections) {
        QJsonObject conn = connVal.toObject();
        if (conn["product"].toString() == productName &&
            conn["port"].toString() == portName) {
          matchedConn = conn;
          break;
        }
      }
      if (matchedConn.isEmpty()) {
        continue;
      }

      QString deviceName = matchedConn["device"].toString();
      QJsonObject device = deviceMap.value(deviceName);
      QString deviceType = device["deviceType"].toString();
      QString deviceId = device["id"].toString();
      QString devicePortName = matchedConn["devicePort"].toString();

      // 从设备端口提取 boundFrames
      QJsonArray boundFrames;
      QJsonArray devicePorts = device["ports"].toArray();
      for (const auto& dpVal : devicePorts) {
        QJsonObject dp = dpVal.toObject();
        if (dp["name"].toString() == devicePortName) {
          boundFrames = dp["boundFrames"].toArray();
          break;
        }
      }

      // 查 MockResponses 配置
      QJsonObject behavior;
      for (const auto& behVal : port_behaviors_) {
        QJsonObject b = behVal.toObject();
        if (b["productName"].toString() == productName &&
            b["deviceId"].toString() == deviceId &&
            b["port"].toString() == portName) {
          behavior = b;
          break;
        }
      }

      auto* portItem = new QTreeWidgetItem(uutItem);
      portItem->setData(0, Qt::UserRole, QStringLiteral("port"));
      portItem->setData(0, Qt::UserRole + 1, productName);
      portItem->setData(0, Qt::UserRole + 2, deviceId);
      portItem->setData(0, Qt::UserRole + 3, portName);
      portItem->setData(0, Qt::UserRole + 4, deviceType);
      portItem->setData(0, Qt::UserRole + 6, QVariant::fromValue(boundFrames));

      if (deviceType == QStringLiteral("serial") ||
          deviceType == QStringLiteral("can") ||
          deviceType == QStringLiteral("a429")) {
        // 帧型端口
        portItem->setText(0, QStringLiteral("%1 (%2)").arg(portName, deviceType));
        QJsonArray responses = behavior["responses"].toArray();
        for (const auto& respVal : responses) {
          QJsonObject resp = respVal.toObject();
          QString frameName = resp["frameName"].toString();
          QString replyFrameName = resp["replyFrameName"].toString();
          auto* respItem = new QTreeWidgetItem(portItem);
          if (frameName.isEmpty()) {
            respItem->setText(0, QStringLiteral("(无帧名) -> %1").arg(replyFrameName));
          } else {
            respItem->setText(0, QStringLiteral("%1 -> %2").arg(frameName, replyFrameName));
          }
          respItem->setData(0, Qt::UserRole, QStringLiteral("response"));
          respItem->setData(0, Qt::UserRole + 4, frameName);
          respItem->setData(0, Qt::UserRole + 5, replyFrameName);
        }
      } else if (deviceType == QStringLiteral("ad")) {
        // AD 通道型端口
        QString mode = behavior.contains("mode")
                            ? behavior["mode"].toString()
                            : QStringLiteral("fixed");
        QString summary;
        if (mode == "waveform") {
          summary = QStringLiteral("波形");
        } else if (mode == "series") {
          summary = QStringLiteral("序列");
        } else {
          double fv = behavior["fixedValue"].toDouble();
          summary = QStringLiteral("固定值: %1").arg(fv);
        }
        portItem->setText(0, QStringLiteral("%1 (%2, %3)").arg(portName, deviceType, summary));
      } else if (deviceType == QStringLiteral("da")) {
        // DA 通道型端口
        double fv = behavior["fixedValue"].toDouble();
        portItem->setText(0, QStringLiteral("%1 (%2, 固定值: %3)").arg(portName, deviceType).arg(fv));
      }
    }

    uutItem->setExpanded(true);
  }
}

void MockConfigEditor::onCurrentItemChanged(QTreeWidgetItem* current,
                                              QTreeWidgetItem* previous) {
  Q_UNUSED(previous);
  if (!current) {
    return;
  }
  // 加载数据时阻塞编辑回写信号
  const QSignalBlocker b1(*da_value_spin_);
  const QSignalBlocker b2(*ad_fixed_spin_);
  const QSignalBlocker b3(*ad_wf_amplitude_);
  const QSignalBlocker b4(*ad_wf_frequency_);
  const QSignalBlocker b5(*ad_wf_offset_);
  const QSignalBlocker b6(*ad_mode_combo_);
  const QSignalBlocker b7(*ad_series_table_);
  const QSignalBlocker b8(*fr_field_table_);
  const QSignalBlocker b9(*fr_reply_frame_);
  QString type = current->data(0, Qt::UserRole).toString();
  if (type == "port") {
    current_product_name_ = current->data(0, Qt::UserRole + 1).toString();
    current_device_id_ = current->data(0, Qt::UserRole + 2).toString();
    current_port_name_ = current->data(0, Qt::UserRole + 3).toString();
    QString deviceType = current->data(0, Qt::UserRole + 4).toString();
    if (deviceType == "da") {
      edit_area_->setCurrentWidget(da_port_page_);
      da_port_label_->setText(current->text(0));
      // 从 port_behaviors_ 加载 fixedValue
      QString productName = current->data(0, Qt::UserRole + 1).toString();
      QString deviceId = current->data(0, Qt::UserRole + 2).toString();
      QString portName = current->data(0, Qt::UserRole + 3).toString();
      for (const auto& behVal : port_behaviors_) {
        QJsonObject b = behVal.toObject();
        if (b["productName"].toString() == productName &&
            b["deviceId"].toString() == deviceId &&
            b["port"].toString() == portName) {
          da_value_spin_->setValue(b["fixedValue"].toDouble());
          break;
        }
      }
    } else if (deviceType == "ad") {
      edit_area_->setCurrentWidget(ad_port_page_);
      ad_port_label_->setText(current->text(0));
      QString productName = current->data(0, Qt::UserRole + 1).toString();
      QString deviceId = current->data(0, Qt::UserRole + 2).toString();
      QString portName = current->data(0, Qt::UserRole + 3).toString();
      for (const auto& behVal : port_behaviors_) {
        QJsonObject b = behVal.toObject();
        if (b["productName"].toString() == productName &&
            b["deviceId"].toString() == deviceId &&
            b["port"].toString() == portName) {
          if (b.contains("mode")) {
            QString mode = b["mode"].toString();
            if (mode == "fixed") {
              ad_mode_combo_->setCurrentIndex(0);
              ad_fixed_spin_->setValue(b["fixedValue"].toDouble());
            } else if (mode == "waveform") {
              ad_mode_combo_->setCurrentIndex(1);
              QJsonObject wf = b["waveform"].toObject();
              ad_wf_amplitude_->setValue(wf["amplitude"].toDouble());
              ad_wf_frequency_->setValue(wf["frequency"].toDouble());
              ad_wf_offset_->setValue(wf["offset"].toDouble());
              QString typeStr = wf["type"].toString();
              if (typeStr == "square") {
                ad_wf_type_->setCurrentIndex(1);
              } else if (typeStr == "triangle") {
                ad_wf_type_->setCurrentIndex(2);
              } else {
                ad_wf_type_->setCurrentIndex(0);
              }
            } else if (mode == "series") {
              ad_mode_combo_->setCurrentIndex(2);
              QJsonArray seriesArr = b["series"].toArray();
              ad_series_table_->setRowCount(seriesArr.size());
              for (int i = 0; i < seriesArr.size(); ++i) {
                ad_series_table_->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
                ad_series_table_->setItem(i, 1, new QTableWidgetItem(QString::number(seriesArr[i].toDouble())));
              }
            }
          } else {
            ad_mode_combo_->setCurrentIndex(0);
            ad_fixed_spin_->setValue(b["fixedValue"].toDouble());
          }
          break;
        }
      }
    }
    // blocker 释放后同步 stacked widget 到 combo 当前值
    ad_mode_stack_->setCurrentIndex(ad_mode_combo_->currentIndex());
  } else if (type == "response") {
    edit_area_->setCurrentWidget(frame_response_page_);
    QTreeWidgetItem* portItem = current->parent();
    if (!portItem) {
      return;
    }
    current_product_name_ = portItem->data(0, Qt::UserRole + 1).toString();
    current_device_id_ = portItem->data(0, Qt::UserRole + 2).toString();
    current_port_name_ = portItem->data(0, Qt::UserRole + 3).toString();
    current_frame_name_ = current->data(0, Qt::UserRole + 4).toString();
    current_reply_frame_name_ = current->data(0, Qt::UserRole + 5).toString();
    for (const auto& behVal : port_behaviors_) {
      QJsonObject b = behVal.toObject();
      if (b["productName"].toString() == current_product_name_ &&
          b["deviceId"].toString() == current_device_id_ &&
          b["port"].toString() == current_port_name_) {
        QJsonArray responses = b["responses"].toArray();
        for (const auto& respVal : responses) {
          QJsonObject resp = respVal.toObject();
          QString frameName = resp["frameName"].toString();
          QString replyFrameName = resp["replyFrameName"].toString();
          if (current->data(0, Qt::UserRole + 4).toString() == frameName &&
              current->data(0, Qt::UserRole + 5).toString() == replyFrameName) {
            fr_info_label_->setText(current->text(0));
            fr_reply_frame_->clear();
            QJsonArray boundFrames = portItem->data(0, Qt::UserRole + 6).value<QJsonArray>();
            if (!boundFrames.isEmpty()) {
              for (const auto& bfVal : boundFrames) {
                fr_reply_frame_->addItem(bfVal.toString());
              }
            } else if (icd_repo_) {
              for (const auto& frame : icd_repo_->frames()) {
                fr_reply_frame_->addItem(QString::fromStdString(std::string(frame->name())));
              }
            }
            fr_reply_frame_->setCurrentText(replyFrameName);
            QJsonArray fieldValues = resp["fieldValues"].toArray();
            fr_field_table_->setRowCount(fieldValues.size());
            for (int i = 0; i < fieldValues.size(); ++i) {
              QJsonObject fv = fieldValues[i].toObject();
              fr_field_table_->setItem(i, 0, new QTableWidgetItem(fv["nodePath"].toString()));
              fr_field_table_->setItem(i, 1, new QTableWidgetItem(QString::number(fv["engValue"].toDouble())));
              fr_field_table_->setItem(i, 2, new QTableWidgetItem(QString()));
              fr_field_table_->setItem(i, 3, new QTableWidgetItem(QString()));
            }
            updateHexPreview(replyFrameName);
            break;
          }
        }
        break;
      }
    }
  }
}

void MockConfigEditor::updateHexPreview(const QString& replyFrameName) {
  if (!signal_resolver_ || replyFrameName.isEmpty()) {
    fr_hex_preview_->setText(QString());
    return;
  }
  QByteArray frameBytes;
  for (int i = 0; i < fr_field_table_->rowCount(); ++i) {
    auto* pathItem = fr_field_table_->item(i, 0);
    auto* valueItem = fr_field_table_->item(i, 1);
    if (!pathItem || !valueItem) {
      continue;
    }
    QString nodePath = pathItem->text();
    double engValue = valueItem->text().toDouble();
    auto signal = signal_resolver_->buildFromIcd(replyFrameName, nodePath);
    if (!signal.valid) {
      continue;
    }
    QByteArray fieldBytes = signal_codec_.encodeToFrame(engValue, signal);
    if (frameBytes.size() < fieldBytes.size()) {
      int oldSize = frameBytes.size();
      frameBytes.resize(fieldBytes.size());
      for (int k = oldSize; k < frameBytes.size(); ++k) {
        frameBytes[k] = '\0';
      }
    }
    for (int j = 0; j < fieldBytes.size(); ++j) {
      frameBytes[j] = static_cast<char>(
          static_cast<unsigned char>(frameBytes[j]) |
          static_cast<unsigned char>(fieldBytes[j]));
    }
    if (auto* hexItem = fr_field_table_->item(i, 3)) {
      hexItem->setText(QString::fromLatin1(fieldBytes.toHex(' ')));
    }
  }
  fr_hex_preview_->setText(QString::fromLatin1(frameBytes.toHex(' ')));
}

int MockConfigEditor::findCurrentBehaviorIndex() const {
  for (int i = 0; i < port_behaviors_.size(); ++i) {
    QJsonObject b = port_behaviors_[i].toObject();
    if (b["productName"].toString() == current_product_name_ &&
        b["deviceId"].toString() == current_device_id_ &&
        b["port"].toString() == current_port_name_) {
      return i;
    }
  }
  return -1;
}

void MockConfigEditor::markModified() {
  if (!modified_) {
    modified_ = true;
    emit modificationChanged(true);
  }
}

void MockConfigEditor::onDaValueChanged() {
  int idx = findCurrentBehaviorIndex();
  if (idx < 0) {
    LOG_WARN("MOCK_CFG", "DA 编辑无对应 behavior 条目 [prod={} dev={} port={}]",
             current_product_name_.toStdString(),
             current_device_id_.toStdString(),
             current_port_name_.toStdString());
    return;
  }
  QJsonObject b = port_behaviors_[idx].toObject();
  b["fixedValue"] = da_value_spin_->value();
  port_behaviors_.replace(idx, b);
  markModified();
}

void MockConfigEditor::onAdValueChanged() {
  int idx = findCurrentBehaviorIndex();
  if (idx < 0) {
    LOG_WARN("MOCK_CFG", "AD 编辑无对应 behavior 条目 [prod={} dev={} port={}]",
             current_product_name_.toStdString(),
             current_device_id_.toStdString(),
             current_port_name_.toStdString());
    return;
  }
  QJsonObject b = port_behaviors_[idx].toObject();
  int mode = ad_mode_combo_->currentIndex();
  if (mode == 0) {
    b["mode"] = QStringLiteral("fixed");
    b["fixedValue"] = ad_fixed_spin_->value();
  } else if (mode == 1) {
    b["mode"] = QStringLiteral("waveform");
    QJsonObject wf;
    QString typeStr = ad_wf_type_->currentText();
    if (typeStr == QStringLiteral("方波")) {
      wf["type"] = QStringLiteral("square");
    } else if (typeStr == QStringLiteral("三角波")) {
      wf["type"] = QStringLiteral("triangle");
    } else {
      wf["type"] = QStringLiteral("sine");
    }
    wf["amplitude"] = ad_wf_amplitude_->value();
    wf["frequency"] = ad_wf_frequency_->value();
    wf["offset"] = ad_wf_offset_->value();
    b["waveform"] = wf;
  } else if (mode == 2) {
    b["mode"] = QStringLiteral("series");
    QJsonArray series;
    for (int i = 0; i < ad_series_table_->rowCount(); ++i) {
      auto* item = ad_series_table_->item(i, 1);
      if (item) {
        series.append(item->text().toDouble());
      }
    }
    b["series"] = series;
  }
  port_behaviors_.replace(idx, b);
  markModified();
}

void MockConfigEditor::onFrFieldChanged(QTableWidgetItem* item) {
  if (item->column() != 1) {
    return;
  }
  int idx = findCurrentBehaviorIndex();
  if (idx < 0) {
    LOG_WARN("MOCK_CFG", "帧响应编辑无对应 behavior 条目 [prod={} dev={} port={}]",
             current_product_name_.toStdString(),
             current_device_id_.toStdString(),
             current_port_name_.toStdString());
    return;
  }
  QJsonObject b = port_behaviors_[idx].toObject();
  QJsonArray responses = b["responses"].toArray();
  for (int i = 0; i < responses.size(); ++i) {
    QJsonObject resp = responses[i].toObject();
    if (resp["frameName"].toString() == current_frame_name_ &&
        resp["replyFrameName"].toString() == current_reply_frame_name_) {
      QJsonArray fieldValues = resp["fieldValues"].toArray();
      if (item->row() < fieldValues.size()) {
        // 更新现有行
        QJsonObject fv = fieldValues[item->row()].toObject();
        fv["engValue"] = item->text().toDouble();
        fieldValues.replace(item->row(), fv);
      } else {
        // 新增行：补齐 fieldValues 数组并创建新条目
        while (fieldValues.size() < item->row()) {
          fieldValues.append(QJsonObject());
        }
        QJsonObject fv;
        auto* pathItem = fr_field_table_->item(item->row(), 0);
        fv["nodePath"] = pathItem ? pathItem->text() : QString();
        fv["engValue"] = item->text().toDouble();
        fieldValues.append(fv);
      }
      resp["fieldValues"] = fieldValues;
      responses.replace(i, resp);
      break;
    }
  }
  b["responses"] = responses;
  port_behaviors_.replace(idx, b);
  markModified();
  updateHexPreview(current_reply_frame_name_);
}

bool MockConfigEditor::isRealMode() const {
  QJsonArray devices = topology_doc_["devices"].toArray();
  if (devices.isEmpty()) {
    return false;
  }
  for (const auto& devVal : devices) {
    QJsonObject dev = devVal.toObject();
    QString pluginId = dev["pluginId"].toString();
    if (pluginId.isEmpty()) {
      continue;
    }
    auto* plugin = etest::core::plugin::PluginManager::instance().plugin(pluginId);
    if (!plugin) {
      return false;  // 插件未加载，保守地认为非真实模式
    }
    if (plugin->metaData().is_mock) {
      return false;
    }
  }
  return true;
}

void MockConfigEditor::onReplyFrameChanged(const QString& text) {
  int idx = findCurrentBehaviorIndex();
  if (idx < 0) {
    return;
  }
  QJsonObject b = port_behaviors_[idx].toObject();
  QJsonArray responses = b["responses"].toArray();
  for (int i = 0; i < responses.size(); ++i) {
    QJsonObject resp = responses[i].toObject();
    if (resp["frameName"].toString() == current_frame_name_ &&
        resp["replyFrameName"].toString() == current_reply_frame_name_) {
      resp["replyFrameName"] = text;
      current_reply_frame_name_ = text;
      responses.replace(i, resp);
      break;
    }
  }
  b["responses"] = responses;
  port_behaviors_.replace(idx, b);
  markModified();
  updateHexPreview(text);
}

void MockConfigEditor::onFrAddRow() {
  int row = fr_field_table_->rowCount();
  fr_field_table_->insertRow(row);
  fr_field_table_->setItem(row, 0, new QTableWidgetItem(QString()));
  fr_field_table_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0")));
  fr_field_table_->setItem(row, 2, new QTableWidgetItem(QString()));
  fr_field_table_->setItem(row, 3, new QTableWidgetItem(QString()));
  markModified();
}

void MockConfigEditor::onFrDeleteRow() {
  int row = fr_field_table_->currentRow();
  if (row < 0) {
    return;
  }
  fr_field_table_->removeRow(row);
  // 同步从 JSON 中移除
  int idx = findCurrentBehaviorIndex();
  if (idx < 0) {
    return;
  }
  QJsonObject b = port_behaviors_[idx].toObject();
  QJsonArray responses = b["responses"].toArray();
  for (int i = 0; i < responses.size(); ++i) {
    QJsonObject resp = responses[i].toObject();
    if (resp["frameName"].toString() == current_frame_name_ &&
        resp["replyFrameName"].toString() == current_reply_frame_name_) {
      QJsonArray fieldValues = resp["fieldValues"].toArray();
      if (row < fieldValues.size()) {
        fieldValues.removeAt(row);
      }
      resp["fieldValues"] = fieldValues;
      responses.replace(i, resp);
      break;
    }
  }
  b["responses"] = responses;
  port_behaviors_.replace(idx, b);
  markModified();
  updateHexPreview(current_reply_frame_name_);
}

}  // namespace etest::app
