#include "ProtocolFileWizard.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>

#include "AppIconProvider.h"
#include "WizardTemplateCard.h"
#include "logger/Logger.h"

#include <icd/node.hpp>

namespace etest::app {

namespace {

// ── 字段数据类型（设计决策 #2：移除 int8）──

QStringList typeOptions() {
  return {QStringLiteral("uint8"),  QStringLiteral("uint16"),
          QStringLiteral("int16"),  QStringLiteral("uint32"),
          QStringLiteral("int32"),  QStringLiteral("float"),
          QStringLiteral("double")};
}

icd::ValueType valueTypeFor(const QString& type) {
  if (type == QStringLiteral("uint8")) {
    return icd::ValueType::byte_;
  }
  if (type == QStringLiteral("uint16")) {
    return icd::ValueType::word;
  }
  if (type == QStringLiteral("int16")) {
    return icd::ValueType::shortint;
  }
  if (type == QStringLiteral("uint32")) {
    return icd::ValueType::longword;
  }
  if (type == QStringLiteral("int32")) {
    return icd::ValueType::integer;
  }
  if (type == QStringLiteral("float")) {
    return icd::ValueType::single;
  }
  if (type == QStringLiteral("double")) {
    return icd::ValueType::double_;
  }
  return icd::ValueType::unknown;
}

QString typeAbbr(const QString& type) {
  if (type == QStringLiteral("uint8")) {
    return QStringLiteral("U8");
  }
  if (type == QStringLiteral("uint16")) {
    return QStringLiteral("U16");
  }
  if (type == QStringLiteral("int16")) {
    return QStringLiteral("I16");
  }
  if (type == QStringLiteral("uint32")) {
    return QStringLiteral("U32");
  }
  if (type == QStringLiteral("int32")) {
    return QStringLiteral("I32");
  }
  if (type == QStringLiteral("float")) {
    return QStringLiteral("F32");
  }
  if (type == QStringLiteral("double")) {
    return QStringLiteral("F64");
  }
  return type.toUpper();
}

// ── 模板静态数据（转译 HTML defaultFields；offset 为绝对位偏移）──

WizardField makeField(const QString& name, int offset, int width,
                      const QString& type) {
  WizardField f;
  f.name = name;
  f.offset = offset;
  f.width = width;
  f.type = type;
  f.scale = QStringLiteral("1.0");
  return f;
}

QList<WizardField> templateFields(const QString& id) {
  if (id == QStringLiteral("a429")) {
    return {makeField(QStringLiteral("Label"), 0, 8, QStringLiteral("uint8")),
            makeField(QStringLiteral("SDI"), 8, 2, QStringLiteral("uint8")),
            makeField(QStringLiteral("Data"), 10, 18, QStringLiteral("uint32")),
            makeField(QStringLiteral("SSM"), 28, 2, QStringLiteral("uint8")),
            makeField(QStringLiteral("Parity"), 30, 1, QStringLiteral("uint8"))};
  }
  if (id == QStringLiteral("can")) {
    return {makeField(QStringLiteral("ID"), 0, 11, QStringLiteral("uint16")),
            makeField(QStringLiteral("RTR"), 11, 1, QStringLiteral("uint8")),
            makeField(QStringLiteral("IDE"), 12, 1, QStringLiteral("uint8")),
            makeField(QStringLiteral("DLC"), 13, 4, QStringLiteral("uint8")),
            makeField(QStringLiteral("Data0"), 16, 8, QStringLiteral("uint8")),
            makeField(QStringLiteral("Data1"), 24, 8, QStringLiteral("uint8"))};
  }
  if (id == QStringLiteral("1553")) {
    return {makeField(QStringLiteral("Sync"), 0, 3, QStringLiteral("uint8")),
            makeField(QStringLiteral("RT Address"), 3, 5,
                      QStringLiteral("uint8")),
            makeField(QStringLiteral("T/R"), 8, 1, QStringLiteral("uint8")),
            makeField(QStringLiteral("Subaddress"), 9, 5,
                      QStringLiteral("uint8")),
            makeField(QStringLiteral("Data"), 14, 16, QStringLiteral("uint16")),
            makeField(QStringLiteral("Parity"), 30, 1, QStringLiteral("uint8"))};
  }
  // custom：帧头 / 长度 / 数据载荷
  return {makeField(QStringLiteral("帧头"), 0, 8, QStringLiteral("uint8")),
          makeField(QStringLiteral("长度"), 8, 8, QStringLiteral("uint8")),
          makeField(QStringLiteral("数据载荷"), 16, 16,
                    QStringLiteral("uint16"))};
}

// 帧 ID 解析：支持 0x 前缀 hex、无前缀 hex（如 FF）、十进制；拒绝八进制陷阱
// （"0100" 按十进制 100 而非八进制 64）。设计决策 #12。
bool parseFrameId(const QString& text, int* out) {
  const QString t = text.trimmed();
  if (t.isEmpty()) {
    return false;
  }
  bool ok = false;
  int v = 0;
  if (t.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
    v = t.mid(2).toInt(&ok, 16);
  } else {
    v = t.toInt(&ok, 10);
    if (!ok) {
      v = t.toInt(&ok, 16);
    }
  }
  if (ok) {
    *out = v;
    return true;
  }
  return false;
}

// 摘要字段 tag 文案：name (U32 | B16)，B 为绝对位偏移
QString fieldTagText(const WizardField& f) {
  const QString name =
      f.name.trimmed().isEmpty() ? QStringLiteral("未命名") : f.name.trimmed();
  return QStringLiteral("%1 (%2 | B%3)").arg(name, typeAbbr(f.type))
      .arg(f.offset);
}

}  // namespace

// ── 模板页 ──

class ProtocolFileWizard::TemplatePage : public WizardPage {
 public:
  explicit TemplatePage(QWidget* parent = nullptr);

