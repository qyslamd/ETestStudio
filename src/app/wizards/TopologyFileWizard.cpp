#include "TopologyFileWizard.h"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <utility>

#include "AppIconProvider.h"
#include "WizardTemplateCard.h"
#include "logger/Logger.h"

#include "topology/TopologyDocument.h"

namespace etest::app {

using etest::core_ui::AppIconProvider;

namespace {

// ── 简易流式布局：端口 chip 超出宽度时自动换行 ──
class FlowLayout : public QLayout {
 public:
  // 注意：作为子布局加入其它布局时不能带 widget 父对象创建（否则会触发
  // widget->setLayout 覆盖父布局），统一无父创建后由 addLayout 归属。
  explicit FlowLayout(QWidget* parent = nullptr) : QLayout(parent) {
    setContentsMargins(0, 0, 0, 0);
    setSpacing(6);
  }
  ~FlowLayout() override {
    while (QLayoutItem* item = takeAt(0)) {
      delete item;
    }
  }

  void addItem(QLayoutItem* item) override { items_.append(item); }
  int count() const override { return items_.size(); }
  QLayoutItem* itemAt(int index) const override { return items_.value(index); }
  QLayoutItem* takeAt(int index) override {
    if (index < 0 || index >= items_.size()) {
      return nullptr;
    }
    return items_.takeAt(index);
  }
  QSize sizeHint() const override { return calculateSize(SizeHint); }
  QSize minimumSize() const override { return calculateSize(MinimumSize); }
  void setGeometry(const QRect& rect) override {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
  }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override {
    return doLayout(QRect(0, 0, width, 0), true);
  }

 private:
  enum SizeType { MinimumSize, SizeHint };

  QSize calculateSize(SizeType type) const {
    QSize size;
    for (const QLayoutItem* item : items_) {
      const QSize s =
          (type == MinimumSize) ? item->minimumSize() : item->sizeHint();
      size = size.expandedTo(s);
    }
    const QMargins m = contentsMargins();
    size += QSize(m.left() + m.right(), m.top() + m.bottom());
    return size;
  }

  int doLayout(const QRect& rect, bool testOnly) const {
    const int left = contentsMargins().left();
    const int top = contentsMargins().top();
    const int right = contentsMargins().right();
    const int bottom = contentsMargins().bottom();
    QRect effective = rect.adjusted(left, top, -right, -bottom);
    int x = effective.x();
    int y = effective.y();
    int lineHeight = 0;
    for (QLayoutItem* item : items_) {
      const int spaceX = spacing();
      const int spaceY = spacing();
      const int w = item->sizeHint().width();
      const int h = item->sizeHint().height();
      if (x + w > effective.right() + 1 && lineHeight > 0) {
        x = effective.x();
        y = y + lineHeight + spaceY;
        lineHeight = 0;
      }
      if (!testOnly) {
        item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
      }
      x += w + spaceX;
      lineHeight = qMax(lineHeight, h);
    }
    return y + lineHeight - rect.y() + bottom;
  }

  QList<QLayoutItem*> items_;
};

// ── 内置 5 类 mock 设备模板（设计决策 2）──
struct DeviceTemplate {
  QString type;
  QString displayName;
  QString pluginId;
  int channels;
};

const DeviceTemplate kDeviceTemplates[] = {
    {QStringLiteral("can"), QStringLiteral("PXI-CAN卡"),
     QStringLiteral("etest.plugin.device.mock_can"), 2},
    {QStringLiteral("a429"), QStringLiteral("PXI-A429卡"),
     QStringLiteral("etest.plugin.device.mock_a429"), 2},
    {QStringLiteral("ad"), QStringLiteral("PXI-AD采集卡"),
     QStringLiteral("etest.plugin.device.mock_ad"), 8},
    {QStringLiteral("serial"), QStringLiteral("PXI-串口卡"),
     QStringLiteral("etest.plugin.device.mock_serial"), 1},
    {QStringLiteral("da"), QStringLiteral("PXI-DA输出卡"),
     QStringLiteral("etest.plugin.device.mock_da"), 4},
};

const DeviceTemplate* deviceTemplate(const QString& type) {
  for (const DeviceTemplate& t : kDeviceTemplates) {
    if (t.type == type) {
      return &t;
    }
  }
  return &kDeviceTemplates[0];
}

// FunctionType 枚举无 CAN（决策 7 澄清：CAN 端口 functionType=CUSTOM，
// 兼容性完全依赖 allowedDeviceTypes 的 deviceType 匹配）。
etest::topology::FunctionType functionTypeFor(const QString& deviceType) {
  using etest::topology::FunctionType;
  if (deviceType == QStringLiteral("a429")) {
    return FunctionType::A429;
  }
  if (deviceType == QStringLiteral("ad")) {
    return FunctionType::AD;
  }
  if (deviceType == QStringLiteral("da")) {
    return FunctionType::DA;
  }
  if (deviceType == QStringLiteral("serial")) {
    return FunctionType::SERIAL;
  }
  return FunctionType::CUSTOM;
}

bool isIllegalName(const QString& text) {
  const QString illegal = QStringLiteral("\\/:*?\"<>|");
  for (const QChar& c : text) {
    if (illegal.contains(c)) {
      return true;
    }
  }
  return false;
}

QString deviceTypeOf(const TopologyData& data, const QString& name) {
  for (const WizardDevice& d : data.devices) {
    if (d.name == name) {
      return d.deviceType;
    }
  }
  return QString();
}

bool uutExists(const TopologyData& data, const QString& name) {
  for (const WizardUut& u : data.uuts) {
    if (u.name == name) {
      return true;
    }
  }
  return false;
}

bool deviceExists(const TopologyData& data, const QString& name) {
  for (const WizardDevice& d : data.devices) {
    if (d.name == name) {
      return true;
    }
  }
  return false;
}

// 依据当前连线集合重算每个 UUT 端口的 allowedDeviceTypes（决策 7：仅含
// 实际连线设备的 deviceType），保证增删连线后端口属性与连线一致。
void reconcileUutPortAllowedTypes(TopologyData& data) {
  for (WizardUut& u : data.uuts) {
    for (int i = 0; i < u.ports.size(); ++i) {
      QStringList types;
      for (const WizardConnection& c : data.connections) {
        if (c.uutName == u.name && c.uutPort == u.ports[i]) {
          const QString t = deviceTypeOf(data, c.deviceName);
          if (!t.isEmpty() && !types.contains(t)) {
            types.append(t);
          }
        }
      }
      u.portAllowedTypes[i] = types;
    }
  }
}

// 取第一个未占用的 pN 作为新增端口名，保证同实体内端口名唯一（决策 6）
QString nextPortName(const QStringList& existing) {
  int n = 0;
  while (existing.contains(QStringLiteral("p%1").arg(n))) {
    ++n;
  }
  return QStringLiteral("p%1").arg(n);
}

// ── 模板预置辅助 ──
void addDeviceToData(TopologyData& data,
                     const DeviceTemplate& tpl,
                     const QString& name) {
  WizardDevice d;
  d.name = name;
  d.deviceType = tpl.type;
  d.pluginId = tpl.pluginId;
  for (int i = 0; i < tpl.channels; ++i) {
    d.ports.append(QStringLiteral("ch%1").arg(i));
  }
  data.devices.append(d);
}

void addUutToData(TopologyData& data,
                  const QString& name,
                  const QStringList& ports) {
  WizardUut u;
  u.name = name;
  u.ports = ports;
  for (int i = 0; i < ports.size(); ++i) {
    u.portAllowedTypes.append(QStringList());
  }
  data.uuts.append(u);
}

void addConnectionToData(TopologyData& data,
                         const QString& deviceName,
                         const QString& devicePort,
                         const QString& uutName,
                         const QString& uutPort) {
  WizardConnection c;
  c.deviceName = deviceName;
  c.devicePort = devicePort;
  c.uutName = uutName;
  c.uutPort = uutPort;
  data.connections.append(c);
}

}  // namespace

// ── 模板页 ──

class TopologyFileWizard::TemplatePage : public WizardPage {
 public:
  explicit TemplatePage(QWidget* parent = nullptr);

