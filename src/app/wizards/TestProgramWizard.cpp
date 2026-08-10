#include "TestProgramWizard.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QBrush>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include <functional>
#include <utility>

#include "AppIconProvider.h"
#include "dialogs/StepEditDialog.h"
#include "ThemeManager.h"
#include "WizardTemplateCard.h"
#include "logger/Logger.h"
#include "project/ProjectManager.h"
#include "test_program/TestProgramData.h"

using namespace etest::core::project;

namespace etest::app {

namespace {

// ── 扁平模型辅助 ──

bool isFlowStarter(const QString& cmd) {
  return cmd == QStringLiteral("LOOP") || cmd == QStringLiteral("WHILE") ||
         cmd == QStringLiteral("IF");
}

bool isFlowEnder(const QString& cmd) {
  return cmd == QStringLiteral("END_LOOP") ||
         cmd == QStringLiteral("END_WHILE") || cmd == QStringLiteral("END_IF");
}

bool isStructural(const QString& cmd) {
  return isFlowEnder(cmd) || cmd == QStringLiteral("ELSE");
}

QString endNameFor(const QString& starter) {
  if (starter == QStringLiteral("LOOP")) {
    return QStringLiteral("END_LOOP");
  }
  if (starter == QStringLiteral("WHILE")) {
    return QStringLiteral("END_WHILE");
  }
  if (starter == QStringLiteral("IF")) {
    return QStringLiteral("END_IF");
  }
  return QString();
}

WizardStep makeStructuralRow(const QString& cmd) {
  WizardStep s;
  s.cmd = cmd;
  return s;
}

WizardStep stepFromResult(const StepEditResult& r) {
  WizardStep ws;
  ws.cmd = r.cmd;
  ws.target = r.target;
  ws.value = r.value;
  ws.tolerance = r.tolerance;
  ws.timeout = r.timeout;
  ws.condition = r.condition;
  ws.loopCount = r.loopCount;
  return ws;
}

// ── 模板静态数据（转译 HTML templates）──

WizardCase templateEmptyCase() {
  WizardCase c;
  c.name = QStringLiteral("默认用例");
  return c;
}

WizardCase templateBasicCase() {
  WizardCase c;
  c.name = QStringLiteral("信号测试");
  {
    WizardStep s = stepFromResult(StepEditResult{});
    s.cmd = QStringLiteral("SET");
    s.target = QStringLiteral("电压输出");
    s.value = QStringLiteral("5.0");
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("WAIT");
    s.target = QStringLiteral("电压输出");
    s.value = QStringLiteral("5.0");
    s.condition = QStringLiteral("==");
    s.timeout = QStringLiteral("5000");
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("VERIFY");
    s.target = QStringLiteral("电压输出");
    s.value = QStringLiteral("5.0");
    s.tolerance = QStringLiteral("0.05");
    s.timeout = QStringLiteral("30000");
    c.steps.append(s);
  }
  return c;
}

WizardCase templateLoopCase() {
  WizardCase c;
  c.name = QStringLiteral("循环测试");
  {
    WizardStep s;
    s.cmd = QStringLiteral("LOOP");
    s.loopCount = QStringLiteral("5");
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("SET");
    s.value = QStringLiteral("3.3");
    s.parent = true;
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("DELAY");
    s.value = QStringLiteral("100");
    s.parent = true;
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("VERIFY");
    s.value = QStringLiteral("3.3");
    s.parent = true;
    c.steps.append(s);
  }
  c.steps.append(makeStructuralRow(QStringLiteral("END_LOOP")));
  return c;
}

WizardCase templateInitCase() {
  WizardCase c;
  c.name = QStringLiteral("初始化");
  {
    WizardStep s;
    s.cmd = QStringLiteral("SET");
    s.target = QStringLiteral("电压输出");
    s.value = QStringLiteral("0");
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("SET");
    s.target = QStringLiteral("电流输出");
    s.value = QStringLiteral("0");
    c.steps.append(s);
  }
  return c;
}

WizardCase templateMainCase() {
  WizardCase c;
  c.name = QStringLiteral("主测试");
  {
    WizardStep s;
    s.cmd = QStringLiteral("SET");
    s.target = QStringLiteral("电压输出");
    s.value = QStringLiteral("5.0");
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("VERIFY");
    s.target = QStringLiteral("电压输出");
    s.value = QStringLiteral("5.0");
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("IF");
    s.target = QStringLiteral("电压输出");
    s.condition = QStringLiteral(">=");
    s.value = QStringLiteral("5.0");
    c.steps.append(s);
  }
  {
    WizardStep s;
    s.cmd = QStringLiteral("SET");
    s.target = QStringLiteral("状态标志");
    s.value = QStringLiteral("1");
    s.parent = true;
    c.steps.append(s);
  }
  c.steps.append(makeStructuralRow(QStringLiteral("ELSE")));
  {
    WizardStep s;
    s.cmd = QStringLiteral("SET");
    s.target = QStringLiteral("状态标志");
    s.value = QStringLiteral("0");
    s.parent = true;
    c.steps.append(s);
  }
  c.steps.append(makeStructuralRow(QStringLiteral("END_IF")));
  return c;
}

QVector<WizardCase> templateCases(const QString& id) {
  if (id == QStringLiteral("basic")) {
    return {templateBasicCase()};
  }
  if (id == QStringLiteral("loop")) {
    return {templateLoopCase()};
  }
  if (id == QStringLiteral("composite")) {
    return {templateInitCase(), templateMainCase()};
  }
  return {templateEmptyCase()};
}

// ── 字段映射辅助 ──

QVariant parseValueText(const QString& text) {
  bool ok = false;
  const double d = text.trimmed().toDouble(&ok);
  return ok ? QVariant(d) : QVariant(text);
}

bool toleranceEnabled(const QString& cmd) {
  return cmd == QStringLiteral("SET") || cmd == QStringLiteral("CHECK") ||
         cmd == QStringLiteral("VERIFY");
}

// 叶子步骤 → TestStepData（扁平字段映射，见设计文档）
TestStepData leafToStep(const WizardStep& ws) {
  TestStepData s;
  s.cmd = ws.cmd;
  s.target = ws.target;
  s.value = parseValueText(ws.value);
  if (ws.cmd == QStringLiteral("CHECK")) {
    s.timeoutMs = 0;  // 同步检查：显式置 0
  } else if (ws.cmd == QStringLiteral("VERIFY") ||
             ws.cmd == QStringLiteral("WAIT") ||
             ws.cmd == QStringLiteral("WHILE")) {
    s.timeoutMs = ws.timeout.trimmed().toInt(nullptr, 10);
    if (s.timeoutMs <= 0) {
      s.timeoutMs = 5000;  // 缺省与编辑器默认一致
    }
  } else {
    s.timeoutMs = 0;
  }
  if (ws.cmd == QStringLiteral("DELAY")) {
    s.delayMs = ws.value.trimmed().toInt(nullptr, 10);
  }
  if (toleranceEnabled(ws.cmd)) {
    bool vOk = false;
    bool tOk = false;
    const double v = ws.value.trimmed().toDouble(&vOk);
    const double t = ws.tolerance.trimmed().toDouble(&tOk);
    if (vOk && tOk) {
      s.tolerance.min = v - t;
      s.tolerance.max = v + t;
      s.tolerance.enabled = true;
    }
  }
  if (ws.cmd == QStringLiteral("WAIT") || ws.cmd == QStringLiteral("WHILE") ||
      ws.cmd == QStringLiteral("IF")) {
    s.condition.target = ws.target;
    s.condition.op = ws.condition;
    s.condition.value = parseValueText(ws.value);
  }
  return s;
}

// 扁平 → 嵌套翻译（含结构防御：LOOP/WHILE 空体、IF THEN 为空、块内嵌套、END
// 不配对、结构行出现在顶层均视为结构错误）。失败时 LOG_ERROR + error 描述，由
// 调用方弹窗。
bool buildNestedSteps(const QVector<WizardStep>& flat,
                      QVector<TestStepData>* out, QString* error) {
  auto fail = [error](const QString& msg) {
    if (error) {
      *error = msg;
    }
    LOG_ERROR("TESTPROG_UI", "测试程序向导翻译失败: {}", msg.toStdString());
    return false;
  };
  QVector<TestStepData> result;
  const int n = flat.size();
  int i = 0;
  while (i < n) {
    const WizardStep& ws = flat[i];
    const QString& cmd = ws.cmd;
    if (isStructural(cmd)) {
      return fail(QStringLiteral("结构符号 %1 出现在顶层").arg(cmd));
    }
    if (isFlowStarter(cmd)) {
      const QString endName = endNameFor(cmd);
      TestStepData flow;
      flow.cmd = cmd;
      if (cmd == QStringLiteral("LOOP") || cmd == QStringLiteral("WHILE")) {
        flow.loopCount = ws.loopCount.trimmed().toInt(nullptr, 10);
        if (flow.loopCount <= 0) {
          flow.loopCount = 1;
        }
      }
      if (cmd == QStringLiteral("WHILE") || cmd == QStringLiteral("IF")) {
        flow.condition.target = ws.target;
        flow.condition.op = ws.condition;
        flow.condition.value = parseValueText(ws.value);
      }
      if (cmd == QStringLiteral("WHILE")) {
        // WHILE 走流分支，不经过 leafToStep，需显式映射超时字段
        flow.timeoutMs = ws.timeout.trimmed().toInt(nullptr, 10);
        if (flow.timeoutMs <= 0) {
          flow.timeoutMs = 5000;  // 缺省与编辑器默认一致
        }
      }
      // 收集 body 至配对 END
      QVector<TestStepData> body;
      QVector<TestStepData> elseBody;
      bool inElse = false;
      int j = i + 1;
      for (; j < n; ++j) {
        const WizardStep& b = flat[j];
        if (b.cmd == endName) {
          break;
        }
        if (isFlowStarter(b.cmd)) {
          return fail(QStringLiteral("%1 块体内嵌套 %2").arg(cmd, b.cmd));
        }
        if (isFlowEnder(b.cmd)) {
          return fail(QStringLiteral("%1 缺少配对 %2").arg(cmd, endName));
        }
        if (b.cmd == QStringLiteral("ELSE")) {
          if (cmd != QStringLiteral("IF")) {
            return fail(QStringLiteral("%1 块体内不允许出现 ELSE").arg(cmd));
          }
          if (inElse) {
            return fail(QStringLiteral("IF 块体内出现多个 ELSE"));
          }
          inElse = true;
          continue;
        }
        (inElse ? elseBody : body).append(leafToStep(b));
      }
      if (j >= n) {
        return fail(QStringLiteral("%1 缺少配对 %2").arg(cmd, endName));
      }
      if (body.isEmpty()) {
        return fail(QStringLiteral("%1 的块体/THEN 分支为空").arg(cmd));
      }
      flow.subSteps = body;
      flow.elseSubSteps = elseBody;
      result.append(flow);
      i = j + 1;
      continue;
    }
    // 叶子
    result.append(leafToStep(ws));
    ++i;
  }
  *out = result;
  return true;
}

// ── 摘要辅助 ──

QString summarizeCase(const WizardCase& c) {
  QStringList cmds;
  for (const auto& s : c.steps) {
    if (!isStructural(s.cmd)) {
      cmds << s.cmd;
    }
  }
  if (cmds.isEmpty()) {
    return c.name + QStringLiteral(": （无步骤）");
  }
  const int shown = qMin(4, cmds.size());
  QString preview = cmds.mid(0, shown).join(QStringLiteral(" → "));
  if (cmds.size() > 4) {
    preview += QStringLiteral(" …");
  }
  return c.name + QStringLiteral(": ") + preview;
}

QColor bodyRowGray() {
  return QColor(128, 128, 128, 24);
}

}  // namespace

// ── 命令徽章 ──

// 命令徽章：程序化绘制（非样式表），按命令语义色亮/暗双套。
class TestProgramWizard::StepCommandBadge : public QWidget {
 public:
  explicit StepCommandBadge(const QString& cmd, QWidget* parent = nullptr)
      : QWidget(parent), cmd_(cmd) {
    setFixedHeight(24);
    QFontMetrics fm(font());
    setFixedWidth(fm.horizontalAdvance(displayText()) + 20);
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QColor bg;
    QColor fg;
    paletteFor(&bg, &fg);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect(), 5, 5);
    p.setPen(fg);
    QFont f = font();
    f.setPointSize(qMax(f.pointSize() - 1, 8));
    f.setBold(true);
    p.setFont(f);
    p.drawText(rect(), Qt::AlignCenter, displayText());
  }