  QString selectedTemplateId() const;
  QString stepLabel() const override { return QStringLiteral("模板"); }

 private:
  void initUi();
  void initSignals();
  WizardTemplateCard* addCard(QButtonGroup* group, QHBoxLayout* layout,
                              const QString& templateId, const QString& iconName,
                              const QString& title, const QString& desc,
                              const QString& badge);

  QButtonGroup* group_ = nullptr;
};

ProtocolFileWizard::TemplatePage::TemplatePage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void ProtocolFileWizard::TemplatePage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro =
      new QLabel(QStringLiteral("选择协议模板，系统将自动生成帧结构和字段配置示例。"),
                 this);
  intro->setObjectName(QStringLiteral("protoInfoIntro"));
  layout->addWidget(intro);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);

  auto* grid = new QHBoxLayout();
  grid->setSpacing(14);
  layout->addLayout(grid);

  auto* custom = addCard(group_, grid, QStringLiteral("custom"),
                         QStringLiteral("pencil"), QStringLiteral("自定义空协议"),
                         QStringLiteral("手动添加帧头和字段，自由定义"),
                         QStringLiteral("推荐"));
  addCard(group_, grid, QStringLiteral("a429"), QStringLiteral("plane"),
          QStringLiteral("ARINC 429"),
          QStringLiteral("航空总线标准协议，含 Label/SDI/Data/SSM"), QString());
  addCard(group_, grid, QStringLiteral("can"), QStringLiteral("car"),
          QStringLiteral("CAN 2.0B"),
          QStringLiteral("标准数据帧，含 ID、DLC 和数据场"), QString());
  addCard(group_, grid, QStringLiteral("1553"), QStringLiteral("satellite"),
          QStringLiteral("MIL-STD-1553B"), QStringLiteral("命令/状态字 + 数据块结构"),
          QString());
  custom->setChecked(true);

  layout->addStretch();
}

void ProtocolFileWizard::TemplatePage::initSignals() {
  connect(group_,
          QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this,
          [this](QAbstractButton* /*button*/) { emit completeChanged(); });
}

QString ProtocolFileWizard::TemplatePage::selectedTemplateId() const {
  if (!group_) {
    return QStringLiteral("custom");
  }
  QAbstractButton* checked = group_->checkedButton();
  return checked ? checked->property("templateId").toString()
                 : QStringLiteral("custom");
}

WizardTemplateCard* ProtocolFileWizard::TemplatePage::addCard(
    QButtonGroup* group, QHBoxLayout* layout, const QString& templateId,
    const QString& iconName, const QString& title, const QString& desc,
    const QString& badge) {
  auto* card = new WizardTemplateCard(iconName, title, desc, badge, this);
  card->setProperty("templateId", templateId);
  group->addButton(card);
  layout->addWidget(card, 1);
  return card;
}

// ── 帧属性页 ──

class ProtocolFileWizard::FrameInfoPage : public WizardPage {
 public:
  explicit FrameInfoPage(QWidget* parent = nullptr);

  QString name() const;
  QString idText() const;
  /// 帧 ID 解析为 int（空或非法 → 0，设计决策 #4）
  int idValue() const;
  /// "data" / "cmd" / "data_cmd"
  QString typeKey() const;
  /// "little" / "big"
  QString orderKey() const;
  QString description() const;
  QString stepLabel() const override { return QStringLiteral("帧属性"); }
  bool validatePage() override;

 private:
  void initUi();
  void initSignals();
  void onTextChanged();
  bool isNameValid() const;
  bool isIdValid() const;
  void flashFieldError(QLineEdit* edit);

  QLineEdit* name_edit_ = nullptr;
  QLineEdit* id_edit_ = nullptr;
  QComboBox* type_combo_ = nullptr;
  QComboBox* order_combo_ = nullptr;
  QTextEdit* desc_edit_ = nullptr;
};

