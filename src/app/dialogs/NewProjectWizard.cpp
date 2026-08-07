#include "NewProjectWizard.h"

#include <QButtonGroup>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "ThemeManager.h"
#include "WizardTemplateCard.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "utils/switch_button.h"

using namespace etest::core::config;

namespace etest::app {

// ── Page 1: 模板选择 ──

class NewProjectWizard::TemplatePage : public WizardPage {
 public:
  explicit TemplatePage(QWidget* parent = nullptr);

  QString selectedTemplateId() const;
  QString stepLabel() const override { return QStringLiteral("模板"); }

 private:
  void initUi();
  void initSignals();
  WizardTemplateCard* addCard(QButtonGroup* group,
                              QHBoxLayout* layout,
                              const QString& templateId,
                              const QString& iconName,
                              const QString& title,
                              const QString& desc,
                              const QString& badge);

  QButtonGroup* group_ = nullptr;
};

NewProjectWizard::TemplatePage::TemplatePage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void NewProjectWizard::TemplatePage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro = new QLabel(
      QStringLiteral("选择一个项目模板作为起点，您也可以稍后调整。"), this);
  intro->setObjectName(QStringLiteral("templateIntro"));
  layout->addWidget(intro);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);

  auto* grid = new QHBoxLayout();
  grid->setSpacing(14);
  layout->addLayout(grid);

  // 默认选中「空项目」
  auto* empty =
      addCard(group_, grid, QStringLiteral("empty"),
              QStringLiteral("file_generic"), QStringLiteral("空项目"),
              QStringLiteral("手动配置拓扑、协议和用例，完全自定义"),
              QStringLiteral("推荐"));
  addCard(group_, grid, QStringLiteral("demo"), QStringLiteral("microchip"),
          QStringLiteral("Mock 演示"),
          QStringLiteral("包含模拟设备、示例协议和演示用例"),
          QStringLiteral("快速体验"));
  addCard(group_, grid, QStringLiteral("industrial"),
          QStringLiteral("industry"), QStringLiteral("工业控制"),
          QStringLiteral("预设 PLC、Modbus、CAN 总线测试套件"), QString());
  empty->setChecked(true);

  layout->addStretch();
}

void NewProjectWizard::TemplatePage::initSignals() {
  // 切换模板时刷新摘要
  connect(group_, &QButtonGroup::idClicked, this, &WizardPage::completeChanged);
}

QString NewProjectWizard::TemplatePage::selectedTemplateId() const {
  if (!group_) {
    return QStringLiteral("empty");
  }
  QAbstractButton* checked = group_->checkedButton();
  return checked ? checked->property("templateId").toString()
                 : QStringLiteral("empty");
}

WizardTemplateCard* NewProjectWizard::TemplatePage::addCard(
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
  layout->addWidget(card, 1);
  return card;
}

// ── Page 2: 项目信息 ──

class NewProjectWizard::ProjectInfoPage : public WizardPage {
 public:
  explicit ProjectInfoPage(QWidget* parent = nullptr);

  QString projectName() const;
  QString projectLocation() const;  // 绝对路径，未填写时为空
  QString projectVersion() const;
  QString projectDescription() const;
  QString stepLabel() const override { return QStringLiteral("信息"); }

  bool validatePage() override;

 private:
  void initUi();
  void initSignals();
  void onTextChanged();
  void initDefaultLocation();
  /// 项目名称是否合法（非空且不含非法字符）
  bool isNameValid() const;
  /// 字段非法时红闪输入框（error 动态属性 + 1200ms 后清除）
  void flashFieldError(QLineEdit* edit);

  QLineEdit* name_edit_ = nullptr;
  QLineEdit* location_edit_ = nullptr;
  QLineEdit* version_edit_ = nullptr;
  QLineEdit* desc_edit_ = nullptr;
};