  QString name() const;
  QString selectedTemplateId() const;
  /// 程序化恢复勾选（loadTemplate 用，不触发 completeChanged）
  void setTemplateId(const QString& templateId);
  QString stepLabel() const override { return QStringLiteral("模板"); }
  bool isComplete() const override;
  bool validatePage() override;

 private:
  void initUi();
  void initSignals();
  void onNameChanged();
  WizardTemplateCard* addCard(QButtonGroup* group,
                              QHBoxLayout* layout,
                              const QString& templateId,
                              const QString& iconName,
                              const QString& title,
                              const QString& desc,
                              const QString& badge);

  QLineEdit* name_edit_ = nullptr;
  QLabel* name_hint_ = nullptr;
  QButtonGroup* group_ = nullptr;
};

TopologyFileWizard::TemplatePage::TemplatePage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

QString TopologyFileWizard::TemplatePage::name() const {
  return name_edit_->text().trimmed();
}

QString TopologyFileWizard::TemplatePage::selectedTemplateId() const {
  QAbstractButton* checked = group_ ? group_->checkedButton() : nullptr;
  if (!checked) {
    return QString();
  }
  return checked->property("templateId").toString();
}

void TopologyFileWizard::TemplatePage::setTemplateId(
    const QString& templateId) {
  if (!group_) {
    return;
  }
  for (QAbstractButton* btn : group_->buttons()) {
    if (btn->property("templateId").toString() == templateId) {
      btn->setChecked(true);
      return;
    }
  }
}

bool TopologyFileWizard::TemplatePage::isComplete() const {
  return !name().isEmpty() && !isIllegalName(name());
}

bool TopologyFileWizard::TemplatePage::validatePage() {
  if (!isComplete()) {
    QMessageBox::information(this, QStringLiteral("无法继续"),
                             QStringLiteral("请填写有效的拓扑名称。"));
    return false;
  }
  return true;
}

void TopologyFileWizard::TemplatePage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro = new QLabel(
      QStringLiteral("选择拓扑模板，快速搭建测试系统的硬件连接骨架。"), this);
  intro->setObjectName(QStringLiteral("topoIntro"));
  layout->addWidget(intro);

  // 拓扑名称（设计决策 3）
  auto* nameRow = new QHBoxLayout();
  nameRow->setSpacing(10);
  auto* nameLabel = new QLabel(QStringLiteral("拓扑名称"), this);
  nameLabel->setObjectName(QStringLiteral("topoNameLabel"));
  nameRow->addWidget(nameLabel);
  name_edit_ = new QLineEdit(QStringLiteral("新建拓扑文件"), this);
  name_edit_->setObjectName(QStringLiteral("topoNameEdit"));
  name_edit_->setClearButtonEnabled(true);
  nameRow->addWidget(name_edit_, 1);
  name_hint_ =
      new QLabel(QStringLiteral("名称不能包含 \\ / : * ? \" < > | 字符"), this);
  name_hint_->setObjectName(QStringLiteral("topoNameHint"));
  name_hint_->hide();
  nameRow->addWidget(name_hint_);
  layout->addLayout(nameRow);

  // 模板卡片
  group_ = new QButtonGroup(this);
  group_->setExclusive(true);
  auto* cards = new QHBoxLayout();
  cards->setSpacing(14);
  auto* emptyCard =
      addCard(group_, cards, QStringLiteral("empty"),
              QStringLiteral("topo_tap"), QStringLiteral("空拓扑"),
              QStringLiteral("从零开始添加设备和 UUT"), QStringLiteral("推荐"));
  addCard(group_, cards, QStringLiteral("single"),
          QStringLiteral("topo_device"), QStringLiteral("单设备 + UUT"),
          QStringLiteral("一个测试设备连接一个被测对象"), QString());
  addCard(group_, cards, QStringLiteral("multi"), QStringLiteral("topology"),
          QStringLiteral("多设备混合"),
          QStringLiteral("AD/DA/CAN/串口 多种设备混合拓扑"), QString());
  layout->addLayout(cards);
  layout->addStretch();

  emptyCard->setChecked(true);
}

void TopologyFileWizard::TemplatePage::initSignals() {
  connect(name_edit_, &QLineEdit::textChanged, this,
          &TemplatePage::onNameChanged);
}

WizardTemplateCard* TopologyFileWizard::TemplatePage::addCard(
    QButtonGroup* group,
    QHBoxLayout* layout,
    const QString& templateId,
    const QString& iconName,
    const QString& title,
    const QString& desc,
    const QString& badge) {
  auto* card = new WizardTemplateCard(iconName, title, desc, badge, this);
  card->setProperty("templateId", templateId);
  group->addButton(card);
  layout->addWidget(card);
  connect(card, &QAbstractButton::clicked, this,
          [this](bool) { emit completeChanged(); });
  return card;
}

void TopologyFileWizard::TemplatePage::onNameChanged() {
  const bool valid = isComplete();
  name_hint_->setVisible(!valid);
  name_edit_->setProperty("error", !valid);
  name_edit_->style()->unpolish(name_edit_);
  name_edit_->style()->polish(name_edit_);
  emit completeChanged();
}

// ── 设备 & UUT 页 ──

class TopologyFileWizard::DeviceUutPage : public WizardPage {
 public:
  explicit DeviceUutPage(TopologyData* data, QWidget* parent = nullptr);

  QString stepLabel() const override { return QStringLiteral("设备 & UUT"); }
  bool validatePage() override;
  /// 数据变化后重建列表（loadTemplate 后调用；同时复位自增序号）
  void reset();
  /// 数据变化后重建列表（增删端口/设备后调用）
  void rebuild();
  /// 数据变化回调（由向导注入，用于联动连线页/摘要）
  void setChangedCallback(std::function<void()> callback);