 private:
  QString displayText() const {
    if (cmd_ == QStringLiteral("END_LOOP")) {
      return QStringLiteral("END LOOP");
    }
    if (cmd_ == QStringLiteral("END_WHILE")) {
      return QStringLiteral("END WHILE");
    }
    if (cmd_ == QStringLiteral("END_IF")) {
      return QStringLiteral("END IF");
    }
    return cmd_;
  }

  // 8 命令 → 4 语义角色，亮/暗各一对（浅底深字 / 深底浅字）
  void paletteFor(QColor* bg, QColor* fg) const {
    struct Role {
      QColor lightBg;
      QColor lightFg;
      QColor darkBg;
      QColor darkFg;
    };
    Role role;
    if (cmd_ == QStringLiteral("SET")) {
      role = {QColor(0xD4, 0xED, 0xDA), QColor(0x15, 0x57, 0x24),
              QColor(0x1E, 0x3A, 0x2A), QColor(0x7F, 0xE0, 0xA6)};
    } else if (cmd_ == QStringLiteral("VERIFY")) {
      role = {QColor(0xFF, 0xF3, 0xCD), QColor(0x85, 0x64, 0x04),
              QColor(0x3A, 0x2E, 0x0E), QColor(0xE8, 0xC0, 0x68)};
    } else if (cmd_ == QStringLiteral("WAIT") ||
               cmd_ == QStringLiteral("CHECK")) {
      role = {QColor(0xF8, 0xD7, 0xDA), QColor(0x72, 0x1C, 0x24),
              QColor(0x3A, 0x1A, 0x1E), QColor(0xF0, 0x9B, 0x9B)};
    } else {
      role = {QColor(0xE2, 0xE3, 0xE5), QColor(0x38, 0x3D, 0x41),
              QColor(0x2E, 0x30, 0x33), QColor(0xC8, 0xCB, 0xD0)};
    }
    const bool dark = core_ui::ThemeManager::instance().isDarkTheme();
    *bg = dark ? role.darkBg : role.lightBg;
    *fg = dark ? role.darkFg : role.lightFg;
  }

  QString cmd_;
};

// ── 模板选择页 ──

class TestProgramWizard::TemplatePage : public WizardPage {
 public:
  explicit TemplatePage(QWidget* parent = nullptr);