NewProjectWizard::ProjectInfoPage::ProjectInfoPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void NewProjectWizard::ProjectInfoPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  // 项目名称
  auto* nameLabelRow = new QHBoxLayout();
  nameLabelRow->setSpacing(6);
  auto* nameLabel = new QLabel(QStringLiteral("项目名称"), this);
  nameLabel->setObjectName(QStringLiteral("fieldLabel"));
  auto* nameHint = new QLabel(QStringLiteral("· 建议使用英文或拼音"), this);
  nameHint->setObjectName(QStringLiteral("fieldHint"));
  nameLabelRow->addWidget(nameLabel);
  nameLabelRow->addWidget(nameHint);
  nameLabelRow->addStretch();
  name_edit_ = new QLineEdit(this);
  name_edit_->setObjectName(QStringLiteral("projectNameEdit"));
  name_edit_->setPlaceholderText(QStringLiteral("例如：PowerSupplyTest"));
  layout->addLayout(nameLabelRow);
  layout->addWidget(name_edit_);

  // 位置 + 版本（双列）
  auto* rowLayout = new QHBoxLayout();
  rowLayout->setSpacing(18);
  auto* locationGroup = new QVBoxLayout();
  locationGroup->setSpacing(6);
  auto* locationLabel = new QLabel(QStringLiteral("位置"), this);
  locationLabel->setObjectName(QStringLiteral("fieldLabel"));
  location_edit_ = new QLineEdit(this);
  location_edit_->setObjectName(QStringLiteral("projectLocationEdit"));
  locationGroup->addWidget(locationLabel);
  locationGroup->addWidget(location_edit_);

  auto* versionGroup = new QVBoxLayout();
  versionGroup->setSpacing(6);
  auto* versionLabel = new QLabel(QStringLiteral("版本"), this);
  versionLabel->setObjectName(QStringLiteral("fieldLabel"));
  version_edit_ = new QLineEdit(this);
  version_edit_->setObjectName(QStringLiteral("projectVersionEdit"));
  version_edit_->setText(QStringLiteral("1.0.0"));
  versionGroup->addWidget(versionLabel);
  versionGroup->addWidget(version_edit_);

  rowLayout->addLayout(locationGroup, 1);
  rowLayout->addLayout(versionGroup, 1);
  layout->addLayout(rowLayout);

  // 描述
  auto* descLabelRow = new QHBoxLayout();
  descLabelRow->setSpacing(6);
  auto* descLabel = new QLabel(QStringLiteral("描述"), this);
  descLabel->setObjectName(QStringLiteral("fieldLabel"));
  auto* descHint = new QLabel(QStringLiteral("· 可选"), this);
  descHint->setObjectName(QStringLiteral("fieldHint"));
  descLabelRow->addWidget(descLabel);
  descLabelRow->addWidget(descHint);
  descLabelRow->addStretch();
  desc_edit_ = new QLineEdit(this);
  desc_edit_->setObjectName(QStringLiteral("projectDescEdit"));
  desc_edit_->setPlaceholderText(QStringLiteral("简要说明该测试项目的用途"));
  layout->addLayout(descLabelRow);
  layout->addWidget(desc_edit_);

  layout->addStretch();

  initDefaultLocation();
}

void NewProjectWizard::ProjectInfoPage::initSignals() {
  connect(name_edit_, &QLineEdit::textChanged, this,
          &ProjectInfoPage::onTextChanged);
  connect(location_edit_, &QLineEdit::textChanged, this,
          &ProjectInfoPage::onTextChanged);
  connect(version_edit_, &QLineEdit::textChanged, this,
          &ProjectInfoPage::onTextChanged);
  connect(desc_edit_, &QLineEdit::textChanged, this,
          &ProjectInfoPage::onTextChanged);
}

QString NewProjectWizard::ProjectInfoPage::projectName() const {
  return name_edit_->text().trimmed();
}

QString NewProjectWizard::ProjectInfoPage::projectLocation() const {
  const QString text = location_edit_->text().trimmed();
  if (text.isEmpty()) {
    return QString();
  }
  return QDir(text).absolutePath();
}

