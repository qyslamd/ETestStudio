#include "NewFileGuideWizard.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVariant>
#include <QVBoxLayout>

#include "WizardTemplateCard.h"

namespace etest::app {

namespace {

struct FileTypeInfo {
  QString type;        // 卡片 type 属性
  QString categoryId;  // createNewFile 的 categoryId（决定目标目录）
  QString extension;   // createNewFile 的 extension（决定向导分支）
  QString baseName;    // createNewFile 的 baseName（空文件默认名）
  QString iconName;
  QString title;
  QString desc;
};

const FileTypeInfo kFileTypes[] = {
    {QStringLiteral("protocol"), QStringLiteral("protocol"),
     QStringLiteral("eprotox"), QStringLiteral("新建协议文件"),
     QStringLiteral("file_eproto"), QStringLiteral("协议文件"),
     QStringLiteral("创建 ICD 帧结构，定义信号字段与编码规则")},
    {QStringLiteral("topology"), QStringLiteral("topology"),
     QStringLiteral("etopo"), QStringLiteral("新建拓扑文件"),
     QStringLiteral("file_etopo"), QStringLiteral("拓扑文件"),
     QStringLiteral("定义设备、UUT 与信号连线的硬件拓扑")},
    {QStringLiteral("testprog"), QStringLiteral("testprog"),
     QStringLiteral("etprog"), QStringLiteral("新建测试程序"),
     QStringLiteral("testprogram"), QStringLiteral("测试程序"),
     QStringLiteral("配置测试用例与测试步骤")},
};

const FileTypeInfo* fileTypeInfo(const QString& type) {
  for (const FileTypeInfo& t : kFileTypes) {
    if (t.type == type) {
      return &t;
    }
  }
  return nullptr;
}

}  // namespace

// ── 类型选择页 ──

class NewFileGuideWizard::TypePage : public WizardPage {
 public:
  explicit TypePage(QWidget* parent = nullptr);

  QString selectedType() const;
  QString stepLabel() const override { return QStringLiteral("类型"); }

 private:
  void initUi();
  WizardTemplateCard* addCard(QButtonGroup* group, QHBoxLayout* layout,
                              const FileTypeInfo& info);

  QButtonGroup* group_ = nullptr;
};

NewFileGuideWizard::TypePage::TypePage(QWidget* parent) : WizardPage(parent) {
  initUi();
}

QString NewFileGuideWizard::TypePage::selectedType() const {
  QAbstractButton* checked = group_ ? group_->checkedButton() : nullptr;
  if (!checked) {
    return QString();
  }
  return checked->property("type").toString();
}

void NewFileGuideWizard::TypePage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro = new QLabel(
      QStringLiteral("选择要创建的文件类型，进入对应的创建向导。"), this);
  layout->addWidget(intro);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);
  auto* cards = new QHBoxLayout();
  cards->setSpacing(14);
  WizardTemplateCard* first = nullptr;
  for (const FileTypeInfo& t : kFileTypes) {
    WizardTemplateCard* card = addCard(group_, cards, t);
    if (!first) {
      first = card;
    }
  }
  layout->addLayout(cards);
  layout->addStretch();

  if (first) {
    first->setChecked(true);
  }
}

WizardTemplateCard* NewFileGuideWizard::TypePage::addCard(
    QButtonGroup* group, QHBoxLayout* layout, const FileTypeInfo& info) {
  auto* card = new WizardTemplateCard(info.iconName, info.title, info.desc,
                                      QString(), this);
  card->setProperty("type", info.type);
  group->addButton(card);
  layout->addWidget(card);
  return card;
}

// ── 向导主体 ──

NewFileGuideWizard::NewFileGuideWizard(QWidget* parent)
    : BaseWizardDialog(parent) {
  initUi();
  initSignals();
}

void NewFileGuideWizard::initUi() {
  setWindowTitle(QStringLiteral("新建文件"));
  setHeader(QStringLiteral("file_new"), QStringLiteral("新建文件"),
            QStringLiteral("选择要创建的文件类型"));
  setCreateButtonText(QStringLiteral("创建文件"));

  type_page_ = new TypePage(this);
  addPage(type_page_);
}

void NewFileGuideWizard::initSignals() {}

QString NewFileGuideWizard::selectedCategoryId() const {
  const FileTypeInfo* info = fileTypeInfo(type_page_->selectedType());
  return info ? info->categoryId : QString();
}

QString NewFileGuideWizard::selectedExtension() const {
  const FileTypeInfo* info = fileTypeInfo(type_page_->selectedType());
  return info ? info->extension : QString();
}

QString NewFileGuideWizard::selectedBaseName() const {
  const FileTypeInfo* info = fileTypeInfo(type_page_->selectedType());
  return info ? info->baseName : QString();
}

bool NewFileGuideWizard::onCreateValidate() {
  if (selectedCategoryId().isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("无法创建文件"),
                         QStringLiteral("请选择要创建的文件类型。"));
    return false;
  }
  return true;
}

}  // namespace etest::app