 private:
  void initUi();
  void initSignals();
  void notifyChanged();
  void addDevice(const DeviceTemplate& tpl);
  void addUut();
  void removeDevice(int index);
  void removeUut(int index);
  void addDevicePort(int index);
  void addUutPort(int index);
  void removeDevicePort(int index, int portIndex);
  void removeUutPort(int index, int portIndex);
  void clearDevices();
  void clearUuts();
  QWidget* makeDeviceItem(int index);
  QWidget* makeUutItem(int index);
  QToolButton* makeSmallButton(const QString& iconName,
                               std::function<void()> handler,
                               QWidget* parent);
  QWidget* makePortChip(const QString& portName,
                        std::function<void()> onRemove,
                        QWidget* parent);

  std::function<void()> changed_cb_;
  TopologyData* data_ = nullptr;
  int device_seq_ = 0;
  int uut_seq_ = 0;

  QVBoxLayout* device_list_layout_ = nullptr;
  QVBoxLayout* uut_list_layout_ = nullptr;
  QLabel* device_count_ = nullptr;
  QLabel* uut_count_ = nullptr;
  QLabel* device_empty_ = nullptr;
  QLabel* uut_empty_ = nullptr;
};

TopologyFileWizard::DeviceUutPage::DeviceUutPage(TopologyData* data,
                                                 QWidget* parent)
    : WizardPage(parent), data_(data) {
  initUi();
  initSignals();
}

bool TopologyFileWizard::DeviceUutPage::validatePage() {
  if (data_->devices.isEmpty() || data_->uuts.isEmpty()) {
    QMessageBox::information(
        this, QStringLiteral("无法继续"),
        QStringLiteral("请至少添加一个测试设备和一个被测对象（UUT）。"));
    return false;
  }
  return true;
}

void TopologyFileWizard::DeviceUutPage::reset() {
  device_seq_ = data_->devices.size();
  uut_seq_ = data_->uuts.size();
  rebuild();
}

void TopologyFileWizard::DeviceUutPage::setChangedCallback(
    std::function<void()> callback) {
  changed_cb_ = std::move(callback);
}

void TopologyFileWizard::DeviceUutPage::notifyChanged() {
  if (changed_cb_) {
    changed_cb_();
  }
}

void TopologyFileWizard::DeviceUutPage::rebuild() {
  auto clearLayout = [](QVBoxLayout* layout, QWidget* keepAlive) {
    while (QLayoutItem* item = layout->takeAt(0)) {
      if (QWidget* w = item->widget()) {
        if (w != keepAlive) {
          w->deleteLater();
        }
      }
      delete item;
    }
  };
  clearLayout(device_list_layout_, device_empty_);
  clearLayout(uut_list_layout_, uut_empty_);

  // 空态提示常驻列表首项（不被 clearLayout 销毁），再重建实体卡片
  device_list_layout_->addWidget(device_empty_);
  uut_list_layout_->addWidget(uut_empty_);

  for (int i = 0; i < data_->devices.size(); ++i) {
    device_list_layout_->addWidget(makeDeviceItem(i));
  }
  device_list_layout_->addStretch();
  for (int i = 0; i < data_->uuts.size(); ++i) {
    uut_list_layout_->addWidget(makeUutItem(i));
  }
  uut_list_layout_->addStretch();

  device_count_->setText(QString::number(data_->devices.size()));
  uut_count_->setText(QString::number(data_->uuts.size()));
  device_empty_->setVisible(data_->devices.isEmpty());
  uut_empty_->setVisible(data_->uuts.isEmpty());
}