  QString selectedTemplateId() const;
  /// 程序化恢复选中（取消重载确认时回退卡片勾选；不触发 completeChanged）
  void setSelectedTemplateId(const QString& templateId);
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

TestProgramWizard::TemplatePage::TemplatePage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void TestProgramWizard::TemplatePage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro =
      new QLabel(QStringLiteral("选择一个模板作为起点，选中后将载入对应的示例用例与步骤。"),
                 this);
  intro->setObjectName(QStringLiteral("tpInfoIntro"));
  layout->addWidget(intro);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);

  auto* grid = new QHBoxLayout();
  grid->setSpacing(14);
  layout->addLayout(grid);

  auto* empty = addCard(group_, grid, QStringLiteral("empty"),
                        QStringLiteral("file_generic"), QStringLiteral("空程序"),
                        QStringLiteral("从零开始编写测试步骤"),
                        QStringLiteral("推荐"));
  addCard(group_, grid, QStringLiteral("basic"), QStringLiteral("bolt"),
          QStringLiteral("基础信号测试"),
          QStringLiteral("SET → WAIT → VERIFY 标准流程"), QString());
  addCard(group_, grid, QStringLiteral("loop"), QStringLiteral("sync"),
          QStringLiteral("循环控制测试"),
          QStringLiteral("包含 LOOP 循环与条件判断"), QString());
  addCard(group_, grid, QStringLiteral("composite"),
          QStringLiteral("layer_group"), QStringLiteral("复合场景测试"),
          QStringLiteral("多用例 + IF/WHILE 分支"), QString());
  empty->setChecked(true);

  layout->addStretch();
}

void TestProgramWizard::TemplatePage::initSignals() {
  // 选中变化走基类 completeChanged（向导侧读取 selectedTemplateId()）
  connect(group_,
          QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this,
          [this](QAbstractButton* /*button*/) { emit completeChanged(); });
}

QString TestProgramWizard::TemplatePage::selectedTemplateId() const {
  if (!group_) {
    return QStringLiteral("empty");
  }
  QAbstractButton* checked = group_->checkedButton();
  return checked ? checked->property("templateId").toString()
                 : QStringLiteral("empty");
}

void TestProgramWizard::TemplatePage::setSelectedTemplateId(
    const QString& templateId) {
  if (!group_) {
    return;
  }
  const QList<QAbstractButton*> buttons = group_->buttons();
  for (QAbstractButton* b : buttons) {
    if (b->property("templateId").toString() == templateId) {
      b->setChecked(true);
      return;
    }
  }
}

WizardTemplateCard* TestProgramWizard::TemplatePage::addCard(
    QButtonGroup* group, QHBoxLayout* layout, const QString& templateId,
    const QString& iconName, const QString& title, const QString& desc,
    const QString& badge) {
  auto* card = new WizardTemplateCard(iconName, title, desc, badge, this);
  card->setProperty("templateId", templateId);
  group->addButton(card);
  layout->addWidget(card, 1);
  return card;
}

// ── 信息页 ──

class TestProgramWizard::ProgramInfoPage : public WizardPage {
 public:
  explicit ProgramInfoPage(QWidget* parent = nullptr);

  QString name() const;
  QString version() const;
  QString author() const;
  QString description() const;
  QString precondition() const;
  QString stepLabel() const override { return QStringLiteral("信息"); }
  bool validatePage() override;

 private:
  void initUi();
  void initSignals();
  void onTextChanged();
  bool isNameValid() const;
  /// 重名预检：cases/<name>.etprog 是否已存在（项目未打开时视为可用）
  bool isNameAvailable() const;
  void flashFieldError(QLineEdit* edit);

  QLineEdit* name_edit_ = nullptr;
  QLineEdit* version_edit_ = nullptr;
  QLineEdit* author_edit_ = nullptr;
  QTextEdit* desc_edit_ = nullptr;
  QTextEdit* precond_edit_ = nullptr;
  QLabel* name_hint_ = nullptr;
};

TestProgramWizard::ProgramInfoPage::ProgramInfoPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
  initSignals();
}

void TestProgramWizard::ProgramInfoPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);

  // 程序名称
  auto* nameLabelRow = new QHBoxLayout();
  nameLabelRow->setSpacing(6);
  auto* nameLabel = new QLabel(QStringLiteral("程序名称"), this);
  nameLabel->setObjectName(QStringLiteral("tpFieldLabel"));
  auto* nameHint = new QLabel(QStringLiteral("· 建议使用英文或拼音"), this);
  nameHint->setObjectName(QStringLiteral("tpFieldLabel"));
  nameLabelRow->addWidget(nameLabel);
  nameLabelRow->addWidget(nameHint);
  nameLabelRow->addStretch();
  layout->addLayout(nameLabelRow);

  name_edit_ = new QLineEdit(this);
  name_edit_->setObjectName(QStringLiteral("progNameEdit"));
  name_edit_->setPlaceholderText(QStringLiteral("例如：PowerSupplyTest"));
  layout->addWidget(name_edit_);

  // 行内重名提示
  name_hint_ = new QLabel(QStringLiteral("该名称已被使用，请更换"), this);
  name_hint_->setObjectName(QStringLiteral("tpNameHint"));
  QPalette hintPal = name_hint_->palette();
  hintPal.setColor(QPalette::WindowText, QColor(0xD1, 0x34, 0x38));
  name_hint_->setPalette(hintPal);
  name_hint_->hide();
  layout->addWidget(name_hint_);

  // 版本 + 作者（双列）
  auto* rowLayout = new QHBoxLayout();
  rowLayout->setSpacing(18);

  auto* versionGroup = new QVBoxLayout();
  versionGroup->setSpacing(6);
  auto* versionLabel = new QLabel(QStringLiteral("版本"), this);
  versionLabel->setObjectName(QStringLiteral("tpFieldLabel"));
  version_edit_ = new QLineEdit(this);
  version_edit_->setObjectName(QStringLiteral("progVersionEdit"));
  version_edit_->setText(QStringLiteral("1.0.0"));
  versionGroup->addWidget(versionLabel);
  versionGroup->addWidget(version_edit_);

  auto* authorGroup = new QVBoxLayout();
  authorGroup->setSpacing(6);
  auto* authorLabel = new QLabel(QStringLiteral("作者"), this);
  authorLabel->setObjectName(QStringLiteral("tpFieldLabel"));
  author_edit_ = new QLineEdit(this);
  author_edit_->setObjectName(QStringLiteral("progAuthorEdit"));
  author_edit_->setPlaceholderText(QStringLiteral("可选"));
  authorGroup->addWidget(authorLabel);
  authorGroup->addWidget(author_edit_);

  rowLayout->addLayout(versionGroup, 1);
  rowLayout->addLayout(authorGroup, 1);
  layout->addLayout(rowLayout);

  // 程序描述
  auto* descLabel = new QLabel(QStringLiteral("程序描述"), this);
  descLabel->setObjectName(QStringLiteral("tpFieldLabel"));
  layout->addWidget(descLabel);
  desc_edit_ = new QTextEdit(this);
  desc_edit_->setObjectName(QStringLiteral("progDescEdit"));
  desc_edit_->setFixedHeight(72);
  desc_edit_->setPlaceholderText(QStringLiteral("简要说明测试程序用途（可选）"));
  layout->addWidget(desc_edit_);

  // 前置条件
  auto* precondLabel = new QLabel(QStringLiteral("前置条件"), this);
  precondLabel->setObjectName(QStringLiteral("tpFieldLabel"));
  layout->addWidget(precondLabel);
  precond_edit_ = new QTextEdit(this);
  precond_edit_->setObjectName(QStringLiteral("progPrecondEdit"));
  precond_edit_->setFixedHeight(56);
  precond_edit_->setPlaceholderText(QStringLiteral("执行前需要满足的条件（可选）"));
  layout->addWidget(precond_edit_);

  layout->addStretch();
}

