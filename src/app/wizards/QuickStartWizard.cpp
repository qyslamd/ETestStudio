#include "wizards/QuickStartWizard.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "wizards/WizardTemplateCard.h"

namespace etest::app {

QuickStartWizard::QuickStartWizard(QWidget* parent) : BaseWizardDialog(parent) {
  template_page_ = new TemplatePage(this);
  info_page_ = new InfoPage(this);
  summary_page_ = new SummaryPage(this);
  addPage(template_page_);
  addPage(info_page_);
  addPage(summary_page_);

  setHeader(QStringLiteral("plus"), QStringLiteral("快速开始"),
            QStringLiteral("几步创建一个测试工程"));
  setCreateButtonText(QStringLiteral("完成"));

  // 模板/项目名变化 → 摘要联动
  auto updateSummary = [this]() {
    const QString name = info_page_->projectName();
    summary_page_->setSummary(
        QStringLiteral("模板：%1\n项目：%2")
            .arg(template_page_->selectedTemplateName())
            .arg(name.isEmpty() ? QStringLiteral("(未命名)") : name));
  };
  connect(template_page_, &WizardPage::completeChanged, this, updateSummary);
  connect(info_page_, &WizardPage::completeChanged, this, updateSummary);
  updateSummary();
}

bool QuickStartWizard::onCreateValidate() {
  // UI 空壳（D3）：完成仅关闭，不创建
  return true;
}

// ── 页 1：设备模板选择 ──

QuickStartWizard::TemplatePage::TemplatePage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
}

void QuickStartWizard::TemplatePage::initUi() {
  auto* layout = new QVBoxLayout(this);
  auto* intro = new QLabel(QStringLiteral("选择内置设备模板，快速搭建测试环境"), this);
  intro->setObjectName(QStringLiteral("templateIntro"));
  layout->addWidget(intro);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);

  auto* grid = new QHBoxLayout();
  grid->setSpacing(14);
  layout->addLayout(grid);

  auto addCard = [this, grid](const QString& title, const QString& iconName,
                              const QString& desc) {
    auto* card = new WizardTemplateCard(iconName, title, desc, QString(), this);
    group_->addButton(card);
    grid->addWidget(card);
    return card;
  };

  auto* first = addCard(QStringLiteral("综合测试台"), QStringLiteral("monitor"),
                        QStringLiteral("拓扑 + 协议 + 测试程序"));
  addCard(QStringLiteral("AD 采集"), QStringLiteral("signal"),
          QStringLiteral("模拟量采集 + 波形"));
  addCard(QStringLiteral("DA 输出"), QStringLiteral("signal"),
          QStringLiteral("模拟量输出"));
  addCard(QStringLiteral("串口 / CAN / A429"), QStringLiteral("microchip"),
          QStringLiteral("总线通信"));
  first->setChecked(true);

  layout->addStretch();
}

QString QuickStartWizard::TemplatePage::selectedTemplateName() const {
  QAbstractButton* checked = group_ ? group_->checkedButton() : nullptr;
  return checked ? checked->text() : QString();
}

// ── 页 2：项目信息 ──

QuickStartWizard::InfoPage::InfoPage(QWidget* parent) : WizardPage(parent) {
  initUi();
}

void QuickStartWizard::InfoPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  auto* intro = new QLabel(QStringLiteral("配置项目基本信息"), this);
  intro->setObjectName(QStringLiteral("templateIntro"));
  layout->addWidget(intro);

  name_edit_ = new QLineEdit(this);
  name_edit_->setPlaceholderText(QStringLiteral("项目名称"));
  layout->addWidget(name_edit_);

  location_edit_ = new QLineEdit(this);
  location_edit_->setPlaceholderText(QStringLiteral("保存位置"));
  layout->addWidget(location_edit_);

  auto* browse = new QPushButton(QStringLiteral("浏览"), this);
  layout->addWidget(browse);
  layout->addStretch();
}

QString QuickStartWizard::InfoPage::projectName() const {
  return name_edit_ ? name_edit_->text().trimmed() : QString();
}

// ── 页 3：摘要 ──

QuickStartWizard::SummaryPage::SummaryPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
}

void QuickStartWizard::SummaryPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  summary_label_ = new QLabel(QStringLiteral("准备创建测试工程"), this);
  summary_label_->setObjectName(QStringLiteral("summaryLabel"));
  summary_label_->setWordWrap(true);
  layout->addWidget(summary_label_);
  layout->addStretch();
}

void QuickStartWizard::SummaryPage::setSummary(const QString& text) {
  if (summary_label_) {
    summary_label_->setText(text);
  }
}

}  // namespace etest::app