void TopologyFileWizard::DeviceUutPage::addDevice(const DeviceTemplate& tpl) {
  ++device_seq_;
  addDeviceToData(*data_, tpl,
                  tpl.displayName + QStringLiteral("_%1").arg(device_seq_));
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::addUut() {
  ++uut_seq_;
  addUutToData(*data_, QStringLiteral("UUT_%1").arg(uut_seq_),
               {QStringLiteral("p0")});
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::removeDevice(int index) {
  if (index < 0 || index >= data_->devices.size()) {
    return;
  }
  const QString name = data_->devices[index].name;
  data_->devices.removeAt(index);
  QList<WizardConnection> kept;
  for (const WizardConnection& c : data_->connections) {
    if (c.deviceName != name) {
      kept.append(c);
    }
  }
  data_->connections = kept;
  reconcileUutPortAllowedTypes(*data_);
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::removeUut(int index) {
  if (index < 0 || index >= data_->uuts.size()) {
    return;
  }
  const QString name = data_->uuts[index].name;
  data_->uuts.removeAt(index);
  QList<WizardConnection> kept;
  for (const WizardConnection& c : data_->connections) {
    if (c.uutName != name) {
      kept.append(c);
    }
  }
  data_->connections = kept;
  reconcileUutPortAllowedTypes(*data_);
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::addDevicePort(int index) {
  if (index < 0 || index >= data_->devices.size()) {
    return;
  }
  WizardDevice& d = data_->devices[index];
  d.ports.append(nextPortName(d.ports));
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::addUutPort(int index) {
  if (index < 0 || index >= data_->uuts.size()) {
    return;
  }
  WizardUut& u = data_->uuts[index];
  u.ports.append(nextPortName(u.ports));
  u.portAllowedTypes.append(QStringList());
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::removeDevicePort(int index,
                                                         int portIndex) {
  if (index < 0 || index >= data_->devices.size()) {
    return;
  }
  WizardDevice& d = data_->devices[index];
  if (portIndex < 0 || portIndex >= d.ports.size()) {
    return;
  }
  const QString portName = d.ports[portIndex];
  d.ports.removeAt(portIndex);
  QList<WizardConnection> kept;
  for (const WizardConnection& c : data_->connections) {
    if (!(c.deviceName == d.name && c.devicePort == portName)) {
      kept.append(c);
    }
  }
  data_->connections = kept;
  reconcileUutPortAllowedTypes(*data_);
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::removeUutPort(int index,
                                                      int portIndex) {
  if (index < 0 || index >= data_->uuts.size()) {
    return;
  }
  WizardUut& u = data_->uuts[index];
  if (portIndex < 0 || portIndex >= u.ports.size()) {
    return;
  }
  const QString portName = u.ports[portIndex];
  u.ports.removeAt(portIndex);
  u.portAllowedTypes.removeAt(portIndex);
  QList<WizardConnection> kept;
  for (const WizardConnection& c : data_->connections) {
    if (!(c.uutName == u.name && c.uutPort == portName)) {
      kept.append(c);
    }
  }
  data_->connections = kept;
  reconcileUutPortAllowedTypes(*data_);
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::clearDevices() {
  if (data_->devices.isEmpty()) {
    return;
  }
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("清空设备"),
      QStringLiteral("确定要清空所有设备吗？关联的连线也会被删除。"));
  if (reply != QMessageBox::Yes) {
    return;
  }
  data_->devices.clear();
  QList<WizardConnection> kept;
  for (const WizardConnection& c : data_->connections) {
    if (uutExists(*data_, c.uutName)) {
      kept.append(c);
    }
  }
  data_->connections = kept;
  reconcileUutPortAllowedTypes(*data_);
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::clearUuts() {
  if (data_->uuts.isEmpty()) {
    return;
  }
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("清空 UUT"),
      QStringLiteral("确定要清空所有 UUT 吗？关联的连线也会被删除。"));
  if (reply != QMessageBox::Yes) {
    return;
  }
  data_->uuts.clear();
  QList<WizardConnection> kept;
  for (const WizardConnection& c : data_->connections) {
    if (deviceExists(*data_, c.deviceName)) {
      kept.append(c);
    }
  }
  data_->connections = kept;
  reconcileUutPortAllowedTypes(*data_);
  rebuild();
  notifyChanged();
}

void TopologyFileWizard::DeviceUutPage::initUi() {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(20);

  // ── 左：设备池 ──
  auto* devPool = new QFrame(this);
  devPool->setObjectName(QStringLiteral("topoPool"));
  auto* devLay = new QVBoxLayout(devPool);
  devLay->setContentsMargins(14, 14, 14, 12);
  devLay->setSpacing(10);

  auto* devHead = new QHBoxLayout();
  auto* devTitle = new QLabel(QStringLiteral("测试设备"), devPool);
  devTitle->setObjectName(QStringLiteral("topoPoolTitle"));
  devHead->addWidget(devTitle);
  devHead->addStretch();
  device_count_ = new QLabel(QStringLiteral("0"), devPool);
  device_count_->setObjectName(QStringLiteral("topoCountBadge"));
  devHead->addWidget(device_count_);
  devLay->addLayout(devHead);

  auto* devScroll = new QScrollArea(devPool);
  devScroll->setWidgetResizable(true);
  devScroll->setFrameShape(QFrame::NoFrame);
  auto* devContent = new QWidget(devScroll);
  auto* devListLay = new QVBoxLayout(devContent);
  devListLay->setContentsMargins(0, 0, 0, 0);
  devListLay->setSpacing(8);
  device_list_layout_ = devListLay;
  device_empty_ =
      new QLabel(QStringLiteral("点击下方按钮添加设备"), devContent);
  device_empty_->setObjectName(QStringLiteral("topoEmptyHint"));
  device_empty_->setAlignment(Qt::AlignCenter);
  devListLay->addWidget(device_empty_);
  devListLay->addStretch();
  devScroll->setWidget(devContent);
  devLay->addWidget(devScroll, 1);

  auto* devFooter = new QHBoxLayout();
  auto* addDeviceBtn = new QToolButton(devPool);
  addDeviceBtn->setObjectName(QStringLiteral("topoAddDeviceBtn"));
  addDeviceBtn->setText(QStringLiteral("添加设备"));
  addDeviceBtn->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("microchip")));
  addDeviceBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  addDeviceBtn->setPopupMode(QToolButton::InstantPopup);
  auto* menu = new QMenu(addDeviceBtn);
  for (const DeviceTemplate& t : kDeviceTemplates) {
    QAction* action = menu->addAction(t.displayName + QStringLiteral("  (") +
                                      t.type + QStringLiteral(")"));
    connect(action, &QAction::triggered, this, [this, t]() { addDevice(t); });
  }
  addDeviceBtn->setMenu(menu);
  devFooter->addWidget(addDeviceBtn);
  auto* clearDevBtn = new QToolButton(devPool);
  clearDevBtn->setObjectName(QStringLiteral("topoDangerBtn"));
  clearDevBtn->setText(QStringLiteral("清空"));
  clearDevBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(clearDevBtn, &QToolButton::clicked, this,
          [this](bool) { clearDevices(); });
  devFooter->addWidget(clearDevBtn);
  devFooter->addStretch();
  devLay->addLayout(devFooter);

  layout->addWidget(devPool, 1);

  // ── 右：UUT 池 ──
  auto* uutPool = new QFrame(this);
  uutPool->setObjectName(QStringLiteral("topoPool"));
  auto* uutLay = new QVBoxLayout(uutPool);
  uutLay->setContentsMargins(14, 14, 14, 12);
  uutLay->setSpacing(10);

  auto* uutHead = new QHBoxLayout();
  auto* uutTitle = new QLabel(QStringLiteral("被测对象 (UUT)"), uutPool);
  uutTitle->setObjectName(QStringLiteral("topoPoolTitle"));
  uutHead->addWidget(uutTitle);
  uutHead->addStretch();
  uut_count_ = new QLabel(QStringLiteral("0"), uutPool);
  uut_count_->setObjectName(QStringLiteral("topoCountBadge"));
  uutHead->addWidget(uut_count_);
  uutLay->addLayout(uutHead);

  auto* uutScroll = new QScrollArea(uutPool);
  uutScroll->setWidgetResizable(true);
  uutScroll->setFrameShape(QFrame::NoFrame);
  auto* uutContent = new QWidget(uutScroll);
  auto* uutListLay = new QVBoxLayout(uutContent);
  uutListLay->setContentsMargins(0, 0, 0, 0);
  uutListLay->setSpacing(8);
  uut_list_layout_ = uutListLay;
  uut_empty_ = new QLabel(QStringLiteral("点击下方按钮添加 UUT"), uutContent);
  uut_empty_->setObjectName(QStringLiteral("topoEmptyHint"));
  uut_empty_->setAlignment(Qt::AlignCenter);
  uutListLay->addWidget(uut_empty_);
  uutListLay->addStretch();
  uutScroll->setWidget(uutContent);
  uutLay->addWidget(uutScroll, 1);

  auto* uutFooter = new QHBoxLayout();
  auto* addUutBtn = new QToolButton(uutPool);
  addUutBtn->setObjectName(QStringLiteral("topoAddDeviceBtn"));
  addUutBtn->setText(QStringLiteral("添加 UUT"));
  addUutBtn->setIcon(AppIconProvider::instance().icon(QStringLiteral("plus")));
  addUutBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  connect(addUutBtn, &QToolButton::clicked, this, [this](bool) { addUut(); });
  uutFooter->addWidget(addUutBtn);
  auto* clearUutBtn = new QToolButton(uutPool);
  clearUutBtn->setObjectName(QStringLiteral("topoDangerBtn"));
  clearUutBtn->setText(QStringLiteral("清空"));
  clearUutBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(clearUutBtn, &QToolButton::clicked, this,
          [this](bool) { clearUuts(); });
  uutFooter->addWidget(clearUutBtn);
  uutFooter->addStretch();
  uutLay->addLayout(uutFooter);

  layout->addWidget(uutPool, 1);
}

void TopologyFileWizard::DeviceUutPage::initSignals() {}

QWidget* TopologyFileWizard::DeviceUutPage::makeDeviceItem(int index) {
  const WizardDevice& d = data_->devices[index];
  auto* card = new QFrame(this);
  card->setObjectName(QStringLiteral("topoItem"));
  card->setFrameShape(QFrame::NoFrame);
  auto* lay = new QVBoxLayout(card);
  lay->setContentsMargins(10, 8, 10, 8);
  lay->setSpacing(6);

  auto* head = new QHBoxLayout();
  head->setSpacing(8);
  auto* icon = new QLabel(card);
  icon->setPixmap(AppIconProvider::instance()
                      .icon(QStringLiteral("microchip"))
                      .pixmap(16, 16));
  head->addWidget(icon);
  auto* name = new QLabel(d.name, card);
  name->setObjectName(QStringLiteral("topoItemName"));
  name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  head->addWidget(name, 1);
  auto* typeTag = new QLabel(d.deviceType, card);
  typeTag->setObjectName(QStringLiteral("topoPortsTag"));
  head->addWidget(typeTag);
  auto* countTag =
      new QLabel(QStringLiteral("%1端口").arg(d.ports.size()), card);
  countTag->setObjectName(QStringLiteral("topoPortsTag"));
  head->addWidget(countTag);
  head->addWidget(makeSmallButton(
      QStringLiteral("plus"), [this, index]() { addDevicePort(index); }, card));
  head->addWidget(makeSmallButton(
      QStringLiteral("trash"), [this, index]() { removeDevice(index); }, card));
  lay->addLayout(head);

  auto* portsLay = new FlowLayout();
  for (int p = 0; p < d.ports.size(); ++p) {
    portsLay->addWidget(makePortChip(
        d.ports[p], [this, index, p]() { removeDevicePort(index, p); }, card));
  }
  lay->addLayout(portsLay);
  return card;
}

QWidget* TopologyFileWizard::DeviceUutPage::makeUutItem(int index) {
  const WizardUut& u = data_->uuts[index];
  auto* card = new QFrame(this);
  card->setObjectName(QStringLiteral("topoItem"));
  card->setFrameShape(QFrame::NoFrame);
  auto* lay = new QVBoxLayout(card);
  lay->setContentsMargins(10, 8, 10, 8);
  lay->setSpacing(6);

  auto* head = new QHBoxLayout();
  head->setSpacing(8);
  auto* icon = new QLabel(card);
  icon->setPixmap(AppIconProvider::instance()
                      .icon(QStringLiteral("topo_uut"))
                      .pixmap(16, 16));
  head->addWidget(icon);
  auto* name = new QLabel(u.name, card);
  name->setObjectName(QStringLiteral("topoItemName"));
  name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  head->addWidget(name, 1);
  auto* countTag =
      new QLabel(QStringLiteral("%1端口").arg(u.ports.size()), card);
  countTag->setObjectName(QStringLiteral("topoPortsTag"));
  head->addWidget(countTag);
  head->addWidget(makeSmallButton(
      QStringLiteral("plus"), [this, index]() { addUutPort(index); }, card));
  head->addWidget(makeSmallButton(
      QStringLiteral("trash"), [this, index]() { removeUut(index); }, card));
  lay->addLayout(head);

  auto* portsLay = new FlowLayout();
  for (int p = 0; p < u.ports.size(); ++p) {
    portsLay->addWidget(makePortChip(
        u.ports[p], [this, index, p]() { removeUutPort(index, p); }, card));
  }
  lay->addLayout(portsLay);
  return card;
}

QToolButton* TopologyFileWizard::DeviceUutPage::makeSmallButton(
    const QString& iconName,
    std::function<void()> handler,
    QWidget* parent) {
  auto* btn = new QToolButton(parent);
  btn->setObjectName(QStringLiteral("topoItemBtn"));
  btn->setProperty("danger", iconName == QStringLiteral("trash"));
  btn->setIcon(AppIconProvider::instance().icon(iconName));
  btn->setIconSize(QSize(14, 14));
  btn->setCursor(Qt::PointingHandCursor);
  connect(btn, &QToolButton::clicked, this, [handler](bool) { handler(); });
  return btn;
}

QWidget* TopologyFileWizard::DeviceUutPage::makePortChip(
    const QString& portName,
    std::function<void()> onRemove,
    QWidget* parent) {
  auto* chip = new QFrame(parent);
  chip->setObjectName(QStringLiteral("topoPortChip"));
  chip->setFrameShape(QFrame::NoFrame);
  auto* lay = new QHBoxLayout(chip);
  lay->setContentsMargins(6, 2, 2, 2);
  lay->setSpacing(2);
  auto* label = new QLabel(portName, chip);
  label->setObjectName(QStringLiteral("topoPortChipName"));
  lay->addWidget(label);
  auto* del = new QToolButton(chip);
  del->setObjectName(QStringLiteral("topoPortChipDel"));
  del->setIcon(AppIconProvider::instance().icon("close"));
  del->setFixedSize(14, 14);
  del->setCursor(Qt::PointingHandCursor);
  connect(del, &QToolButton::clicked, this, [onRemove](bool) { onRemove(); });
  lay->addWidget(del);
  return chip;
}

// ── 连线页 ──

class TopologyFileWizard::ConnectionPage : public WizardPage {
 public:
  explicit ConnectionPage(TopologyData* data, QWidget* parent = nullptr);

  QString stepLabel() const override { return QStringLiteral("连线"); }
  bool validatePage() override;
  /// 数据/页面变化后刷新组合框与列表
  void refresh();
  /// 数据变化回调（由向导注入，用于联动摘要）
  void setChangedCallback(std::function<void()> callback);

 private:
  void initUi();
  void initSignals();
  void notifyChanged();
  void populateDeviceCombo();
  void populateUutCombo();
  void onDeviceComboChanged();
  void onUutComboChanged();
  void addConnection();
  void removeConnection(int index);
  void refreshList();
  bool connectionExists(const QString& device,
                        const QString& devicePort,
                        const QString& uut,
                        const QString& uutPort) const;
  QWidget* makeConnectionRow(int index);
  QToolButton* makeDeleteButton(std::function<void()> handler, QWidget* parent);

  std::function<void()> changed_cb_;
  TopologyData* data_ = nullptr;
  QComboBox* device_combo_ = nullptr;
  QComboBox* device_port_combo_ = nullptr;
  QComboBox* uut_combo_ = nullptr;
  QComboBox* uut_port_combo_ = nullptr;
  QToolButton* add_btn_ = nullptr;
  QStackedWidget* list_stack_ = nullptr;
  QLabel* empty_label_ = nullptr;
  QWidget* list_container_ = nullptr;
  QVBoxLayout* list_layout_ = nullptr;
};

TopologyFileWizard::ConnectionPage::ConnectionPage(TopologyData* data,
                                                   QWidget* parent)
    : WizardPage(parent), data_(data) {
  initUi();
  initSignals();
}

bool TopologyFileWizard::ConnectionPage::validatePage() {
  if (!data_->connections.isEmpty()) {
    return true;
  }
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("连线为空"),
      QStringLiteral("当前未定义任何连线，确定要继续吗？"));
  return reply == QMessageBox::Yes;
}

void TopologyFileWizard::ConnectionPage::refresh() {
  populateDeviceCombo();
  populateUutCombo();
  refreshList();
}

void TopologyFileWizard::ConnectionPage::setChangedCallback(
    std::function<void()> callback) {
  changed_cb_ = std::move(callback);
}

void TopologyFileWizard::ConnectionPage::notifyChanged() {
  if (changed_cb_) {
    changed_cb_();
  }
}

void TopologyFileWizard::ConnectionPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);

  auto* intro =
      new QLabel(QStringLiteral("定义设备端口与 UUT 端口的物理连接"), this);
  intro->setObjectName(QStringLiteral("topoIntro"));
  layout->addWidget(intro);

  // 新增连线行
  auto* addRow = new QFrame(this);
  addRow->setObjectName(QStringLiteral("topoConnAddRow"));
  addRow->setFrameShape(QFrame::NoFrame);
  auto* addLay = new QHBoxLayout(addRow);
  addLay->setContentsMargins(12, 10, 12, 10);
  addLay->setSpacing(8);
  device_combo_ = new QComboBox(addRow);
  device_combo_->setObjectName(QStringLiteral("topoConnDeviceCombo"));
  device_port_combo_ = new QComboBox(addRow);
  device_port_combo_->setObjectName(QStringLiteral("topoConnDevicePortCombo"));
  uut_combo_ = new QComboBox(addRow);
  uut_combo_->setObjectName(QStringLiteral("topoConnUutCombo"));
  uut_port_combo_ = new QComboBox(addRow);
  uut_port_combo_->setObjectName(QStringLiteral("topoConnUutPortCombo"));
  auto* arrow = new QLabel(QStringLiteral("→"), addRow);
  arrow->setObjectName(QStringLiteral("topoConnArrow"));
  add_btn_ = new QToolButton(addRow);
  add_btn_->setObjectName(QStringLiteral("topoAddConnBtn"));
  add_btn_->setText(QStringLiteral("添加连线"));
  add_btn_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  add_btn_->setIcon(AppIconProvider::instance().icon(QStringLiteral("plus")));
  addLay->addWidget(device_combo_, 1);
  addLay->addWidget(device_port_combo_, 1);
  addLay->addWidget(arrow);
  addLay->addWidget(uut_combo_, 1);
  addLay->addWidget(uut_port_combo_, 1);
  addLay->addWidget(add_btn_);
  layout->addWidget(addRow);

  // 已有连线列表（空态 / 列表 双页切换）
  list_stack_ = new QStackedWidget(this);
  empty_label_ =
      new QLabel(QStringLiteral("暂无连线，点击上方添加连线"), list_stack_);
  empty_label_->setObjectName(QStringLiteral("topoEmptyHint"));
  empty_label_->setAlignment(Qt::AlignCenter);
  list_container_ = new QWidget(list_stack_);
  list_layout_ = new QVBoxLayout(list_container_);
  list_layout_->setContentsMargins(0, 0, 0, 0);
  list_layout_->setSpacing(6);
  list_stack_->addWidget(empty_label_);
  list_stack_->addWidget(list_container_);
  layout->addWidget(list_stack_, 1);

  auto* tip = new QLabel(
      QStringLiteral("每条连线表示一个设备端口与一个 UUT 端口的物理连接"),
      this);
  tip->setObjectName(QStringLiteral("topoTip"));
  layout->addWidget(tip);
}

void TopologyFileWizard::ConnectionPage::initSignals() {
  connect(device_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &ConnectionPage::onDeviceComboChanged);
  connect(uut_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &ConnectionPage::onUutComboChanged);
  connect(add_btn_, &QToolButton::clicked, this,
          &ConnectionPage::addConnection);
}

void TopologyFileWizard::ConnectionPage::populateDeviceCombo() {
  const QString prev = device_combo_->currentData().toString();
  device_combo_->clear();
  for (const WizardDevice& d : data_->devices) {
    if (!d.ports.isEmpty()) {
      device_combo_->addItem(d.name, d.name);
    }
  }
  const int idx = device_combo_->findData(prev);
  if (idx >= 0) {
    device_combo_->setCurrentIndex(idx);
  }
  onDeviceComboChanged();
}

void TopologyFileWizard::ConnectionPage::populateUutCombo() {
  const QString prev = uut_combo_->currentData().toString();
  uut_combo_->clear();
  for (const WizardUut& u : data_->uuts) {
    if (!u.ports.isEmpty()) {
      uut_combo_->addItem(u.name, u.name);
    }
  }
  const int idx = uut_combo_->findData(prev);
  if (idx >= 0) {
    uut_combo_->setCurrentIndex(idx);
  }
  onUutComboChanged();
}

void TopologyFileWizard::ConnectionPage::onDeviceComboChanged() {
  const QString device = device_combo_->currentData().toString();
  const QString prevPort = device_port_combo_->currentData().toString();
  device_port_combo_->clear();
  for (const WizardDevice& d : data_->devices) {
    if (d.name != device) {
      continue;
    }
    for (const QString& port : d.ports) {
      device_port_combo_->addItem(port, port);
    }
    break;
  }
  const int idx = device_port_combo_->findData(prevPort);
  if (idx >= 0) {
    device_port_combo_->setCurrentIndex(idx);
  }
}

void TopologyFileWizard::ConnectionPage::onUutComboChanged() {
  const QString uut = uut_combo_->currentData().toString();
  const QString prevPort = uut_port_combo_->currentData().toString();
  uut_port_combo_->clear();
  for (const WizardUut& u : data_->uuts) {
    if (u.name != uut) {
      continue;
    }
    for (const QString& port : u.ports) {
      uut_port_combo_->addItem(port, port);
    }
    break;
  }
  const int idx = uut_port_combo_->findData(prevPort);
  if (idx >= 0) {
    uut_port_combo_->setCurrentIndex(idx);
  }
}

bool TopologyFileWizard::ConnectionPage::connectionExists(
    const QString& device,
    const QString& devicePort,
    const QString& uut,
    const QString& uutPort) const {
  for (const WizardConnection& c : data_->connections) {
    if (c.deviceName == device && c.devicePort == devicePort &&
        c.uutName == uut && c.uutPort == uutPort) {
      return true;
    }
  }
  return false;
}

void TopologyFileWizard::ConnectionPage::addConnection() {
  const QString device = device_combo_->currentData().toString();
  const QString devicePort = device_port_combo_->currentData().toString();
  const QString uut = uut_combo_->currentData().toString();
  const QString uutPort = uut_port_combo_->currentData().toString();
  if (device.isEmpty() || devicePort.isEmpty() || uut.isEmpty() ||
      uutPort.isEmpty()) {
    return;
  }
  if (connectionExists(device, devicePort, uut, uutPort)) {
    QMessageBox::information(this, QStringLiteral("添加连线"),
                             QStringLiteral("该连线已存在，请尝试其他组合。"));
    return;
  }
  WizardConnection c;
  c.deviceName = device;
  c.devicePort = devicePort;
  c.uutName = uut;
  c.uutPort = uutPort;
  data_->connections.append(c);
  reconcileUutPortAllowedTypes(*data_);
  refreshList();
  notifyChanged();
}

void TopologyFileWizard::ConnectionPage::removeConnection(int index) {
  if (index < 0 || index >= data_->connections.size()) {
    return;
  }
  data_->connections.removeAt(index);
  reconcileUutPortAllowedTypes(*data_);
  refreshList();
  notifyChanged();
}

void TopologyFileWizard::ConnectionPage::refreshList() {
  while (QLayoutItem* item = list_layout_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }
  for (int i = 0; i < data_->connections.size(); ++i) {
    list_layout_->addWidget(makeConnectionRow(i));
  }
  list_layout_->addStretch();
  list_stack_->setCurrentIndex(data_->connections.isEmpty() ? 0 : 1);
}

QWidget* TopologyFileWizard::ConnectionPage::makeConnectionRow(int index) {
  const WizardConnection& c = data_->connections[index];
  auto* row = new QFrame(this);
  row->setObjectName(QStringLiteral("topoConnRow"));
  row->setFrameShape(QFrame::NoFrame);
  auto* lay = new QHBoxLayout(row);
  lay->setContentsMargins(12, 8, 12, 8);
  lay->setSpacing(8);
  auto* deviceLabel =
      new QLabel(c.deviceName + QStringLiteral(" · ") + c.devicePort, row);
  auto* arrow = new QLabel(QStringLiteral("→"), row);
  arrow->setObjectName(QStringLiteral("topoConnArrow"));
  auto* uutLabel =
      new QLabel(c.uutName + QStringLiteral(" · ") + c.uutPort, row);
  auto* del =
      makeDeleteButton([this, index]() { removeConnection(index); }, row);
  lay->addWidget(deviceLabel, 1);
  lay->addWidget(arrow);
  lay->addWidget(uutLabel, 1);
  lay->addWidget(del);
  return row;
}

QToolButton* TopologyFileWizard::ConnectionPage::makeDeleteButton(
    std::function<void()> handler,
    QWidget* parent) {
  auto* btn = new QToolButton(parent);
  btn->setObjectName(QStringLiteral("topoConnDelete"));
  btn->setIcon(AppIconProvider::instance().icon(QStringLiteral("close")));
  btn->setIconSize(QSize(14, 14));
  btn->setCursor(Qt::PointingHandCursor);
  connect(btn, &QToolButton::clicked, this, [handler](bool) { handler(); });
  return btn;
}

// ── 摘要页 ──

class TopologyFileWizard::SummaryPage : public WizardPage {
 public:
  explicit SummaryPage(QWidget* parent = nullptr);

  void setSummary(const QString& templateLabel, const TopologyData& data);
  QString stepLabel() const override { return QStringLiteral("完成"); }

 private:
  void initUi();
  QLabel* addItem(QGridLayout* grid,
                  int row,
                  int col,
                  const QString& label,
                  const QString& value,
                  int colspan = 1);

  QLabel* template_value_ = nullptr;
  QLabel* device_value_ = nullptr;
  QLabel* uut_value_ = nullptr;
  QLabel* connection_value_ = nullptr;
  QLabel* desc_value_ = nullptr;
};

TopologyFileWizard::SummaryPage::SummaryPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
}

void TopologyFileWizard::SummaryPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro = new QLabel(
      QStringLiteral("确认拓扑配置，点击「创建拓扑」即可生成 .etopo 文件。"),
      this);
  intro->setObjectName(QStringLiteral("topoIntro"));
  layout->addWidget(intro);

  auto* grid = new QGridLayout();
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(32);
  grid->setVerticalSpacing(16);
  template_value_ =
      addItem(grid, 0, 0, QStringLiteral("拓扑模板"), QStringLiteral("—"));
  device_value_ =
      addItem(grid, 0, 1, QStringLiteral("设备数量"), QStringLiteral("0"));
  uut_value_ =
      addItem(grid, 1, 0, QStringLiteral("UUT 数量"), QStringLiteral("0"));
  connection_value_ =
      addItem(grid, 1, 1, QStringLiteral("连线数量"), QStringLiteral("0"));
  desc_value_ = addItem(grid, 2, 0, QStringLiteral("拓扑结构"),
                        QStringLiteral("（暂无设备）"), 2);
  layout->addLayout(grid);
  layout->addStretch();
}