void TestProgramWizard::ProgramInfoPage::initSignals() {
  connect(name_edit_, &QLineEdit::textChanged, this,
          &ProgramInfoPage::onTextChanged);
  connect(version_edit_, &QLineEdit::textChanged, this,
          &ProgramInfoPage::onTextChanged);
  connect(author_edit_, &QLineEdit::textChanged, this,
          &ProgramInfoPage::onTextChanged);
  connect(desc_edit_, &QTextEdit::textChanged, this,
          &ProgramInfoPage::onTextChanged);
  connect(precond_edit_, &QTextEdit::textChanged, this,
          &ProgramInfoPage::onTextChanged);
}

void TestProgramWizard::ProgramInfoPage::onTextChanged() {
  if (isNameValid() && name_edit_->property("error").toBool()) {
    name_edit_->setProperty("error", false);
    name_edit_->style()->unpolish(name_edit_);
    name_edit_->style()->polish(name_edit_);
  }
  const bool valid = isNameValid();
  const bool available = valid ? isNameAvailable() : true;
  name_hint_->setVisible(valid && !available);
  emit completeChanged();
}

bool TestProgramWizard::ProgramInfoPage::validatePage() {
  if (!isNameValid()) {
    flashFieldError(name_edit_);
    return false;
  }
  if (!isNameAvailable()) {
    flashFieldError(name_edit_);
    return false;
  }
  return true;
}

bool TestProgramWizard::ProgramInfoPage::isNameValid() const {
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

bool TestProgramWizard::ProgramInfoPage::isNameAvailable() const {
  ProjectManager& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    return true;
  }
  const QString root = pm.currentProjectRoot();
  if (root.isEmpty()) {
    return true;
  }
  QDir casesDir(QDir(root).absoluteFilePath(QStringLiteral("cases")));
  if (!casesDir.exists()) {
    return true;
  }
  return !QFile::exists(casesDir.absoluteFilePath(this->name() + QStringLiteral(".etprog")));
}

void TestProgramWizard::ProgramInfoPage::flashFieldError(QLineEdit* edit) {
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

QString TestProgramWizard::ProgramInfoPage::name() const {
  return name_edit_->text().trimmed();
}

QString TestProgramWizard::ProgramInfoPage::version() const {
  return version_edit_->text().trimmed();
}

QString TestProgramWizard::ProgramInfoPage::author() const {
  return author_edit_->text().trimmed();
}

QString TestProgramWizard::ProgramInfoPage::description() const {
  return desc_edit_->toPlainText().trimmed();
}

QString TestProgramWizard::ProgramInfoPage::precondition() const {
  return precond_edit_->toPlainText().trimmed();
}

// ── 用例与步骤页 ──

class TestProgramWizard::CasesPage : public WizardPage {
 public:
  explicit CasesPage(QWidget* parent = nullptr);

  QVector<WizardCase> cases() const { return cases_; }
  int totalStepCount() const;
  QString stepPreview() const;
  bool isDirty() const { return dirty_; }
  void setCases(const QVector<WizardCase>& cases);
  QString stepLabel() const override { return QStringLiteral("用例"); }
  bool validatePage() override;

 private:
  void initUi();
  void initSignals();

  // 用例 tab
  void addCase();
  void removeCaseAt(int index);
  void onCaseTabChanged(int index);
  QString nextCaseName() const;

  // 步骤操作
  void onAddStep();
  void onEditStep(int row);
  void onDeleteStep(int row);
  void onMoveStep(int row, bool up);
  void onClearSteps();
  void rebuildStepTable();
  QWidget* makeActions(int row);
  QToolButton* makeToolButton(const QString& iconName,
                              std::function<void()> handler);
  void setCellText(int row, int col, const QString& text);
  void selectRow(int row);

  // 扁平模型辅助
  const QVector<WizardStep>& currentSteps() const;
  QVector<WizardStep>& currentStepsMut();
  const WizardStep& stepAt(int row) const;
  /// 插入位置与块体内标志：afterRow>=0 时在 afterRow 之后插入，否则表尾
  void insertionContext(int afterRow, int* insertPos, bool* insideBlock) const;
  /// 从 startRow 起（含）向后找配对 END 行索引，找不到返回 -1
  int findMatchingEnd(int startRow) const;
  int findElseRow(int ifRow) const;
  bool hasElse(int ifRow) const;
  /// 被删 body 叶子所在块的 THEN/块体在删除后是否为空（空需确认）
  bool removingEmptiesBlock(int row) const;
  int enclosingBlockStart(int bodyRow) const;
  /// 删除起始及其块体（含 END/ELSE/body），删除前弹确认
  void deleteFlowBlock(int startRow);
  /// 删除 IF 的 ELSE 行及其后分支步骤（调用方已确认）
  void removeElseBranch(int ifRow);
  /// 块整体上移/下移一个顶层元素；ELSE 不可移动，非法相邻返回 false
  bool moveBlock(int startRow, bool up);
  /// 控制流→叶子：删除配对 END/ELSE，body 上提为顶层叶子
  void hoistFlowToLeaf(int row, const StepEditResult& r);
  /// 编辑应用（含操作语义与确认）；返回 false 表示用户拒绝（未修改）
  bool applyEdit(int row, const QString& oldCmd, const StepEditResult& r);
  /// 更新字段（保持结构标志与 parent 不变）
  static void updateStepFields(WizardStep& s, const StepEditResult& r);
  void setDirty();

  QTabWidget* tab_widget_ = nullptr;
  QStackedWidget* table_stack_ = nullptr;
  QTableWidget* step_table_ = nullptr;
  QLabel* empty_label_ = nullptr;
  QPushButton* add_step_btn_ = nullptr;
  QPushButton* clear_btn_ = nullptr;
  QPushButton* add_case_btn_ = nullptr;
  QVector<WizardCase> cases_;
  bool dirty_ = false;
};

TestProgramWizard::CasesPage::CasesPage(QWidget* parent) : WizardPage(parent) {
  cases_ = templateCases(QStringLiteral("empty"));
  initUi();
  initSignals();
}

void TestProgramWizard::CasesPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  // 步骤工具栏
  auto* toolbar = new QWidget(this);
  auto* toolbarLay = new QHBoxLayout(toolbar);
  toolbarLay->setContentsMargins(0, 0, 0, 0);
  toolbarLay->setSpacing(8);
  add_step_btn_ = new QPushButton(this);
  add_step_btn_->setObjectName(QStringLiteral("stepToolbarBtn"));
  add_step_btn_->setIcon(core_ui::AppIconProvider::instance().icon(
      QStringLiteral("testprog_add_step")));
  add_step_btn_->setText(QStringLiteral("添加步骤"));
  clear_btn_ = new QPushButton(this);
  clear_btn_->setObjectName(QStringLiteral("stepToolbarBtn"));
  clear_btn_->setIcon(core_ui::AppIconProvider::instance().icon(
      QStringLiteral("testprog_delete_all")));
  clear_btn_->setText(QStringLiteral("清空"));
  toolbarLay->addWidget(add_step_btn_);
  toolbarLay->addWidget(clear_btn_);
  toolbarLay->addStretch();
  layout->addWidget(toolbar);

  // 用例 tab（角部「新增用例」）
  tab_widget_ = new QTabWidget(this);
  tab_widget_->setObjectName(QStringLiteral("casesTabWidget"));
  tab_widget_->setTabsClosable(true);
  tab_widget_->setMovable(false);
  add_case_btn_ = new QPushButton(this);
  add_case_btn_->setObjectName(QStringLiteral("addCaseBtn"));
  add_case_btn_->setIcon(core_ui::AppIconProvider::instance().icon(
      QStringLiteral("testprog_add_case")));
  add_case_btn_->setText(QStringLiteral("新增用例"));
  add_case_btn_->setCursor(Qt::PointingHandCursor);
  tab_widget_->setCornerWidget(add_case_btn_, Qt::TopRightCorner);
  layout->addWidget(tab_widget_);

  // 步骤表格 / 空态
  table_stack_ = new QStackedWidget(this);
  step_table_ = new QTableWidget(table_stack_);
  step_table_->setObjectName(QStringLiteral("stepTable"));
  step_table_->setColumnCount(7);
  step_table_->setHorizontalHeaderLabels(
      {QStringLiteral("#"), QStringLiteral("命令"), QStringLiteral("目标信号"),
       QStringLiteral("值"), QStringLiteral("容差"), QStringLiteral("超时(ms)"),
       QStringLiteral("操作")});
  step_table_->verticalHeader()->setVisible(false);
  step_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  step_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  step_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  step_table_->setShowGrid(false);
  step_table_->setFrameShape(QFrame::NoFrame);
  step_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  step_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  step_table_->setColumnWidth(0, 36);
  step_table_->setColumnWidth(1, 110);
  step_table_->setColumnWidth(3, 100);
  step_table_->setColumnWidth(4, 70);
  step_table_->setColumnWidth(5, 90);
  step_table_->setColumnWidth(6, 150);
  table_stack_->addWidget(step_table_);

  empty_label_ = new QLabel(
      QStringLiteral("暂无步骤，点击「添加步骤」开始定义测试逻辑。"), table_stack_);
  empty_label_->setAlignment(Qt::AlignCenter);
  table_stack_->addWidget(empty_label_);

  layout->addWidget(table_stack_, 1);
}