QString NewProjectWizard::ProjectInfoPage::projectVersion() const {
  return version_edit_->text().trimmed();
}

QString NewProjectWizard::ProjectInfoPage::projectDescription() const {
  return desc_edit_->text().trimmed();
}

bool NewProjectWizard::ProjectInfoPage::isNameValid() const {
  const QString name = projectName();
  if (name.isEmpty()) {
    return false;
  }
  static const QString invalidChars = "\\/:*?\"<>|";
  for (const QChar& c : name) {
    if (invalidChars.contains(c)) {
      return false;
    }
  }
  return true;
}

bool NewProjectWizard::ProjectInfoPage::validatePage() {
  if (!isNameValid()) {
    flashFieldError(name_edit_);
    return false;
  }
  if (location_edit_->text().trimmed().isEmpty()) {
    flashFieldError(location_edit_);
    return false;
  }
  return true;
}

void NewProjectWizard::ProjectInfoPage::onTextChanged() {
  // 输入重新合法后立即清除残留的 error 红框，而不是等 1.2s 定时器到期
  if (isNameValid() && name_edit_->property("error").toBool()) {
    name_edit_->setProperty("error", false);
    name_edit_->style()->unpolish(name_edit_);
    name_edit_->style()->polish(name_edit_);
  }
  emit completeChanged();
}

void NewProjectWizard::ProjectInfoPage::flashFieldError(QLineEdit* edit) {
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

void NewProjectWizard::ProjectInfoPage::initDefaultLocation() {
  auto& cfg = ConfigManager::instance();
  QString lastPath = cfg.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
  if (lastPath.isEmpty()) {
    lastPath = QDir::homePath();
  } else {
    // 存储的是上次打开项目的目录（项目文件夹本身），新建时取上一级
    lastPath = QFileInfo(lastPath).dir().absolutePath();
  }
  // 派生目录不存在（例如上次路径被删除）时回退到用户主目录
  if (!QDir(lastPath).exists()) {
    lastPath = QDir::homePath();
  }
  location_edit_->setText(lastPath);
}

// ── Page 3: 高级配置 ──

class NewProjectWizard::AdvancedConfigPage : public WizardPage {
 public:
  explicit AdvancedConfigPage(QWidget* parent = nullptr);

  bool createIcdTemplate() const;
  bool enableMockDevice() const;
  bool initGitRepo() const;
  bool generateSampleTests() const;
  QString stepLabel() const override { return QStringLiteral("配置"); }

 private:
  void initUi();
  void initSignals();
  void addToggle(QVBoxLayout* layout,
                 const QString& title,
                 const QString& desc,
                 bool checked,
                 SwitchButton** outSwitch);
  void applyThemeBrushes();

  QList<SwitchButton*> toggles_;
};

NewProjectWizard::AdvancedConfigPage::AdvancedConfigPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void NewProjectWizard::AdvancedConfigPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  addToggle(layout, QStringLiteral("自动创建 ICD 协议模板"),
            QStringLiteral("生成一份示例 ICD 帧定义文件，方便快速上手"), true,
            nullptr);
  addToggle(layout, QStringLiteral("启用 Mock 设备模拟"),
            QStringLiteral("在无真实硬件时使用虚拟设备进行调试"), true,
            nullptr);
  addToggle(layout, QStringLiteral("初始化 Git 仓库"),
            QStringLiteral("自动初始化本地 Git 并生成 .gitignore"), false,
            nullptr);
  addToggle(layout, QStringLiteral("生成示例测试用例"),
            QStringLiteral("创建一组基础测试步骤示例，帮助理解框架"), true,
            nullptr);

  layout->addStretch();

  applyThemeBrushes();
}

void NewProjectWizard::AdvancedConfigPage::initSignals() {
  connect(&core_ui::ThemeManager::instance(),
          &core_ui::ThemeManager::themeChanged, this,
          [this](bool) { applyThemeBrushes(); });
}

bool NewProjectWizard::AdvancedConfigPage::createIcdTemplate() const {
  return toggles_.value(0) ? toggles_.at(0)->isChecked() : true;
}