ProtocolFileWizard::FrameInfoPage::FrameInfoPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void ProtocolFileWizard::FrameInfoPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);

  // 帧名称
  auto* nameLabelRow = new QHBoxLayout();
  nameLabelRow->setSpacing(6);
  auto* nameLabel = new QLabel(QStringLiteral("帧名称"), this);
  nameLabel->setObjectName(QStringLiteral("protoFieldLabel"));
  auto* nameHint = new QLabel(QStringLiteral("· 建议使用英文标识"), this);
  nameHint->setObjectName(QStringLiteral("protoFieldLabel"));
  nameLabelRow->addWidget(nameLabel);
  nameLabelRow->addWidget(nameHint);
  nameLabelRow->addStretch();
  layout->addLayout(nameLabelRow);

  name_edit_ = new QLineEdit(this);
  name_edit_->setObjectName(QStringLiteral("frameNameEdit"));
  name_edit_->setPlaceholderText(QStringLiteral("例如：A429_EngineStatus"));
  layout->addWidget(name_edit_);

  // 帧 ID（hex，非必填）
  auto* idLabel = new QLabel(QStringLiteral("帧 ID"), this);
  idLabel->setObjectName(QStringLiteral("protoFieldLabel"));
  layout->addWidget(idLabel);
  id_edit_ = new QLineEdit(this);
  id_edit_->setObjectName(QStringLiteral("frameIdEdit"));
  id_edit_->setPlaceholderText(QStringLiteral("例如：0x100（可选）"));
  layout->addWidget(id_edit_);

  // 帧类型 + 字节序（双列）
  auto* rowLayout = new QHBoxLayout();
  rowLayout->setSpacing(18);

  auto* typeGroup = new QVBoxLayout();
  typeGroup->setSpacing(6);
  auto* typeLabel = new QLabel(QStringLiteral("帧类型"), this);
  typeLabel->setObjectName(QStringLiteral("protoFieldLabel"));
  type_combo_ = new QComboBox(this);
  type_combo_->setObjectName(QStringLiteral("frameTypeCombo"));
  type_combo_->addItem(QStringLiteral("数据帧 (Data)"), QStringLiteral("data"));
  type_combo_->addItem(QStringLiteral("命令帧 (Command)"),
                       QStringLiteral("cmd"));
  type_combo_->addItem(QStringLiteral("数据/命令混合"),
                       QStringLiteral("data_cmd"));
  typeGroup->addWidget(typeLabel);
  typeGroup->addWidget(type_combo_);

  auto* orderGroup = new QVBoxLayout();
  orderGroup->setSpacing(6);
  auto* orderLabel = new QLabel(QStringLiteral("字节序"), this);
  orderLabel->setObjectName(QStringLiteral("protoFieldLabel"));
  order_combo_ = new QComboBox(this);
  order_combo_->setObjectName(QStringLiteral("byteOrderCombo"));
  order_combo_->addItem(QStringLiteral("小端 (Little Endian)"),
                        QStringLiteral("little"));
  order_combo_->addItem(QStringLiteral("大端 (Big Endian)"),
                        QStringLiteral("big"));
  orderGroup->addWidget(orderLabel);
  orderGroup->addWidget(order_combo_);

  rowLayout->addLayout(typeGroup, 1);
  rowLayout->addLayout(orderGroup, 1);
  layout->addLayout(rowLayout);

  // 帧描述
  auto* descLabel = new QLabel(QStringLiteral("帧描述"), this);
  descLabel->setObjectName(QStringLiteral("protoFieldLabel"));
  layout->addWidget(descLabel);
  desc_edit_ = new QTextEdit(this);
  desc_edit_->setObjectName(QStringLiteral("frameDescEdit"));
  desc_edit_->setFixedHeight(72);
  desc_edit_->setPlaceholderText(QStringLiteral("简要说明该帧的用途"));
  layout->addWidget(desc_edit_);

  layout->addStretch();
}

void ProtocolFileWizard::FrameInfoPage::initSignals() {
  connect(name_edit_, &QLineEdit::textChanged, this,
          &FrameInfoPage::onTextChanged);
  connect(id_edit_, &QLineEdit::textChanged, this,
          &FrameInfoPage::onTextChanged);
  connect(desc_edit_, &QTextEdit::textChanged, this,
          &FrameInfoPage::onTextChanged);
  connect(type_combo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int /*index*/) { emit completeChanged(); });
  connect(order_combo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int /*index*/) { emit completeChanged(); });
}

void ProtocolFileWizard::FrameInfoPage::onTextChanged() {
  // 任一输入修正即清除对应红闪，避免错误提示残留到定时器到期
  if (name_edit_->property("error").toBool()) {
    name_edit_->setProperty("error", false);
    name_edit_->style()->unpolish(name_edit_);
    name_edit_->style()->polish(name_edit_);
  }
  if (id_edit_->property("error").toBool()) {
    id_edit_->setProperty("error", false);
    id_edit_->style()->unpolish(id_edit_);
    id_edit_->style()->polish(id_edit_);
  }
  emit completeChanged();
}