void TestProgramWizard::CasesPage::initSignals() {
  connect(add_step_btn_, &QPushButton::clicked, this, &CasesPage::onAddStep);
  connect(clear_btn_, &QPushButton::clicked, this, &CasesPage::onClearSteps);
  connect(add_case_btn_, &QPushButton::clicked, this, &CasesPage::addCase);
  connect(tab_widget_, &QTabWidget::currentChanged, this,
          &CasesPage::onCaseTabChanged);
  connect(tab_widget_, &QTabWidget::tabCloseRequested, this,
          &CasesPage::removeCaseAt);
}

const QVector<WizardStep>& TestProgramWizard::CasesPage::currentSteps() const {
  const int idx = qBound(0, tab_widget_->currentIndex(), cases_.size() - 1);
  return cases_.at(idx).steps;
}

QVector<WizardStep>& TestProgramWizard::CasesPage::currentStepsMut() {
  const int idx = qBound(0, tab_widget_->currentIndex(), cases_.size() - 1);
  return cases_[idx].steps;
}

const WizardStep& TestProgramWizard::CasesPage::stepAt(int row) const {
  return currentSteps().at(row);
}

void TestProgramWizard::CasesPage::setCases(const QVector<WizardCase>& cases) {
  cases_ = cases;
  dirty_ = false;
  while (tab_widget_->count() > 0) {
    tab_widget_->removeTab(0);
  }
  for (const WizardCase& c : cases_) {
    tab_widget_->addTab(new QWidget(tab_widget_), c.name);
  }
  if (tab_widget_->count() > 0) {
    tab_widget_->setCurrentIndex(0);
  }
  rebuildStepTable();
}

void TestProgramWizard::CasesPage::addCase() {
  WizardCase c;
  c.name = nextCaseName();
  cases_.append(c);
  tab_widget_->addTab(new QWidget(tab_widget_), c.name);
  tab_widget_->setCurrentIndex(tab_widget_->count() - 1);
  setDirty();
  rebuildStepTable();
  emit completeChanged();
}

void TestProgramWizard::CasesPage::removeCaseAt(int index) {
  if (cases_.size() <= 1) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("至少保留 1 个用例"));
    return;
  }
  if (index < 0 || index >= cases_.size()) {
    return;
  }
  cases_.removeAt(index);
  tab_widget_->removeTab(index);
  setDirty();
  rebuildStepTable();
  emit completeChanged();
}

void TestProgramWizard::CasesPage::onCaseTabChanged(int index) {
  Q_UNUSED(index);
  rebuildStepTable();
}

QString TestProgramWizard::CasesPage::nextCaseName() const {
  for (int n = 1;; ++n) {
    const QString name = QStringLiteral("用例 %1").arg(n);
    bool used = false;
    for (const WizardCase& c : cases_) {
      if (c.name == name) {
        used = true;
        break;
      }
    }
    if (!used) {
      return name;
    }
  }
}

void TestProgramWizard::CasesPage::rebuildStepTable() {
  step_table_->setRowCount(0);
  const QVector<WizardStep>& steps = currentSteps();
  if (steps.isEmpty()) {
    table_stack_->setCurrentWidget(empty_label_);
    return;
  }
  table_stack_->setCurrentWidget(step_table_);

  step_table_->setRowCount(steps.size());
  const QBrush gray = QBrush(bodyRowGray());
  for (int i = 0; i < steps.size(); ++i) {
    const WizardStep& ws = steps.at(i);
    setCellText(i, 0, QString::number(i + 1));
    step_table_->setCellWidget(i, 1, new StepCommandBadge(ws.cmd, step_table_));
    QString target = ws.target;
    if (ws.parent) {
      target.prepend(QStringLiteral("↳ "));
    }
    setCellText(i, 2, target);
    setCellText(i, 3, ws.value);
    setCellText(i, 4, ws.tolerance);
    setCellText(i, 5, ws.timeout);
    step_table_->setCellWidget(i, 6, makeActions(i));
    step_table_->setRowHeight(i, 30);
    if (ws.parent) {
      for (int c = 0; c < 6; ++c) {
        if (QTableWidgetItem* item = step_table_->item(i, c)) {
          item->setBackground(gray);
        }
      }
    }
  }
}

void TestProgramWizard::CasesPage::setCellText(int row, int col,
                                               const QString& text) {
  QTableWidgetItem* item = step_table_->item(row, col);
  if (!item) {
    item = new QTableWidgetItem(text);
    step_table_->setItem(row, col, item);
  } else {
    item->setText(text);
  }
}

void TestProgramWizard::CasesPage::selectRow(int row) {
  if (row >= 0 && row < step_table_->rowCount()) {
    step_table_->setCurrentCell(row, 0);
  }
}

QWidget* TestProgramWizard::CasesPage::makeActions(int row) {
  const WizardStep& ws = stepAt(row);
  const bool structural = isStructural(ws.cmd);
  const bool elseRow = (ws.cmd == QStringLiteral("ELSE"));
  auto* container = new QWidget(step_table_);
  auto* lay = new QHBoxLayout(container);
  lay->setContentsMargins(2, 0, 2, 0);
  lay->setSpacing(2);

  if (!structural) {
    lay->addWidget(makeToolButton(
        QStringLiteral("pencil"), [this, row]() { onEditStep(row); }));
  }
  // END 行上移/下移 = 整块移动；ELSE 不可移动（按钮置灰）
  auto* up = makeToolButton(QStringLiteral("testprog_move_up"),
                            [this, row]() { onMoveStep(row, true); });
  auto* down = makeToolButton(QStringLiteral("testprog_move_down"),
                              [this, row]() { onMoveStep(row, false); });
  up->setEnabled(!elseRow);
  down->setEnabled(!elseRow);
  lay->addWidget(up);
  lay->addWidget(down);

  if (!structural) {
    lay->addWidget(makeToolButton(QStringLiteral("testprog_remove_step"),
                                  [this, row]() { onDeleteStep(row); }));
  }
  lay->addStretch();
  return container;
}

QToolButton* TestProgramWizard::CasesPage::makeToolButton(
    const QString& iconName, std::function<void()> handler) {
  auto* btn = new QToolButton(step_table_);
  btn->setIcon(core_ui::AppIconProvider::instance().icon(iconName));
  btn->setIconSize(QSize(16, 16));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setAutoRaise(true);
  connect(btn, &QToolButton::clicked, this,
          [handler](bool /*checked*/) { handler(); });
  return btn;
}