bool NewProjectWizard::AdvancedConfigPage::enableMockDevice() const {
  return toggles_.value(1) ? toggles_.at(1)->isChecked() : true;
}

bool NewProjectWizard::AdvancedConfigPage::initGitRepo() const {
  return toggles_.value(2) ? toggles_.at(2)->isChecked() : false;
}

bool NewProjectWizard::AdvancedConfigPage::generateSampleTests() const {
  return toggles_.value(3) ? toggles_.at(3)->isChecked() : true;
}

void NewProjectWizard::AdvancedConfigPage::addToggle(QVBoxLayout* layout,
                                                     const QString& title,
                                                     const QString& desc,
                                                     bool checked,
                                                     SwitchButton** outSwitch) {
  auto* group = new QWidget(this);
  group->setObjectName(QStringLiteral("configGroup"));

  auto* row = new QHBoxLayout(group);
  row->setContentsMargins(0, 12, 0, 12);
  row->setSpacing(12);

  auto* info = new QVBoxLayout();
  info->setSpacing(2);
  auto* titleLabel = new QLabel(title, group);
  titleLabel->setObjectName(QStringLiteral("configGroupTitle"));
  auto* descLabel = new QLabel(desc, group);
  descLabel->setObjectName(QStringLiteral("configGroupDesc"));
  descLabel->setWordWrap(true);
  info->addWidget(titleLabel);
  info->addWidget(descLabel);

  auto* toggle = new SwitchButton(group);
  toggle->setObjectName(QStringLiteral("configGroupToggle"));
  toggle->setChecked(checked);
  toggles_.append(toggle);

  row->addLayout(info, 1);
  row->addWidget(toggle, 0, Qt::AlignVCenter);
  layout->addWidget(group);

  if (outSwitch) {
    *outSwitch = toggle;
  }
}

void NewProjectWizard::AdvancedConfigPage::applyThemeBrushes() {
  const auto& tm = core_ui::ThemeManager::instance();
  const QColor offColor =
      tm.isDarkTheme() ? QColor(0x3A, 0x3A, 0x4A) : QColor(0xD0, 0xD0, 0xDD);
  for (SwitchButton* toggle : toggles_) {
    toggle->setOnBackground(tm.accentColor());
    toggle->setOffBackground(offColor);
  }
}

// ── Page 4: 摘要确认 ──

class NewProjectWizard::SummaryPage : public WizardPage {
 public:
  explicit SummaryPage(QWidget* parent = nullptr);

  void setSummary(const QString& templateLabel,
                  const QString& name,
                  const QString& location,
                  const QString& version,
                  const QString& desc);
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
  QLabel* name_value_ = nullptr;
  QLabel* location_value_ = nullptr;
  QLabel* version_value_ = nullptr;
  QLabel* desc_value_ = nullptr;
};

NewProjectWizard::SummaryPage::SummaryPage(QWidget* parent)
    : WizardPage(parent) {
  // 摘要页没有信号槽连接，只需初始化 UI
  initUi();
}

void NewProjectWizard::SummaryPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro = new QLabel(
      QStringLiteral("请确认以下信息，点击「创建项目」即可完成。"), this);
  intro->setObjectName(QStringLiteral("summaryIntro"));
  layout->addWidget(intro);

  auto* gridWidget = new QWidget(this);
  gridWidget->setObjectName(QStringLiteral("summaryGrid"));
  auto* grid = new QGridLayout(gridWidget);
  grid->setContentsMargins(20, 16, 20, 16);
  grid->setHorizontalSpacing(32);
  grid->setVerticalSpacing(16);

  template_value_ = addItem(grid, 0, 0, QStringLiteral("模板"), QString());
  name_value_ = addItem(grid, 0, 1, QStringLiteral("项目名称"), QString());
  location_value_ = addItem(grid, 1, 0, QStringLiteral("位置"), QString());
  version_value_ = addItem(grid, 1, 1, QStringLiteral("版本"), QString());
  desc_value_ = addItem(grid, 2, 0, QStringLiteral("描述"), QString(), 2);

  layout->addWidget(gridWidget);
  layout->addStretch();
}