QLabel* TopologyFileWizard::SummaryPage::addItem(QGridLayout* grid,
                                                 int row,
                                                 int col,
                                                 const QString& label,
                                                 const QString& value,
                                                 int colspan) {
  auto* labelEl = new QLabel(label, this);
  labelEl->setObjectName(QStringLiteral("topoSummaryLabel"));
  grid->addWidget(labelEl, row, col);
  auto* valueEl = new QLabel(value, this);
  valueEl->setObjectName(QStringLiteral("topoSummaryValue"));
  valueEl->setWordWrap(true);
  grid->addWidget(valueEl, row, col + 1, 1, colspan);
  return valueEl;
}

void TopologyFileWizard::SummaryPage::setSummary(const QString& templateLabel,
                                                 const TopologyData& data) {
  template_value_->setText(templateLabel);
  device_value_->setText(QString::number(data.devices.size()));
  uut_value_->setText(QString::number(data.uuts.size()));
  connection_value_->setText(QString::number(data.connections.size()));

  if (data.devices.isEmpty()) {
    desc_value_->setText(QStringLiteral("（暂无设备）"));
    return;
  }
  QStringList devNames;
  for (const WizardDevice& d : data.devices) {
    devNames << QStringLiteral("%1(%2p)").arg(d.name).arg(d.ports.size());
  }
  QStringList uutNames;
  for (const WizardUut& u : data.uuts) {
    uutNames << QStringLiteral("%1(%2p)").arg(u.name).arg(u.ports.size());
  }
  desc_value_->setText(QStringLiteral("设备: %1 | UUT: %2 | 连线: %3条")
                           .arg(devNames.join(QStringLiteral("; ")))
                           .arg(uutNames.join(QStringLiteral("; ")))
                           .arg(data.connections.size()));
}