bool ProtocolFileWizard::FrameInfoPage::validatePage() {
  if (!isNameValid()) {
    flashFieldError(name_edit_);
    return false;
  }
  if (!isIdValid()) {
    flashFieldError(id_edit_);
    return false;
  }
  return true;
}

bool ProtocolFileWizard::FrameInfoPage::isNameValid() const {
  const QString name = this->name();
  if (name.isEmpty()) {
    return false;
  }
  static const QString kInvalidChars = "\\/:*?\"<>|";
  for (const QChar& c : name) {
    if (kInvalidChars.contains(c)) {
      return false;
    }
  }
  return true;
}

bool ProtocolFileWizard::FrameInfoPage::isIdValid() const {
  if (id_edit_->text().trimmed().isEmpty()) {
    return true;
  }
  int out = 0;
  return parseFrameId(id_edit_->text(), &out);
}

void ProtocolFileWizard::FrameInfoPage::flashFieldError(QLineEdit* edit) {
  edit->setProperty("error", true);
  edit->style()->unpolish(edit);
  edit->style()->polish(edit);
  edit->setFocus();
  QTimer::singleShot(1200, edit, [edit]() {
    edit->setProperty("error", false);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
  });
}

QString ProtocolFileWizard::FrameInfoPage::name() const {
  return name_edit_->text().trimmed();
}

QString ProtocolFileWizard::FrameInfoPage::idText() const {
  return id_edit_->text().trimmed();
}

int ProtocolFileWizard::FrameInfoPage::idValue() const {
  int out = 0;
  parseFrameId(id_edit_->text(), &out);
  return out;
}

QString ProtocolFileWizard::FrameInfoPage::typeKey() const {
  return type_combo_->currentData().toString();
}

QString ProtocolFileWizard::FrameInfoPage::orderKey() const {
  return order_combo_->currentData().toString();
}

QString ProtocolFileWizard::FrameInfoPage::description() const {
  return desc_edit_->toPlainText().trimmed();
}

// ── 字段定义页 ──

class ProtocolFileWizard::FieldPage : public WizardPage {
 public:
  explicit FieldPage(QWidget* parent = nullptr);

  const QList<WizardField>& fields() const { return fields_; }
  void setFields(const QList<WizardField>& fields);
  QString stepLabel() const override { return QStringLiteral("字段"); }
  bool validatePage() override;
  /// 创建期整体校验：系数非数字 → 红闪定位并返回 false
  bool validateAll();

 private:
  void initUi();
  void initSignals();
  void addField();
  void deleteField(int row);
  void syncField(int row);
  void rebuildTable();
  QWidget* makeFieldActions(int row);
  QToolButton* makeToolButton(const QString& iconName,
                              std::function<void()> handler);
  void flashError(QLineEdit* edit);

  QStackedWidget* table_stack_ = nullptr;
  QTableWidget* field_table_ = nullptr;
  QLabel* empty_label_ = nullptr;
  QPushButton* add_field_btn_ = nullptr;
  QList<WizardField> fields_;
  // 每行控件引用（重建时同步重建，供红闪定位）
  QList<QLineEdit*> name_edits_;
  QList<QSpinBox*> offset_edits_;
  QList<QSpinBox*> width_edits_;
  QList<QComboBox*> type_edits_;
  QList<QLineEdit*> scale_edits_;
  QList<QLineEdit*> unit_edits_;
};

ProtocolFileWizard::FieldPage::FieldPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void ProtocolFileWizard::FieldPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  // 工具栏
  auto* toolbar = new QWidget(this);
  auto* toolbarLay = new QHBoxLayout(toolbar);
  toolbarLay->setContentsMargins(0, 0, 0, 0);
  toolbarLay->setSpacing(10);
  auto* intro = new QLabel(QStringLiteral("定义帧内的信号字段"), toolbar);
  intro->setObjectName(QStringLiteral("fieldIntro"));
  toolbarLay->addWidget(intro);
  toolbarLay->addStretch();
  add_field_btn_ = new QPushButton(toolbar);
  add_field_btn_->setObjectName(QStringLiteral("addFieldBtn"));
  add_field_btn_->setIcon(core_ui::AppIconProvider::instance().icon(
      QStringLiteral("plus")));
  add_field_btn_->setText(QStringLiteral("添加字段"));
  toolbarLay->addWidget(add_field_btn_);
  layout->addWidget(toolbar);

  // 字段表 / 空态
  table_stack_ = new QStackedWidget(this);
  field_table_ = new QTableWidget(table_stack_);
  field_table_->setObjectName(QStringLiteral("fieldTable"));
  field_table_->setColumnCount(7);
  field_table_->setHorizontalHeaderLabels(
      {QStringLiteral("字段名称"), QStringLiteral("位偏移"),
       QStringLiteral("位宽"), QStringLiteral("数据类型"), QStringLiteral("系数"),
       QStringLiteral("单位"), QStringLiteral("操作")});
  field_table_->verticalHeader()->setVisible(false);
  field_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  field_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  field_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  field_table_->setShowGrid(false);
  field_table_->setFrameShape(QFrame::NoFrame);
  field_table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // 名称列 Stretch 吃满富余宽度，其余列显式定宽（与 TestProgramWizard 同模式），
  // 避免全部 Fixed 列在窄布局/尺寸回退时被挤成一条
  QHeaderView* header = field_table_->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::Fixed);
  header->setSectionResizeMode(0, QHeaderView::Stretch);
  field_table_->setColumnWidth(1, 80);
  field_table_->setColumnWidth(2, 80);
  field_table_->setColumnWidth(3, 120);
  field_table_->setColumnWidth(4, 90);
  field_table_->setColumnWidth(5, 90);
  field_table_->setColumnWidth(6, 72);
  table_stack_->addWidget(field_table_);

  empty_label_ =
      new QLabel(QStringLiteral("暂无字段，点击「添加字段」开始定义信号。"),
                 table_stack_);
  empty_label_->setObjectName(QStringLiteral("emptyFieldPlaceholder"));
  empty_label_->setAlignment(Qt::AlignCenter);
  table_stack_->addWidget(empty_label_);

  layout->addWidget(table_stack_, 1);
}