void NewProjectWizard::SummaryPage::setSummary(const QString& templateLabel,
                                               const QString& name,
                                               const QString& location,
                                               const QString& version,
                                               const QString& desc) {
  template_value_->setText(templateLabel);
  name_value_->setText(name.isEmpty() ? QStringLiteral("（未命名）") : name);
  location_value_->setText(location.isEmpty() ? QStringLiteral("（未设置）")
                                              : location);
  version_value_->setText(version.isEmpty() ? QStringLiteral("1.0.0")
                                            : version);
  desc_value_->setText(desc.isEmpty() ? QStringLiteral("（无描述）") : desc);
}

QLabel* NewProjectWizard::SummaryPage::addItem(QGridLayout* grid,
                                               int row,
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

// ── NewProjectWizard ──

namespace {
QString templateLabel(const QString& templateId) {
  if (templateId == QStringLiteral("demo")) {
    return QStringLiteral("Mock 演示");
  }
  if (templateId == QStringLiteral("industrial")) {
    return QStringLiteral("工业控制");
  }
  return QStringLiteral("空项目");
}
}  // namespace

NewProjectWizard::NewProjectWizard(QWidget* parent) : BaseWizardDialog(parent) {
  initUi();
  initSignals();
}

QString NewProjectWizard::projectName() const {
  return info_page_ ? info_page_->projectName() : QString();
}

QString NewProjectWizard::projectLocation() const {
  return info_page_ ? info_page_->projectLocation() : QString();
}

QString NewProjectWizard::projectVersion() const {
  return info_page_ ? info_page_->projectVersion() : QString();
}

QString NewProjectWizard::projectDescription() const {
  return info_page_ ? info_page_->projectDescription() : QString();
}

QString NewProjectWizard::templateId() const {
  return template_page_ ? template_page_->selectedTemplateId() : QString();
}

bool NewProjectWizard::createIcdTemplate() const {
  return config_page_ ? config_page_->createIcdTemplate() : true;
}

bool NewProjectWizard::enableMockDevice() const {
  return config_page_ ? config_page_->enableMockDevice() : true;
}

bool NewProjectWizard::initGitRepo() const {
  return config_page_ ? config_page_->initGitRepo() : false;
}

bool NewProjectWizard::generateSampleTests() const {
  return config_page_ ? config_page_->generateSampleTests() : true;
}

void NewProjectWizard::initUi() {
  setWindowTitle(QStringLiteral("新建项目"));
  setHeader(QStringLiteral("folder_plus"), QStringLiteral("新建测试项目"),
            QStringLiteral("配置您的自动化测试工程，向导将帮您快速起步"));

  template_page_ = new TemplatePage(this);
  info_page_ = new ProjectInfoPage(this);
  config_page_ = new AdvancedConfigPage(this);
  summary_page_ = new SummaryPage(this);

  addPage(template_page_);
  addPage(info_page_);
  addPage(config_page_);
  addPage(summary_page_);
}

void NewProjectWizard::initSignals() {
  // 任意输入变化或模板切换时刷新摘要
  connect(template_page_, &WizardPage::completeChanged, this,
          &NewProjectWizard::updateSummary);
  connect(info_page_, &WizardPage::completeChanged, this,
          &NewProjectWizard::updateSummary);
  // 进入完成页时兜底再刷新一次，保证摘要与最终输入一致
  connect(this, &BaseWizardDialog::currentPageChanged, this, [this](int index) {
    if (index == pageCount() - 1) {
      updateSummary();
    }
  });
}

void NewProjectWizard::updateSummary() {
  if (!summary_page_) {
    return;
  }
  summary_page_->setSummary(templateLabel(templateId()), projectName(),
                            projectLocation(), projectVersion(),
                            projectDescription());
}

}  // namespace etest::app