void TestProgramWizard::CasesPage::onAddStep() {
  int insertPos = 0;
  bool insideBlock = false;
  insertionContext(step_table_->currentRow(), &insertPos, &insideBlock);

  StepEditDialog dlg(this);
  dlg.configure(false, insideBlock, StepEditResult());
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const StepEditResult r = dlg.result();
  WizardStep ws = stepFromResult(r);
  QVector<WizardStep>& steps = currentStepsMut();
  if (isFlowStarter(ws.cmd)) {
    steps.insert(insertPos, ws);
    int k = insertPos + 1;
    if (ws.cmd == QStringLiteral("IF") && r.includeElse) {
      steps.insert(k, makeStructuralRow(QStringLiteral("ELSE")));
      ++k;
    }
    steps.insert(k, makeStructuralRow(endNameFor(ws.cmd)));
  } else {
    ws.parent = insideBlock;
    steps.insert(insertPos, ws);
  }
  setDirty();
  rebuildStepTable();
  selectRow(insertPos);
  emit completeChanged();
}

void TestProgramWizard::CasesPage::onEditStep(int row) {
  QVector<WizardStep>& steps = currentStepsMut();
  if (row < 0 || row >= steps.size()) {
    return;
  }
  const WizardStep& ws = steps.at(row);
  if (isStructural(ws.cmd)) {
    return;  // END/ELSE 不可编辑（按钮已隐藏，双保险）
  }
  StepEditResult initial;
  initial.cmd = ws.cmd;
  initial.target = ws.target;
  initial.value = ws.value;
  initial.tolerance = ws.tolerance;
  initial.timeout = ws.timeout;
  initial.condition = ws.condition;
  initial.loopCount = ws.loopCount;
  initial.includeElse = hasElse(row);

  StepEditDialog dlg(this);
  dlg.configure(true, ws.parent, initial);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const StepEditResult r = dlg.result();
  if (applyEdit(row, ws.cmd, r)) {
    setDirty();
    rebuildStepTable();
    emit completeChanged();
  }
}

bool TestProgramWizard::CasesPage::applyEdit(int row, const QString& oldCmd,
                                             const StepEditResult& r) {
  QVector<WizardStep>& steps = currentStepsMut();
  const QString newCmd = r.cmd;
  const bool oldFlow = isFlowStarter(oldCmd);
  const bool newFlow = isFlowStarter(newCmd);

  // 预判是否需要删除 ELSE 分支，先确认再动手（避免拒绝后留下半改状态）。
  // 仅 flow→flow 才可能真删除 ELSE 分支；flow→leaf 走 hoistFlowToLeaf 保留
  // 双分支数据，无需确认。
  const bool needElseRemoval =
      oldFlow && newFlow && hasElse(row) &&
      ((oldCmd == QStringLiteral("IF") && newCmd != QStringLiteral("IF")) ||
       (oldCmd == QStringLiteral("IF") && newCmd == QStringLiteral("IF") &&
        !r.includeElse));
  if (needElseRemoval) {
    const int elseRow = findElseRow(row);
    const int endRow = findMatchingEnd(row);
    int bodyCount = 0;
    for (int i = elseRow + 1; i < endRow; ++i) {
      ++bodyCount;
    }
    if (bodyCount > 0) {
      const QMessageBox::StandardButton reply = QMessageBox::question(
          this, QStringLiteral("删除 ELSE 分支"),
          QStringLiteral("将同时删除 ELSE 分支的 %1 条步骤，确定？").arg(bodyCount),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (reply != QMessageBox::Yes) {
        return false;
      }
    }
  }

  if (oldFlow && newFlow) {
    updateStepFields(steps[row], r);
    const int endRow = findMatchingEnd(row);
    if (endRow >= 0) {
      steps[endRow].cmd = endNameFor(newCmd);
    }
    if (needElseRemoval) {
      removeElseBranch(row);
    } else if (newCmd == QStringLiteral("IF") && r.includeElse &&
               findElseRow(row) < 0) {
      const int endRow2 = findMatchingEnd(row);
      if (endRow2 > 0) {
        steps.insert(endRow2, makeStructuralRow(QStringLiteral("ELSE")));
      }
    }
    return true;
  }
  if (oldFlow && !newFlow) {
    hoistFlowToLeaf(row, r);
    return true;
  }
  if (!oldFlow && newFlow) {
    // 仅顶层叶子可达：body 叶子处控制流命令已禁用
    updateStepFields(steps[row], r);
    steps[row].parent = false;
    int k = row + 1;
    if (newCmd == QStringLiteral("IF") && r.includeElse) {
      steps.insert(k, makeStructuralRow(QStringLiteral("ELSE")));
      ++k;
    }
    steps.insert(k, makeStructuralRow(endNameFor(newCmd)));
    return true;
  }
  updateStepFields(steps[row], r);
  return true;
}

void TestProgramWizard::CasesPage::updateStepFields(WizardStep& s,
                                                    const StepEditResult& r) {
  s.cmd = r.cmd;
  s.target = r.target;
  s.value = r.value;
  s.tolerance = r.tolerance;
  s.timeout = r.timeout;
  s.condition = r.condition;
  s.loopCount = r.loopCount;
}

void TestProgramWizard::CasesPage::hoistFlowToLeaf(int row,
                                                   const StepEditResult& r) {
  QVector<WizardStep>& steps = currentStepsMut();
  const int endRow = findMatchingEnd(row);
  if (endRow < 0) {
    return;
  }
  QVector<WizardStep> body;
  for (int i = row + 1; i < endRow; ++i) {
    if (steps.at(i).cmd == QStringLiteral("ELSE")) {
      continue;
    }
    WizardStep s = steps.at(i);
    s.parent = false;
    body.append(s);
  }
  WizardStep leaf = stepFromResult(r);
  steps.remove(row, endRow - row + 1);
  steps.insert(row, leaf);
  for (int k = 0; k < body.size(); ++k) {
    steps.insert(row + 1 + k, body.at(k));
  }
}

void TestProgramWizard::CasesPage::deleteFlowBlock(int startRow) {
  QVector<WizardStep>& steps = currentStepsMut();
  const int endRow = findMatchingEnd(startRow);
  if (endRow < 0) {
    return;
  }
  int bodyCount = 0;
  for (int i = startRow + 1; i < endRow; ++i) {
    if (steps.at(i).cmd != QStringLiteral("ELSE")) {
      ++bodyCount;
    }
  }
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("删除步骤"),
      QStringLiteral("将同时删除块内 %1 条子步骤，确定？").arg(bodyCount),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }
  steps.remove(startRow, endRow - startRow + 1);
  setDirty();
  rebuildStepTable();
  emit completeChanged();
}

void TestProgramWizard::CasesPage::removeElseBranch(int ifRow) {
  QVector<WizardStep>& steps = currentStepsMut();
  const int elseRow = findElseRow(ifRow);
  if (elseRow < 0) {
    return;
  }
  const int endRow = findMatchingEnd(ifRow);
  if (endRow > elseRow) {
    steps.remove(elseRow, endRow - elseRow);  // ELSE 行 + else body
  }
}

void TestProgramWizard::CasesPage::onDeleteStep(int row) {
  QVector<WizardStep>& steps = currentStepsMut();
  if (row < 0 || row >= steps.size()) {
    return;
  }
  const QString cmd = steps.at(row).cmd;
  if (isStructural(cmd)) {
    return;  // END/ELSE 禁止删除（按钮已隐藏，双保险）
  }
  if (isFlowStarter(cmd)) {
    deleteFlowBlock(row);
    return;
  }
  if (removingEmptiesBlock(row)) {
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, QStringLiteral("删除步骤"),
        QStringLiteral("删除该步骤后，所在控制流块将没有内容（校验无法通过），确定？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
      return;
    }
  }
  steps.remove(row, 1);
  setDirty();
  rebuildStepTable();
  emit completeChanged();
}