void ProtocolFileWizard::FieldPage::initSignals() {
  connect(add_field_btn_, &QPushButton::clicked, this, &FieldPage::addField);
}

void ProtocolFileWizard::FieldPage::setFields(
    const QList<WizardField>& fields) {
  fields_ = fields;
  rebuildTable();
}

void ProtocolFileWizard::FieldPage::rebuildTable() {
  field_table_->setRowCount(0);
  name_edits_.clear();
  offset_edits_.clear();
  width_edits_.clear();
  type_edits_.clear();
  scale_edits_.clear();
  unit_edits_.clear();

  if (fields_.isEmpty()) {
    table_stack_->setCurrentWidget(empty_label_);
    return;
  }
  table_stack_->setCurrentWidget(field_table_);
  field_table_->setRowCount(fields_.size());

  const QStringList typeList = typeOptions();
  for (int i = 0; i < fields_.size(); ++i) {
    const WizardField& f = fields_.at(i);

    auto* nameEdit = new QLineEdit(f.name, field_table_);
    nameEdit->setPlaceholderText(QStringLiteral("信号名称"));
    nameEdit->setFrame(false);
    field_table_->setCellWidget(i, 0, nameEdit);

    auto* offsetSpin = new QSpinBox(field_table_);
    offsetSpin->setRange(0, 65535);
    offsetSpin->setValue(f.offset);
    offsetSpin->setFrame(false);
    field_table_->setCellWidget(i, 1, offsetSpin);

    auto* widthSpin = new QSpinBox(field_table_);
    widthSpin->setRange(1, 64);
    widthSpin->setValue(f.width);
    widthSpin->setFrame(false);
    field_table_->setCellWidget(i, 2, widthSpin);

    auto* typeCombo = new QComboBox(field_table_);
    typeCombo->addItems(typeList);
    typeCombo->setCurrentText(f.type);
    typeCombo->setFrame(false);
    field_table_->setCellWidget(i, 3, typeCombo);

    auto* scaleEdit = new QLineEdit(f.scale, field_table_);
    scaleEdit->setPlaceholderText(QStringLiteral("1.0"));
    scaleEdit->setFrame(false);
    field_table_->setCellWidget(i, 4, scaleEdit);

    auto* unitEdit = new QLineEdit(f.unit, field_table_);
    unitEdit->setPlaceholderText(QStringLiteral("V, rpm"));
    unitEdit->setFrame(false);
    field_table_->setCellWidget(i, 5, unitEdit);

    field_table_->setCellWidget(i, 6, makeFieldActions(i));
    field_table_->setRowHeight(i, 36);

    name_edits_.append(nameEdit);
    offset_edits_.append(offsetSpin);
    width_edits_.append(widthSpin);
    type_edits_.append(typeCombo);
    scale_edits_.append(scaleEdit);
    unit_edits_.append(unitEdit);

    // 编辑 → 写回模型 + 实时刷新摘要（避免整表重建丢焦点，重建仅在增删/模板重载）
    // syncField 内部已 emit completeChanged，无需重复发射
    connect(nameEdit, &QLineEdit::textChanged, this, [this, i]() {
      syncField(i);
    });
    connect(offsetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this, i](int) { syncField(i); });
    connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this, i](int) { syncField(i); });
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, i](int) { syncField(i); });
    connect(scaleEdit, &QLineEdit::textChanged, this, [this, i]() {
      syncField(i);
    });
    connect(unitEdit, &QLineEdit::textChanged, this, [this, i]() {
      syncField(i);
    });
  }
}