// ── 向导主体 ──

TopologyFileWizard::TopologyFileWizard(QWidget* parent)
    : BaseWizardDialog(parent) {
  initUi();
  initSignals();
}

void TopologyFileWizard::initUi() {
  setWindowTitle(QStringLiteral("新建拓扑文件"));
  setHeader(QStringLiteral("file_etopo"), QStringLiteral("新建拓扑文件"),
            QStringLiteral("定义测试系统的硬件拓扑结构：设备、UUT 与信号连线"));
  setCreateButtonText(QStringLiteral("创建拓扑"));

  template_page_ = new TemplatePage(this);
  device_uut_page_ = new DeviceUutPage(&data_, this);
  connection_page_ = new ConnectionPage(&data_, this);
  summary_page_ = new SummaryPage(this);
  addPage(template_page_);
  addPage(device_uut_page_);
  addPage(connection_page_);
  addPage(summary_page_);

  // 卡片走专用 QSS（#topoWizardCard，区别于其它向导）
  if (QWidget* card = findChild<QWidget*>(QStringLiteral("wizardCard"))) {
    card->setObjectName(QStringLiteral("topoWizardCard"));
  }

  loadTemplate(QStringLiteral("empty"));
}

void TopologyFileWizard::initSignals() {
  // 模板页：选中模板变化才重载（名称编辑只触发 completeChanged，不影响模板）
  connect(template_page_, &WizardPage::completeChanged, this, [this]() {
    const QString sel = template_page_->selectedTemplateId();
    if (!sel.isEmpty() && sel != template_id_) {
      loadTemplate(sel);
    }
    updateSummary();
  });
  device_uut_page_->setChangedCallback([this]() {
    connection_page_->refresh();
    updateSummary();
  });
  connection_page_->setChangedCallback([this]() { updateSummary(); });
  connect(this, &BaseWizardDialog::currentPageChanged, this, [this](int index) {
    if (index == pageCount() - 1) {
      updateSummary();
    } else if (index == 2) {
      connection_page_->refresh();
    }
  });
}