bool TestProgramWizard::CasesPage::removingEmptiesBlock(int row) const {
  const QVector<WizardStep>& steps = currentSteps();
  const WizardStep& ws = steps.at(row);
  if (!ws.parent) {
    return false;
  }
  const int start = enclosingBlockStart(row);
  if (start < 0) {
    return false;
  }
  const int end = findMatchingEnd(start);
  // 对 IF 只统计 THEN 分支（ELSE 之前）；LOOP/WHILE 统计整个块体
  int regionEnd = end;
  if (steps.at(start).cmd == QStringLiteral("IF")) {
    const int elseRow = findElseRow(start);
    if (elseRow >= 0) {
      if (row > elseRow) {
        return false;  // 被删叶子在 ELSE 分支，ELSE 可为空，无需确认
      }
      regionEnd = elseRow;
    }
  }
  int remaining = 0;
  for (int i = start + 1; i < regionEnd; ++i) {
    if (i != row && !isStructural(steps.at(i).cmd)) {
      ++remaining;
    }
  }
  return remaining == 0;
}

int TestProgramWizard::CasesPage::enclosingBlockStart(int bodyRow) const {
  const QVector<WizardStep>& steps = currentSteps();
  int depth = 0;
  // END 行自身是闭合符：从 bodyRow 起扫会把本块起始当成"已闭合"，导致返回 -1，
  // 故 END 行需从 bodyRow - 1 起扫（END 对应的深度由块起始上的 depth==0 命中）。
  int i = bodyRow;
  if (i >= 0 && isFlowEnder(steps.at(i).cmd)) {
    --i;
  }
  for (; i >= 0; --i) {
    if (isFlowEnder(steps.at(i).cmd)) {
      ++depth;
    } else if (isFlowStarter(steps.at(i).cmd)) {
      if (depth == 0) {
        return i;
      }
      --depth;
    }
  }
  return -1;
}

int TestProgramWizard::CasesPage::findMatchingEnd(int startRow) const {
  const QVector<WizardStep>& steps = currentSteps();
  int depth = 0;
  for (int i = startRow; i < steps.size(); ++i) {
    if (isFlowStarter(steps.at(i).cmd)) {
      ++depth;
    } else if (isFlowEnder(steps.at(i).cmd)) {
      --depth;
      if (depth == 0) {
        return i;
      }
    }
  }
  return -1;
}

int TestProgramWizard::CasesPage::findElseRow(int ifRow) const {
  const QVector<WizardStep>& steps = currentSteps();
  const int end = findMatchingEnd(ifRow);
  for (int i = ifRow + 1; i < end; ++i) {
    if (steps.at(i).cmd == QStringLiteral("ELSE")) {
      return i;
    }
  }
  return -1;
}

bool TestProgramWizard::CasesPage::hasElse(int ifRow) const {
  return findElseRow(ifRow) >= 0;
}

void TestProgramWizard::CasesPage::onMoveStep(int row, bool up) {
  QVector<WizardStep>& steps = currentStepsMut();
  if (row < 0 || row >= steps.size()) {
    return;
  }
  const QString cmd = steps.at(row).cmd;
  if (cmd == QStringLiteral("ELSE")) {
    return;
  }
  const int n = steps.size();
  if (up && row == 0) {
    return;
  }
  if (!up && row == n - 1) {
    return;
  }
  int newPos = -1;
  if (isFlowStarter(cmd)) {
    if (!moveBlock(row, up)) {
      return;
    }
  } else if (isFlowEnder(cmd)) {
    const int start = enclosingBlockStart(row);
    if (start < 0 || !moveBlock(start, up)) {
      return;
    }
  } else if (steps.at(row).parent) {
    // body 叶子：块内相邻交换（上移止于起始、下移止于 END/ELSE）
    const int target = up ? row - 1 : row + 1;
    if (!steps.at(target).parent) {
      return;
    }
    std::swap(steps[row], steps[target]);
    newPos = target;
  } else {
    // 顶层叶子：只与顶层叶子相邻交换（块是原子单元）
    const int target = up ? row - 1 : row + 1;
    const WizardStep& t = steps.at(target);
    if (t.parent || isFlowStarter(t.cmd) || isFlowEnder(t.cmd) ||
        t.cmd == QStringLiteral("ELSE")) {
      return;
    }
    std::swap(steps[row], steps[target]);
    newPos = target;
  }
  setDirty();
  rebuildStepTable();
  if (newPos >= 0) {
    selectRow(newPos);
  }
  emit completeChanged();
}

bool TestProgramWizard::CasesPage::moveBlock(int startRow, bool up) {
  QVector<WizardStep>& steps = currentStepsMut();
  const int endRow = findMatchingEnd(startRow);
  if (endRow < 0) {
    return false;
  }
  if (up) {
    if (startRow == 0) {
      return false;
    }
    const WizardStep& prev = steps.at(startRow - 1);
    if (prev.parent || isFlowStarter(prev.cmd) || isFlowEnder(prev.cmd) ||
        prev.cmd == QStringLiteral("ELSE")) {
      return false;
    }
    const WizardStep first = steps.at(startRow - 1);
    for (int i = startRow; i <= endRow; ++i) {
      steps[i - 1] = steps.at(i);
    }
    steps[endRow] = first;
    return true;
  }
  if (endRow >= steps.size() - 1) {
    return false;
  }
  const WizardStep& next = steps.at(endRow + 1);
  if (next.parent || isFlowStarter(next.cmd) || isFlowEnder(next.cmd) ||
      next.cmd == QStringLiteral("ELSE")) {
    return false;
  }
  const WizardStep last = steps.at(endRow + 1);
  for (int i = endRow; i >= startRow; --i) {
    steps[i + 1] = steps.at(i);
  }
  steps[startRow] = last;
  return true;
}

void TestProgramWizard::CasesPage::insertionContext(int afterRow,
                                                    int* insertPos,
                                                    bool* insideBlock) const {
  const QVector<WizardStep>& steps = currentSteps();
  const int n = steps.size();
  int pos = n;
  if (afterRow >= 0 && afterRow < n) {
    pos = afterRow + 1;
  }
  int depth = 0;
  for (int i = 0; i < pos; ++i) {
    if (isFlowStarter(steps.at(i).cmd)) {
      ++depth;
    } else if (isFlowEnder(steps.at(i).cmd)) {
      --depth;
    }
  }
  *insertPos = pos;
  *insideBlock = (depth > 0);
}

void TestProgramWizard::CasesPage::onClearSteps() {
  QVector<WizardStep>& steps = currentStepsMut();
  if (steps.isEmpty()) {
    return;
  }
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("清空步骤"),
      QStringLiteral("确定要清空当前用例的所有步骤吗？"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }
  steps.clear();
  setDirty();
  rebuildStepTable();
  emit completeChanged();
}

bool TestProgramWizard::CasesPage::validatePage() {
  if (currentSteps().isEmpty()) {
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, QStringLiteral("继续"),
        QStringLiteral("当前用例没有任何步骤，确定要继续吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
      return false;
    }
  }
  return true;
}

void TestProgramWizard::CasesPage::setDirty() { dirty_ = true; }

int TestProgramWizard::CasesPage::totalStepCount() const {
  int count = 0;
  for (const WizardCase& c : cases_) {
    for (const WizardStep& s : c.steps) {
      if (!isStructural(s.cmd)) {
        ++count;
      }
    }
  }
  return count;
}