void ProtocolFileWizard::FieldPage::syncField(int row) {
  // rebuild 期间 setValue/currentIndexChanged 会在控件列表 append 之前触发
  // 信号，此时 row 越界，直接忽略（数据由 setFields 的 fields_ 兜底）
  if (row < 0 || row >= fields_.size() || row >= name_edits_.size()) {
    return;
  }
  WizardField& f = fields_[row];
  f.name = name_edits_[row]->text().trimmed();
  f.offset = offset_edits_[row]->value();
  f.width = width_edits_[row]->value();
  f.type = type_edits_[row]->currentText();
  f.scale = scale_edits_[row]->text().trimmed();
  f.unit = unit_edits_[row]->text().trimmed();
  emit completeChanged();
}

void ProtocolFileWizard::FieldPage::addField() {
  WizardField f;
  f.name = QStringLiteral("Field_%1").arg(fields_.size() + 1);
  f.offset = 0;
  f.width = 8;
  f.type = QStringLiteral("uint8");
  f.scale = QStringLiteral("1.0");
  f.unit = QString();
  fields_.append(f);
  rebuildTable();
  const int last = fields_.size() - 1;
  field_table_->setCurrentCell(last, 0);
  if (QLineEdit* e = name_edits_.value(last)) {
    e->setFocus();
  }
  emit completeChanged();
}

void ProtocolFileWizard::FieldPage::deleteField(int row) {
  if (row < 0 || row >= fields_.size()) {
    return;
  }
  fields_.removeAt(row);
  rebuildTable();
  emit completeChanged();
}

QWidget* ProtocolFileWizard::FieldPage::makeFieldActions(int row) {
  auto* container = new QWidget(field_table_);
  auto* lay = new QHBoxLayout(container);
  lay->setContentsMargins(2, 0, 2, 0);
  lay->setSpacing(0);
  lay->addStretch();
  lay->addWidget(makeToolButton(
      QStringLiteral("trash"), [this, row]() { deleteField(row); }));
  lay->addStretch();
  return container;
}

QToolButton* ProtocolFileWizard::FieldPage::makeToolButton(
    const QString& iconName, std::function<void()> handler) {
  auto* btn = new QToolButton(field_table_);
  btn->setObjectName(QStringLiteral("fieldDelBtn"));
  btn->setIcon(
      core_ui::AppIconProvider::instance().icon(iconName));
  btn->setIconSize(QSize(18, 18));
  btn->setCursor(Qt::PointingHandCursor);
  connect(btn, &QToolButton::clicked, this,
          [handler](bool) { handler(); });
  return btn;
}

void ProtocolFileWizard::FieldPage::flashError(QLineEdit* edit) {
  edit->setProperty("error", true);
  edit->style()->unpolish(edit);
  edit->style()->polish(edit);
  edit->setFocus();
  QTimer::singleShot(1200, edit, [edit]() {
    edit->setProperty("error", false);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
  });
}

bool ProtocolFileWizard::FieldPage::validatePage() {
  if (fields_.isEmpty()) {
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, QStringLiteral("继续"),
        QStringLiteral("当前未定义任何字段，确定要继续吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
      return false;
    }
  }
  return true;
}

bool ProtocolFileWizard::FieldPage::validateAll() {
  for (int i = 0; i < fields_.size(); ++i) {
    const QString scale =
        scale_edits_.value(i) ? scale_edits_[i]->text().trimmed()
                              : fields_.at(i).scale;
    if (!scale.isEmpty() && scale != QStringLiteral("1.0")) {
      bool ok = false;
      scale.toDouble(&ok);
      if (!ok) {
        field_table_->selectRow(i);
        field_table_->setCurrentCell(i, 4);
        if (QLineEdit* e = scale_edits_.value(i)) {
          flashError(e);
        }
        LOG_WARN("PROTOCOL_UI", "字段系数非数字, 阻止创建 [field={}]",
                 fields_.at(i).name.toStdString());
        return false;
      }
    }
  }
  return true;
}

// ── 完成页 ──

class ProtocolFileWizard::SummaryPage : public WizardPage {
 public:
  explicit SummaryPage(QWidget* parent = nullptr);

  void setSummary(const QString& templateLabel, const QString& name,
                  const QString& idText, const QString& orderLabel,
                  const QString& description,
                  const QList<WizardField>& fields);
  QString stepLabel() const override { return QStringLiteral("完成"); }

 private:
  void initUi();
  QLabel* addItem(QGridLayout* grid, int row, int col, const QString& label,
                  const QString& value, int colspan = 1);

  QLabel* template_value_ = nullptr;
  QLabel* name_value_ = nullptr;
  QLabel* id_value_ = nullptr;
  QLabel* order_value_ = nullptr;
  QLabel* desc_value_ = nullptr;
  QWidget* field_tag_widget_ = nullptr;
  QGridLayout* field_tag_layout_ = nullptr;
};

ProtocolFileWizard::SummaryPage::SummaryPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
}