QString TopologyFileWizard::topologyName() const {
  return template_page_->name();
}

void TopologyFileWizard::loadTemplate(const QString& templateId) {
  data_ = TopologyData();
  if (templateId == QStringLiteral("single")) {
    addDeviceToData(data_, *deviceTemplate(QStringLiteral("can")),
                    QStringLiteral("PXI-CAN卡_1"));
    addUutToData(data_, QStringLiteral("UUT_1"),
                 {QStringLiteral("p0"), QStringLiteral("p1")});
    addConnectionToData(data_, QStringLiteral("PXI-CAN卡_1"),
                        QStringLiteral("ch0"), QStringLiteral("UUT_1"),
                        QStringLiteral("p0"));
  } else if (templateId == QStringLiteral("multi")) {
    addDeviceToData(data_, *deviceTemplate(QStringLiteral("can")),
                    QStringLiteral("PXI-CAN卡_1"));
    addDeviceToData(data_, *deviceTemplate(QStringLiteral("ad")),
                    QStringLiteral("PXI-AD采集卡_1"));
    addDeviceToData(data_, *deviceTemplate(QStringLiteral("serial")),
                    QStringLiteral("PXI-串口卡_1"));
    addUutToData(
        data_, QStringLiteral("飞控#1"),
        {QStringLiteral("CAN"), QStringLiteral("AD"), QStringLiteral("IO")});
    addUutToData(data_, QStringLiteral("飞控#2"),
                 {QStringLiteral("CAN"), QStringLiteral("AD")});
    addConnectionToData(data_, QStringLiteral("PXI-CAN卡_1"),
                        QStringLiteral("ch0"), QStringLiteral("飞控#1"),
                        QStringLiteral("CAN"));
    addConnectionToData(data_, QStringLiteral("PXI-AD采集卡_1"),
                        QStringLiteral("ch0"), QStringLiteral("飞控#1"),
                        QStringLiteral("AD"));
    addConnectionToData(data_, QStringLiteral("PXI-串口卡_1"),
                        QStringLiteral("ch0"), QStringLiteral("飞控#2"),
                        QStringLiteral("CAN"));
  }
  reconcileUutPortAllowedTypes(data_);
  template_id_ = templateId;
  template_page_->setTemplateId(templateId);
  device_uut_page_->reset();
  connection_page_->refresh();
  updateSummary();
}