QString TestProgramWizard::CasesPage::stepPreview() const {
  QStringList lines;
  for (const WizardCase& c : cases_) {
    lines << summarizeCase(c);
  }
  return lines.join(QStringLiteral("\n"));
}

// ── 完成页 ──

class TestProgramWizard::SummaryPage : public WizardPage {
 public:
  explicit SummaryPage(QWidget* parent = nullptr);

  void setSummary(const QString& templateLabel, const QString& name,
                  int caseCount, int totalSteps, const QString& preview);
  QString stepLabel() const override { return QStringLiteral("完成"); }

 private:
  void initUi();
  QLabel* addItem(QGridLayout* grid, int row, int col, const QString& label,
                  const QString& value, int colspan = 1);

  QLabel* template_value_ = nullptr;
  QLabel* name_value_ = nullptr;
  QLabel* case_count_value_ = nullptr;
  QLabel* step_count_value_ = nullptr;
  QLabel* preview_value_ = nullptr;
};

TestProgramWizard::SummaryPage::SummaryPage(QWidget* parent)
    : WizardPage(parent) {
  initUi();
}

void TestProgramWizard::SummaryPage::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  auto* intro = new QLabel(
      QStringLiteral("请确认以下信息，点击「创建测试程序」即可完成。"), this);
  intro->setObjectName(QStringLiteral("summaryIntro"));
  layout->addWidget(intro);

  auto* gridWidget = new QWidget(this);
  gridWidget->setObjectName(QStringLiteral("summaryGrid"));
  auto* grid = new QGridLayout(gridWidget);
  grid->setContentsMargins(20, 16, 20, 16);
  grid->setHorizontalSpacing(32);
  grid->setVerticalSpacing(16);

  template_value_ = addItem(grid, 0, 0, QStringLiteral("程序模板"), QString());
  name_value_ = addItem(grid, 0, 1, QStringLiteral("程序名称"), QString());
  case_count_value_ = addItem(grid, 1, 0, QStringLiteral("用例数"), QString());
  step_count_value_ =
      addItem(grid, 1, 1, QStringLiteral("总步骤数"), QString());

  auto* previewLabel = new QLabel(QStringLiteral("步骤概览"), this);
  previewLabel->setObjectName(QStringLiteral("summaryLabel"));
  grid->addWidget(previewLabel, 2, 0, 1, 2, Qt::AlignTop);
  preview_value_ = new QLabel(QString(), this);
  preview_value_->setObjectName(QStringLiteral("summaryStepPreview"));
  preview_value_->setWordWrap(true);
  preview_value_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  grid->addWidget(preview_value_, 3, 0, 1, 2);

  layout->addWidget(gridWidget);
  layout->addStretch();
}

void TestProgramWizard::SummaryPage::setSummary(const QString& templateLabel,
                                                const QString& name,
                                                int caseCount, int totalSteps,
                                                const QString& preview) {
  template_value_->setText(templateLabel);
  name_value_->setText(name.isEmpty() ? QStringLiteral("（未命名）") : name);
  case_count_value_->setText(QString::number(caseCount));
  step_count_value_->setText(QString::number(totalSteps));
  preview_value_->setText(preview);
}

QLabel* TestProgramWizard::SummaryPage::addItem(QGridLayout* grid, int row,
                                                int col, const QString& label,
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

TestProgramWizard::TestProgramWizard(QWidget* parent)
    : BaseWizardDialog(parent) {
  initUi();
  initSignals();
}

void TestProgramWizard::initUi() {
  setWindowTitle(QStringLiteral("新建测试程序"));
  setHeader(QStringLiteral("file_generic"), QStringLiteral("新建测试程序"),
            QStringLiteral("配置程序信息，定义测试用例与测试步骤，创建自动化测试程序"));
  setCreateButtonText(QStringLiteral("创建测试程序"));

  template_page_ = new TemplatePage(this);
  info_page_ = new ProgramInfoPage(this);
  cases_page_ = new CasesPage(this);
  summary_page_ = new SummaryPage(this);
  addPage(template_page_);
  addPage(info_page_);
  addPage(cases_page_);
  addPage(summary_page_);

  // 卡片走专用 QSS（#tpWizardCard，区别于新建项目向导的 #wizardCard）
  if (QWidget* card = findChild<QWidget*>(QStringLiteral("wizardCard"))) {
    card->setObjectName(QStringLiteral("tpWizardCard"));
  }

  loadTemplate(QStringLiteral("empty"));
}

void TestProgramWizard::initSignals() {
  connect(template_page_, &WizardPage::completeChanged, this,
          [this]() { onTemplateSelected(template_page_->selectedTemplateId()); });
  connect(info_page_, &WizardPage::completeChanged, this,
          &TestProgramWizard::updateSummary);
  connect(cases_page_, &WizardPage::completeChanged, this,
          &TestProgramWizard::updateSummary);
  connect(this, &BaseWizardDialog::currentPageChanged, this,
          [this](int index) {
            if (index == pageCount() - 1) {
              updateSummary();
            }
          });
}

void TestProgramWizard::onTemplateSelected(const QString& templateId) {
  if (cases_page_->isDirty()) {
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, QStringLiteral("重新加载模板"),
        QStringLiteral("重新加载模板将覆盖已编辑的用例数据，确定要继续吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) {
      // 回退卡片勾选到上一模板，避免选中态与已载入数据不一致
      template_page_->setSelectedTemplateId(template_id_);
      return;
    }
  }
  loadTemplate(templateId);
}

void TestProgramWizard::loadTemplate(const QString& templateId) {
  cases_page_->setCases(templateCases(templateId));
  template_id_ = templateId;
  updateSummary();
}

void TestProgramWizard::updateSummary() {
  if (!summary_page_) {
    return;
  }
  summary_page_->setSummary(templateLabel(template_id_), info_page_->name(),
                            cases_page_->cases().size(),
                            cases_page_->totalStepCount(),
                            cases_page_->stepPreview());
}

QString TestProgramWizard::templateLabel(const QString& templateId) {
  if (templateId == QStringLiteral("basic")) {
    return QStringLiteral("基础信号测试");
  }
  if (templateId == QStringLiteral("loop")) {
    return QStringLiteral("循环控制测试");
  }
  if (templateId == QStringLiteral("composite")) {
    return QStringLiteral("复合场景测试");
  }
  return QStringLiteral("空程序");
}

QString TestProgramWizard::templateId() const { return template_id_; }

TestProgramData TestProgramWizard::resultProgram() const {
  TestProgramData suite;
  suite.name = info_page_->name();
  suite.version = info_page_->version();
  suite.author = info_page_->author();
  suite.description = info_page_->description();
  suite.precondition = info_page_->precondition();
  for (const WizardCase& c : cases_page_->cases()) {
    TestCaseData tc;
    tc.name = c.name;
    QString error;
    if (!buildNestedSteps(c.steps, &tc.steps, &error)) {
      // 结构防御已在 onCreateValidate 拦截；此处兜底跳过（理论不可达）
      LOG_ERROR("TESTPROG_UI", "测试程序向导翻译失败, 跳过用例: {}",
                c.name.toStdString());
      continue;
    }
    suite.cases.append(tc);
  }
  return suite;
}

bool TestProgramWizard::onCreateValidate() {
  for (const WizardCase& c : cases_page_->cases()) {
    QVector<TestStepData> nested;
    QString error;
    if (!buildNestedSteps(c.steps, &nested, &error)) {
      QMessageBox::warning(this, QStringLiteral("无法创建测试程序"), error);
      return false;
    }
  }
  return true;
}

void TestProgramWizard::confirmCancel() {
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("取消创建"),
      QStringLiteral("确定要取消创建测试程序吗？已填写的内容将丢失。"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    reject();
  }
}

}  // namespace etest::app