void ProtocolFileWizard::SummaryPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro =
      new QLabel(QStringLiteral("确认协议配置，点击「创建协议」即可生成 .eprotox 文件。"),
                 this);
  intro->setObjectName(QStringLiteral("summaryIntro"));
  layout->addWidget(intro);

  auto* gridWidget = new QWidget(this);
  gridWidget->setObjectName(QStringLiteral("summaryGrid"));
  auto* grid = new QGridLayout(gridWidget);
  grid->setContentsMargins(20, 16, 20, 16);
  grid->setHorizontalSpacing(32);
  grid->setVerticalSpacing(16);

  template_value_ = addItem(grid, 0, 0, QStringLiteral("协议模板"), QString());
  name_value_ = addItem(grid, 0, 1, QStringLiteral("帧名称"), QString());
  id_value_ = addItem(grid, 1, 0, QStringLiteral("帧 ID"), QString());
  order_value_ = addItem(grid, 1, 1, QStringLiteral("字节序"), QString());
  desc_value_ = addItem(grid, 2, 0, QStringLiteral("描述"), QString(), 2);

  // 包含字段（跨 2 列，字段 tag 流式排布）
  auto* fieldItem = new QWidget(this);
  auto* fieldLay = new QVBoxLayout(fieldItem);
  fieldLay->setContentsMargins(0, 0, 0, 0);
  fieldLay->setSpacing(6);
  auto* fieldLabel = new QLabel(QStringLiteral("包含字段"), fieldItem);
  fieldLabel->setObjectName(QStringLiteral("summaryLabel"));
  field_tag_widget_ = new QWidget(fieldItem);
  field_tag_layout_ = new QGridLayout(field_tag_widget_);
  field_tag_layout_->setContentsMargins(0, 0, 0, 0);
  field_tag_layout_->setHorizontalSpacing(6);
  field_tag_layout_->setVerticalSpacing(6);
  fieldLay->addWidget(fieldLabel);
  fieldLay->addWidget(field_tag_widget_);
  grid->addWidget(fieldItem, 3, 0, 1, 2);

  layout->addWidget(gridWidget);
  layout->addStretch();
}

void ProtocolFileWizard::SummaryPage::setSummary(
    const QString& templateLabel, const QString& name, const QString& idText,
    const QString& orderLabel, const QString& description,
    const QList<WizardField>& fields) {
  template_value_->setText(templateLabel);
  name_value_->setText(name.isEmpty() ? QStringLiteral("（未命名）") : name);
  id_value_->setText(idText.isEmpty() ? QStringLiteral("（未设置）") : idText);
  order_value_->setText(orderLabel);
  desc_value_->setText(
      description.isEmpty() ? QStringLiteral("—") : description);

  while (QLayoutItem* item = field_tag_layout_->takeAt(0)) {
    if (item->widget()) {
      item->widget()->deleteLater();
    }
    delete item;
  }
  constexpr int kMaxCols = 4;
  constexpr int kMaxShow = 8;
  if (fields.isEmpty()) {
    auto* none = new QLabel(QStringLiteral("（暂无字段）"), field_tag_widget_);
    none->setObjectName(QStringLiteral("summaryValue"));
    field_tag_layout_->addWidget(none, 0, 0, 1, kMaxCols, Qt::AlignLeft);
    return;
  }
  const int show = std::min(fields.size(), kMaxShow);
  for (int i = 0; i < show; ++i) {
    auto* tag = new QLabel(fieldTagText(fields.at(i)), field_tag_widget_);
    tag->setObjectName(QStringLiteral("fieldTag"));
    field_tag_layout_->addWidget(tag, i / kMaxCols, i % kMaxCols,
                                 Qt::AlignLeft);
  }
  if (fields.size() > kMaxShow) {
    auto* more =
        new QLabel(QStringLiteral("… 共 %1 个字段").arg(fields.size()),
                   field_tag_widget_);
    more->setObjectName(QStringLiteral("summaryValue"));
    field_tag_layout_->addWidget(more, show / kMaxCols, 0, Qt::AlignLeft);
  }
}

QLabel* ProtocolFileWizard::SummaryPage::addItem(QGridLayout* grid, int row,
                                                 int col,
                                                 const QString& label,
                                                 const QString& value,
                                                 int colspan) {
  auto* item = new QWidget(this);
  auto* lay = new QVBoxLayout(item);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(4);
  auto* labelLabel = new QLabel(label, item);
  labelLabel->setObjectName(QStringLiteral("summaryLabel"));
  auto* valueLabel = new QLabel(value, item);
  valueLabel->setObjectName(QStringLiteral("summaryValue"));
  valueLabel->setWordWrap(true);
  lay->addWidget(labelLabel);
  lay->addWidget(valueLabel);
  grid->addWidget(item, row, col, 1, colspan);
  return valueLabel;
}

// ── 向导主体 ──

ProtocolFileWizard::ProtocolFileWizard(QWidget* parent)
    : BaseWizardDialog(parent) {
  initUi();
  initSignals();
}