QString TopologyFileWizard::templateLabel(const QString& templateId) {
  if (templateId == QStringLiteral("single")) {
    return QStringLiteral("单设备 + UUT");
  }
  if (templateId == QStringLiteral("multi")) {
    return QStringLiteral("多设备混合");
  }
  return QStringLiteral("空拓扑");
}

void TopologyFileWizard::updateSummary() {
  summary_page_->setSummary(templateLabel(template_id_), data_);
}

etest::topology::TopologyDocument* TopologyFileWizard::resultDocument() {
  auto* doc = new etest::topology::TopologyDocument(this);
  const qreal kDeviceX = -1100.0;
  const qreal kUutX = -500.0;
  const qreal kRowStep = 160.0;
  const qreal kTop = -800.0;

  for (int i = 0; i < data_.uuts.size(); ++i) {
    const WizardUut& w = data_.uuts[i];
    etest::topology::TopologyProduct p;
    p.name = w.name;
    p.position = QPointF(kUutX, kTop + i * kRowStep);
    for (int j = 0; j < w.ports.size(); ++j) {
      etest::topology::TopologyPort port;
      port.name = w.ports[j];
      port.direction = etest::topology::TopologyPort::Direction::Bidirectional;
      port.allowedDeviceTypes = w.portAllowedTypes[j];
      port.functionType = etest::topology::FunctionType::CUSTOM;
      port.positionHint = -1;
      port.portStyle = 0;
      p.ports.append(port);
    }
    doc->addProduct(p);
  }

  for (int i = 0; i < data_.devices.size(); ++i) {
    const WizardDevice& w = data_.devices[i];
    etest::topology::TopologyDevice d;
    d.name = w.name;
    d.deviceType = w.deviceType;
    d.pluginId = w.pluginId;
    d.position = QPointF(kDeviceX, kTop + i * kRowStep);
    for (const QString& portName : w.ports) {
      etest::topology::TopologyDevicePort dp;
      dp.name = portName;
      dp.direction = etest::topology::TopologyPort::Direction::Bidirectional;
      dp.functionType = functionTypeFor(w.deviceType);
      dp.positionHint = -1;
      dp.portStyle = 0;
      d.ports.append(dp);
    }
    doc->addDevice(d);
  }

  for (const WizardConnection& c : data_.connections) {
    etest::topology::TopologyConnection conn;
    conn.productName = c.uutName;
    conn.portName = c.uutPort;
    conn.deviceName = c.deviceName;
    conn.devicePort = c.devicePort;
    conn.style = etest::topology::PathStyle::Bezier;
    doc->addConnection(conn);
  }

  LOG_INFO("PROJECT_UI", "向导新建拓扑 [devices={}] [uuts={}] [connections={}]",
           data_.devices.size(), data_.uuts.size(), data_.connections.size());
  return doc;
}

bool TopologyFileWizard::onCreateValidate() {
  const QString name = topologyName();
  if (name.isEmpty() || isIllegalName(name)) {
    QMessageBox::warning(this, QStringLiteral("无法创建拓扑"),
                         QStringLiteral("拓扑名称不能为空或包含非法字符。"));
    return false;
  }
  return true;
}

void TopologyFileWizard::confirmCancel() {
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("取消"), QStringLiteral("确定要取消创建拓扑吗？"));
  if (reply == QMessageBox::Yes) {
    reject();
  }
}

}  // namespace etest::app