void ProtocolFileWizard::initUi() {
  setWindowTitle(QStringLiteral("新建协议文件"));
  setHeader(QStringLiteral("file_eproto"), QStringLiteral("新建协议文件"),
            QStringLiteral("创建 ICD 帧结构，定义信号字段与编码规则"));
  setCreateButtonText(QStringLiteral("创建协议"));

  template_page_ = new TemplatePage(this);
  info_page_ = new FrameInfoPage(this);
  field_page_ = new FieldPage(this);
  summary_page_ = new SummaryPage(this);
  addPage(template_page_);
  addPage(info_page_);
  addPage(field_page_);
  addPage(summary_page_);

  // 卡片走专用 QSS（#protoWizardCard，区别于新建项目向导的 #wizardCard）
  if (QWidget* card = findChild<QWidget*>(QStringLiteral("wizardCard"))) {
    card->setObjectName(QStringLiteral("protoWizardCard"));
  }

  loadTemplate(QStringLiteral("custom"));
}

void ProtocolFileWizard::initSignals() {
  connect(template_page_, &WizardPage::completeChanged, this,
          [this]() { onTemplateSelected(template_page_->selectedTemplateId()); });
  connect(info_page_, &WizardPage::completeChanged, this,
          &ProtocolFileWizard::updateSummary);
  connect(field_page_, &WizardPage::completeChanged, this,
          &ProtocolFileWizard::updateSummary);
  connect(this, &BaseWizardDialog::currentPageChanged, this,
          [this](int index) {
            if (index == pageCount() - 1) {
              updateSummary();
            }
          });
}

void ProtocolFileWizard::onTemplateSelected(const QString& templateId) {
  // 设计决策 #10：模板重选直接加载模板字段（覆盖当前字段，无脏确认）。
  // 但重复点击当前已选中卡片不覆盖——否则用户编辑过该模板字段后误点会
  // 丢失全部改动。
  if (templateId == template_id_) {
    return;
  }
  loadTemplate(templateId);
}

void ProtocolFileWizard::loadTemplate(const QString& templateId) {
  field_page_->setFields(templateFields(templateId));
  template_id_ = templateId;
  updateSummary();
}

void ProtocolFileWizard::updateSummary() {
  if (!summary_page_) {
    return;
  }
  summary_page_->setSummary(
      templateLabel(template_id_), info_page_->name(), info_page_->idText(),
      info_page_->orderKey() == QStringLiteral("big") ? QStringLiteral("大端")
                                                      : QStringLiteral("小端"),
      info_page_->description(), field_page_->fields());
}

QString ProtocolFileWizard::templateLabel(const QString& templateId) {
  if (templateId == QStringLiteral("a429")) {
    return QStringLiteral("ARINC 429");
  }
  if (templateId == QStringLiteral("can")) {
    return QStringLiteral("CAN 2.0B");
  }
  if (templateId == QStringLiteral("1553")) {
    return QStringLiteral("MIL-STD-1553B");
  }
  return QStringLiteral("自定义空协议");
}

QString ProtocolFileWizard::templateId() const { return template_id_; }

icd::Frame ProtocolFileWizard::resultFrame() const {
  const QString typeKey = info_page_->typeKey();
  const icd::FrameType frameType =
      typeKey == QStringLiteral("cmd")
          ? icd::FrameType::cmd
          : (typeKey == QStringLiteral("data_cmd") ? icd::FrameType::data_cmd
                                                   : icd::FrameType::data);
  const icd::ByteOrder byteOrder =
      info_page_->orderKey() == QStringLiteral("big")
          ? icd::ByteOrder::big_endian
          : icd::ByteOrder::little_endian;

  icd::Frame frame(info_page_->idValue(), info_page_->name().toStdString(),
                   info_page_->description().toStdString(), frameType,
                   byteOrder);

  for (const WizardField& f : field_page_->fields()) {
    const QString fieldName =
        f.name.trimmed().isEmpty() ? QStringLiteral("未命名") : f.name.trimmed();
    icd::NodeAttrs attrs;
    attrs.unit = f.unit.toStdString();
    attrs.scale_b = std::nullopt;  // 显式不设置 B 系数（设计决策 Y2）
    const QString scale = f.scale.trimmed();
    if (!scale.isEmpty() && scale != QStringLiteral("1.0")) {
      bool ok = false;
      const double v = scale.toDouble(&ok);
      if (ok) {
        attrs.scale_a = static_cast<float>(v);
        attrs.is_scaled = true;
      }
    }
    // 设计决策 #5：offset = 绝对位偏移，映射 Node.offset = bit/8、
    // bit_offset = bit%8
    auto node = std::make_unique<icd::Node>(
        fieldName.toStdString(), std::string(), f.offset / 8, f.offset % 8,
        f.width, valueTypeFor(f.type), icd::Tag::none, std::move(attrs));
    frame.add_root(std::move(node));
  }
  return frame;
}

bool ProtocolFileWizard::onCreateValidate() { return field_page_->validateAll(); }

void ProtocolFileWizard::confirmCancel() {
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("取消创建"),
      QStringLiteral("确定要取消创建协议吗？已填写的内容将丢失。"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    reject();
  }
}

}  // namespace etest::app
