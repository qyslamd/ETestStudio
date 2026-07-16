#include "EtlogViewerWidget.h"

#include <functional>

#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QTreeView>
#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "ThemeManager.h"

namespace etest::app {

using etest::core_ui::ThemeManager;
using etest::core_ui::AppIconProvider;

enum {
  kColStep = 0,
  kColTarget,
  kColStatus,
  kColElapsed,
  kColumnCount
};

enum StepRoles {
  StepDataRole = Qt::UserRole + 1,
  StepPathRole = Qt::UserRole + 2,
};

enum {
  kCaseStatusRole = Qt::UserRole + 3,
};

namespace {
QString themeIconPath(const QString& iconName) {
  bool dark = etest::core_ui::ThemeManager::instance().isDarkTheme();
  return dark
      ? QStringLiteral(":/resources/icons/svg/%1_light.svg").arg(iconName)
      : QStringLiteral(":/resources/icons/svg/%1_dark.svg").arg(iconName);
}
}  // namespace

EtlogViewerWidget::EtlogViewerWidget(const QString& filePath, QWidget* parent)
    : QWidget(parent), file_path_(filePath) {
  initUi();

  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this](bool) { applyThemeColors(); });
}

QString EtlogViewerWidget::displayName() const {
  return QFileInfo(file_path_).fileName();
}

bool EtlogViewerWidget::isModified() const { return false; }
bool EtlogViewerWidget::save() { return false; }
bool EtlogViewerWidget::saveAs(const QString&) { return false; }
QString EtlogViewerWidget::filePath() const { return file_path_; }
QString EtlogViewerWidget::editorId() const { return file_path_; }
QWidget* EtlogViewerWidget::widget() { return this; }
QString EtlogViewerWidget::editorType() const { return QStringLiteral("etlog"); }
QObject* EtlogViewerWidget::signalObject() { return this; }
bool EtlogViewerWidget::canUndo() const { return false; }
bool EtlogViewerWidget::canRedo() const { return false; }
void EtlogViewerWidget::undo() {}
void EtlogViewerWidget::redo() {}

void EtlogViewerWidget::openFile(const QString& filePath) {
  file_path_ = filePath;
  has_error_ = false;

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    showEmptyState(QStringLiteral("无法打开文件"));
    return;
  }

  QJsonParseError err;
  doc_ = QJsonDocument::fromJson(file.readAll(), &err);
  file.close();

  if (err.error != QJsonParseError::NoError || !doc_.isObject()) {
    showEmptyState(QStringLiteral("无法解析测试报告"));
    return;
  }

  QJsonObject root = doc_.object();
  QJsonArray cases = root["cases"].toArray();
  if (cases.isEmpty()) {
    showEmptyState(QStringLiteral("文件中无执行结果"));
    return;
  }

  if (auto* parent = empty_label_->parentWidget()) {
    parent->setVisible(false);
  }
  content_widget_->setVisible(true);

  populateSummary(root);
  populateCaseList(root);

  if (case_list_->count() > 0) {
    case_list_->setCurrentRow(0);
  }
}

void EtlogViewerWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* summary_bar = new QFrame(this);
  summary_bar->setObjectName(QStringLiteral("etlogSummaryBar"));
  summary_bar->setFrameShape(QFrame::StyledPanel);
  auto* summary_layout = new QHBoxLayout(summary_bar);
  summary_layout->setContentsMargins(12, 6, 12, 6);

  title_label_ = new QLabel(this);
  title_label_->setObjectName(QStringLiteral("etlogTitleLabel"));
  summary_layout->addWidget(title_label_, 1);

  icon_title_ = new QLabel(this);
  icon_title_->setFixedSize(16, 16);
  summary_layout->insertWidget(0, icon_title_);

  summary_label_ = new QLabel(this);
  summary_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  summary_layout->addWidget(summary_label_);

  layout->addWidget(summary_bar);

  // Content area
  content_widget_ = new QWidget(this);
  auto* content_layout = new QVBoxLayout(content_widget_);
  content_layout->setContentsMargins(0, 0, 0, 0);

  auto* splitter = new QSplitter(Qt::Horizontal, content_widget_);

  case_list_ = new QListWidget(splitter);
  case_list_->setObjectName(QStringLiteral("etlogCaseList"));
  case_list_->setFrameShape(QFrame::NoFrame);
  case_list_->setMinimumWidth(120);

  step_model_ = new QStandardItemModel(0, kColumnCount, this);
  step_model_->setHorizontalHeaderLabels({
    QStringLiteral("步骤"),
    QStringLiteral("目标"),
    QStringLiteral("状态"),
    QStringLiteral("耗时(ms)"),
  });

  step_tree_ = new QTreeView(splitter);
  step_tree_->setModel(step_model_);
  step_tree_->setFrameShape(QFrame::NoFrame);
  step_tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  step_tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
  step_tree_->setAnimated(true);
  step_tree_->setAllColumnsShowFocus(true);
  step_tree_->setRootIsDecorated(true);
  step_tree_->setUniformRowHeights(true);
  step_tree_->header()->setStretchLastSection(false);
  step_tree_->header()->setSectionResizeMode(kColStep, QHeaderView::Stretch);
  step_tree_->header()->setSectionResizeMode(kColTarget, QHeaderView::ResizeToContents);
  step_tree_->header()->setSectionResizeMode(kColStatus, QHeaderView::ResizeToContents);
  step_tree_->header()->setSectionResizeMode(kColElapsed, QHeaderView::ResizeToContents);

  detail_scroll_ = new QScrollArea(splitter);
  detail_scroll_->setObjectName(QStringLiteral("etlogDetailScroll"));
  detail_scroll_->viewport()->setObjectName(QStringLiteral("etlogDetailViewport"));
  detail_scroll_->setFrameShape(QFrame::NoFrame);
  detail_scroll_->setWidgetResizable(true);

  auto* detail_inner = new QWidget();
  detail_inner->setObjectName(QStringLiteral("etlogDetailInner"));
  detail_scroll_->setWidget(detail_inner);
  auto* outer_layout = new QVBoxLayout(detail_inner);
  outer_layout->setContentsMargins(0, 0, 0, 0);

  auto* form_widget = new QWidget();
  auto* detail_form = new QFormLayout(form_widget);
  detail_form->setContentsMargins(12, 24, 12, 4);
  detail_form->setSpacing(8);

  detail_status_ = new QLabel(QStringLiteral("-"));
  detail_status_->setObjectName(QStringLiteral("etlogDetailStatus"));
  detail_form->addRow(QStringLiteral("状态:"), detail_status_);

  detail_command_ = new QLabel(QStringLiteral("-"));
  detail_command_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  detail_form->addRow(QStringLiteral("命令:"), detail_command_);

  detail_target_ = new QLabel(QStringLiteral("-"));
  detail_target_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  detail_form->addRow(QStringLiteral("目标:"), detail_target_);

  detail_expected_ = new QLabel(QStringLiteral("-"));
  detail_form->addRow(QStringLiteral("期望值:"), detail_expected_);

  detail_actual_ = new QLabel(QStringLiteral("-"));
  detail_form->addRow(QStringLiteral("实际值:"), detail_actual_);

  detail_elapsed_ = new QLabel(QStringLiteral("-"));
  detail_form->addRow(QStringLiteral("耗时(ms):"), detail_elapsed_);

  detail_timestamp_ = new QLabel(QStringLiteral("-"));
  detail_form->addRow(QStringLiteral("时间戳:"), detail_timestamp_);

  outer_layout->addWidget(form_widget);

  // 消息框独立于 QFormLayout 之外
  auto* msg_label = new QLabel(QStringLiteral("消息:"), detail_inner);
  msg_label->setContentsMargins(12, 4, 12, 0);
  outer_layout->addWidget(msg_label);
  detail_message_ = new QTextEdit(this);
  detail_message_->setReadOnly(true);
  detail_message_->setMaximumHeight(100);
  detail_message_->setFrameShape(QFrame::NoFrame);
  detail_message_->setPlaceholderText(QStringLiteral("-"));
  detail_message_->setContentsMargins(12, 0, 12, 12);
  outer_layout->addWidget(detail_message_);
  outer_layout->addStretch(1);

  splitter->addWidget(case_list_);
  splitter->addWidget(step_tree_);
  splitter->addWidget(detail_scroll_);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 2);
  splitter->setStretchFactor(2, 2);

  content_layout->addWidget(splitter, 1);
  layout->addWidget(content_widget_, 1);

  auto* footer_bar = new QFrame(this);
  footer_bar->setObjectName(QStringLiteral("etlogFooterBar"));
  footer_bar->setFrameShape(QFrame::StyledPanel);
  auto* footer_layout = new QHBoxLayout(footer_bar);
  footer_layout->setContentsMargins(12, 4, 12, 4);
  footer_label_ = new QLabel(this);
  footer_label_->setObjectName(QStringLiteral("etlogFooterLabel"));
  footer_layout->addWidget(footer_label_);
  layout->addWidget(footer_bar);

  auto* empty_widget = new QWidget(this);
  auto* empty_layout = new QVBoxLayout(empty_widget);
  empty_label_ = new QLabel(QStringLiteral("打开 .etlog 文件查看测试报告"), empty_widget);
  empty_label_->setObjectName(QStringLiteral("etlogEmptyLabel"));
  empty_label_->setAlignment(Qt::AlignCenter);
  empty_layout->addWidget(empty_label_);
  layout->addWidget(empty_widget, 1);

  content_widget_->setVisible(false);
  applyThemeColors();

  connect(case_list_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row < 0) return;
    QJsonObject root = doc_.object();
    QJsonArray cases = root["cases"].toArray();
    if (row < cases.size()) {
      populateStepTree(cases[row].toObject());
    }
  });

  connect(step_tree_->selectionModel(), &QItemSelectionModel::currentChanged,
          this, [this](const QModelIndex& current, const QModelIndex&) {
    if (!current.isValid()) return;
    auto* item = step_model_->itemFromIndex(current.sibling(current.row(), 0));
    if (!item) return;
    QVariant data = item->data(StepDataRole);
    if (data.isValid()) {
      QJsonObject stepObj = QJsonObject::fromVariantMap(data.toMap());
      populateStepDetail(stepObj);
    }
  });
}

void EtlogViewerWidget::populateSummary(const QJsonObject& root) {
  QString suiteName = root["suiteName"].toString();
  if (suiteName.isEmpty()) {
    suiteName = QFileInfo(file_path_).baseName();
  }
  QJsonObject summary = root["summary"].toObject();
  int total = summary["totalCases"].toInt();
  int pass = summary["passCount"].toInt();
  int fail = summary["failCount"].toInt();
  int err = summary["errorCount"].toInt();
  int dur = summary["durationMs"].toInt();
  Q_UNUSED(total);

  icon_title_->setPixmap(
      AppIconProvider::instance().icon(QStringLiteral("etlog_chart"))
          .pixmap(16, 16));
  title_label_->setText(suiteName);

  QStringList parts;
  parts << QStringLiteral("<span style='color:%1'>PASS: %2</span>")
               .arg(colorForStatus("PASS").name())
               .arg(pass);
  if (fail > 0) {
    parts << QStringLiteral("<span style='color:%1'>FAIL: %2</span>")
                 .arg(colorForStatus("FAIL").name())
                 .arg(fail);
  }
  if (err > 0) {
    parts << QStringLiteral("<span style='color:%1'>ERROR: %2</span>")
                 .arg(colorForStatus("ERROR").name())
                 .arg(err);
  }
  parts << QStringLiteral("<img src='%1' width='14' height='14' style='vertical-align:middle'>&nbsp;%2ms")
               .arg(themeIconPath(QStringLiteral("etlog_timer")))
               .arg(dur);

  summary_label_->setText(parts.join(QStringLiteral("  ")));
  summary_label_->setTextFormat(Qt::RichText);

  QString start = root["startTime"].toString();
  QString end = root["endTime"].toString();
  QString engineVer = root["executionInfo"].toObject()["engineVersion"].toString();
  if (!engineVer.isEmpty()) {
    engineVer = QStringLiteral("  引擎版本: %1").arg(engineVer);
  }
  footer_label_->setText(
      QStringLiteral("执行: %1 \xE2\x86\x92 %2%3")
          .arg(start, end, engineVer));
}

void EtlogViewerWidget::populateCaseList(const QJsonObject& root) {
  case_list_->clear();
  QJsonArray cases = root["cases"].toArray();
  for (const auto& c : cases) {
    QJsonObject caseObj = c.toObject();
    QString name = caseObj["caseName"].toString();
    QString status = caseObj["status"].toString();
    // 兜底：从 steps 重新聚合 case 状态（etlog 中 status 字段可能不准确）
    QJsonArray steps = caseObj["steps"].toArray();
    if (!steps.isEmpty()) {
      QString agg = aggregatedStatus(steps);
      if (agg != "PASS") {
        status = agg;
      }
    }
    int dur = caseObj["durationMs"].toInt();

    QString iconName = iconNameForStatus(status);
    QString text = QStringLiteral("%1 (%2ms)").arg(name).arg(dur);
    auto* item = new QListWidgetItem(text, case_list_);
    item->setIcon(QIcon(themeIconPath(iconName)));
    item->setData(Qt::UserRole, caseObj["caseIndex"].toInt());
    item->setData(kCaseStatusRole, status);
  }
}

void EtlogViewerWidget::populateStepTree(const QJsonObject& caseObj) {
  step_model_->removeRows(0, step_model_->rowCount());
  clearDetail();
  QJsonArray steps = caseObj["steps"].toArray();
  buildStepTreeRecursive(step_model_->invisibleRootItem(), steps, QString());
  step_tree_->expandAll();
}

void EtlogViewerWidget::populateStepDetail(const QJsonObject& step) {
  QString status = step["status"].toString();
  detail_status_->setText(status);
  detail_status_->setStyleSheet(
      QStringLiteral("color: %1;")
          .arg(colorForStatus(status).name()));

  detail_command_->setText(step["command"].toString());
  detail_target_->setText(step["target"].toString());

  detail_expected_->setText(step.contains("expectedValue")
                                ? QString::number(step["expectedValue"].toDouble())
                                : QStringLiteral("-"));

  detail_actual_->setText(step.contains("actualValue")
                              ? QString::number(step["actualValue"].toDouble())
                              : QStringLiteral("-"));

  detail_elapsed_->setText(QString::number(step["elapsedMs"].toInt()));
  detail_timestamp_->setText(step["timestamp"].toString());

  QString msg = step["message"].toString();
  detail_message_->setPlainText(msg.isEmpty() ? QStringLiteral("-") : msg);
}

void EtlogViewerWidget::buildStepTreeRecursive(QStandardItem* parent,
                                                const QJsonArray& steps,
                                                const QString& prefix) {
  for (int i = 0; i < steps.size(); ++i) {
    QJsonObject step = steps[i].toObject();
    QString path = prefix.isEmpty()
                       ? QString::number(i + 1)
                       : QStringLiteral("%1.%2").arg(prefix).arg(i + 1);

    QString cmd = step["command"].toString().toUpper();

    if (cmd == QStringLiteral("LOOP") || cmd == QStringLiteral("WHILE")) {
      QJsonArray iterations = step["iterations"].toArray();

      QString aggStatus = QStringLiteral("PASS");
      for (const auto& iter : iterations) {
        QJsonArray subSteps = iter.toObject()["subSteps"].toArray();
        QString iterStatus = aggregatedStatus(subSteps);
        if (iterStatus == "FAIL" || iterStatus == "ERROR") {
          aggStatus = iterStatus;
        } else if (aggStatus == "PASS" && iterStatus != "PASS") {
          aggStatus = iterStatus;
        }
      }

      QJsonObject loopStep = step;
      loopStep["status"] = aggStatus;
      auto row = createStepRow(loopStep, path);
      parent->appendRow(row);

      for (int j = 0; j < iterations.size(); ++j) {
        QJsonObject iter = iterations[j].toObject();
        int iterNum = iter["iteration"].toInt();
        QJsonArray subSteps = iter["subSteps"].toArray();
        QString iterStatus = aggregatedStatus(subSteps);
        QString iterPath = QStringLiteral("%1.I%2").arg(path).arg(iterNum);

        QJsonObject iterStep;
        iterStep["status"] = iterStatus;
        iterStep["command"] = QStringLiteral("iteration %1").arg(iterNum);

        auto iterRow = createStepRow(iterStep, iterPath);
        row[kColStep]->appendRow(iterRow);

        buildStepTreeRecursive(row[kColStep], subSteps,
                               QStringLiteral("%1.I%2").arg(path).arg(iterNum));
      }
    } else if (cmd == QStringLiteral("IF")) {
      QJsonObject branches = step["branches"].toObject();
      QJsonArray thenSteps = branches["then"].toArray();
      QJsonArray elseSteps = branches["else"].toArray();

      bool thenExecuted = false;
      bool elseExecuted = false;
      for (const auto& s : thenSteps) {
        if (s.toObject()["status"].toString() != "PENDING") {
          thenExecuted = true;
          break;
        }
      }
      if (!thenExecuted) {
        for (const auto& s : elseSteps) {
          if (s.toObject()["status"].toString() != "PENDING") {
            elseExecuted = true;
            break;
          }
        }
      }

      QString ifStatus;
      if (thenExecuted) {
        ifStatus = aggregatedStatus(thenSteps);
      } else if (elseExecuted) {
        ifStatus = aggregatedStatus(elseSteps);
      } else {
        ifStatus = QStringLiteral("PENDING");
      }

      QJsonObject ifStep = step;
      ifStep["status"] = ifStatus;
      auto row = createStepRow(ifStep, path);
      parent->appendRow(row);

      if (!thenSteps.isEmpty()) {
        QString thenLabel = thenExecuted
                                ? QStringLiteral("THEN")
                                : QStringLiteral("THEN (\xE6\x9C\xAA\xE6\x89\xA7\xE8\xA1\x8C)");
        auto* thenItem = new QStandardItem(thenLabel);
        thenItem->setEditable(false);
        row[kColStep]->appendRow(thenItem);
        buildStepTreeRecursive(thenItem, thenSteps,
                               QStringLiteral("%1.THEN").arg(path));
      }

      if (!elseSteps.isEmpty()) {
        QString elseLabel = elseExecuted
                                ? QStringLiteral("ELSE")
                                : QStringLiteral("ELSE (\xE6\x9C\xAA\xE6\x89\xA7\xE8\xA1\x8C)");
        auto* elseItem = new QStandardItem(elseLabel);
        elseItem->setEditable(false);
        row[kColStep]->appendRow(elseItem);
        buildStepTreeRecursive(elseItem, elseSteps,
                               QStringLiteral("%1.ELSE").arg(path));
      }
    } else {
      auto row = createStepRow(step, path);
      parent->appendRow(row);
    }
  }
}

QString EtlogViewerWidget::aggregatedStatus(const QJsonArray& steps) {
  int worst = 0;  // 0=PASS, 1=PENDING, 2=SKIPPED, 3=TIMEOUT, 4=FAIL, 5=ERROR
  for (const auto& s : steps) {
    QJsonObject step = s.toObject();
    QString st = step["status"].toString();

    int prio = 0;
    if (st == "ERROR")
      prio = 5;
    else if (st == "FAIL")
      prio = 4;
    else if (st == "TIMEOUT")
      prio = 3;
    else if (st == "SKIPPED")
      prio = 2;
    else if (st == "PENDING")
      prio = 1;
    if (prio > worst) {
      worst = prio;
      if (worst == 5) return QStringLiteral("ERROR");
    }

    // Recurse into LOOP/WHILE iterations
    for (const auto& iter : step["iterations"].toArray()) {
      QString sub = aggregatedStatus(
          iter.toObject()["subSteps"].toArray());
      if (sub == "ERROR") return QStringLiteral("ERROR");
      if (sub == "FAIL" && worst < 4) worst = 4;
      if (sub == "TIMEOUT" && worst < 3) worst = 3;
      if (sub == "SKIPPED" && worst < 2) worst = 2;
    }

    // Recurse into IF branches
    QJsonObject branches = step["branches"].toObject();
    if (!branches.isEmpty()) {
      QString thenSub = aggregatedStatus(branches["then"].toArray());
      QString elseSub = aggregatedStatus(branches["else"].toArray());
      for (const auto& sub : {thenSub, elseSub}) {
        if (sub == "ERROR") return QStringLiteral("ERROR");
        if (sub == "FAIL" && worst < 4) worst = 4;
        if (sub == "TIMEOUT" && worst < 3) worst = 3;
        if (sub == "SKIPPED" && worst < 2) worst = 2;
      }
    }
  }

  switch (worst) {
    case 4:
      return QStringLiteral("FAIL");
    case 3:
      return QStringLiteral("TIMEOUT");
    case 2:
      return QStringLiteral("SKIPPED");
    case 1:
      return QStringLiteral("PENDING");
    default:
      return QStringLiteral("PASS");
  }
}

QList<QStandardItem*> EtlogViewerWidget::createStepRow(const QJsonObject& step,
                                                        const QString& path) {
  QString status = step["status"].toString();
  QString cmd = step["command"].toString();
  QString target = step["target"].toString();
  int elapsed = step["elapsedMs"].toInt();

  auto* item0 = new QStandardItem(cmd);
  item0->setEditable(false);
  item0->setIcon(QIcon(themeIconPath(iconNameForStatus(status))));
  item0->setData(QVariant::fromValue(step.toVariantMap()), StepDataRole);
  item0->setData(path, StepPathRole);

  auto* item1 = new QStandardItem(target);
  item1->setEditable(false);

  auto* item2 = new QStandardItem(status);
  item2->setEditable(false);
  item2->setForeground(QBrush(colorForStatus(status)));

  auto* item3 = new QStandardItem(QString::number(elapsed));
  item3->setEditable(false);

  return {item0, item1, item2, item3};
}

void EtlogViewerWidget::showEmptyState(const QString& message) {
  has_error_ = true;
  empty_label_->setText(message);
  if (auto* parent = empty_label_->parentWidget()) {
    parent->setVisible(true);
  }
  content_widget_->setVisible(false);
}

void EtlogViewerWidget::clearDetail() {
  detail_command_->setText(QStringLiteral("-"));
  detail_target_->setText(QStringLiteral("-"));
  detail_status_->setText(QStringLiteral("-"));
  detail_status_->setStyleSheet(QString());
  detail_expected_->setText(QStringLiteral("-"));
  detail_actual_->setText(QStringLiteral("-"));
  detail_elapsed_->setText(QStringLiteral("-"));
  detail_timestamp_->setText(QStringLiteral("-"));
  detail_message_->setPlainText(QStringLiteral("-"));
}

void EtlogViewerWidget::applyThemeColors() {
  // 刷新步骤树中所有 item 的状态颜色
  if (step_model_) {
    std::function<void(QStandardItem*)> recolor;
    recolor = [&](QStandardItem* parent) {
      for (int i = 0; i < parent->rowCount(); ++i) {
        auto* statusItem = parent->child(i, kColStatus);
        if (statusItem) {
          statusItem->setForeground(
              QBrush(colorForStatus(statusItem->text())));
        }
        auto* stepItem = parent->child(i, kColStep);
        if (stepItem && stepItem->hasChildren()) {
          recolor(stepItem);
        }
      }
    };
    recolor(step_model_->invisibleRootItem());
  }

  // 刷新当前详情面板的状态颜色
  QString currentStatus = detail_status_->text();
  if (currentStatus != QStringLiteral("-")) {
    detail_status_->setStyleSheet(
        QStringLiteral("color: %1;").arg(
            colorForStatus(currentStatus).name()));
  }

  refreshIcons();
}

QString EtlogViewerWidget::iconNameForStatus(const QString& status) {
  if (status == "PASS") return QStringLiteral("etlog_pass");
  if (status == "FAIL") return QStringLiteral("etlog_fail");
  if (status == "ERROR") return QStringLiteral("etlog_error");
  if (status == "TIMEOUT") return QStringLiteral("etlog_timer");
  if (status == "SKIPPED") return QStringLiteral("etlog_skip");
  return QStringLiteral("etlog_pending");
}

QColor EtlogViewerWidget::colorForStatus(const QString& status) {
  bool dark = ThemeManager::instance().isDarkTheme();
  if (status == "PASS") return dark ? QColor(129, 199, 132) : QColor(76, 175, 80);
  if (status == "FAIL") return dark ? QColor(229, 115, 115) : QColor(244, 67, 54);
  if (status == "ERROR") return dark ? QColor(239, 83, 80) : QColor(183, 28, 28);
  if (status == "TIMEOUT") return dark ? QColor(255, 183, 77) : QColor(255, 152, 0);
  if (status == "SKIPPED") return dark ? QColor(189, 189, 189) : QColor(158, 158, 158);
  return dark ? QColor(97, 97, 97) : QColor(189, 189, 189);
}

void EtlogViewerWidget::refreshIcons() {
  // 1. 刷新标题栏图标
  icon_title_->setPixmap(
      AppIconProvider::instance().icon(QStringLiteral("etlog_chart"))
          .pixmap(16, 16));

  // 2. 刷新用例列表图标
  for (int i = 0; i < case_list_->count(); ++i) {
    auto* item = case_list_->item(i);
    QString status = item->data(kCaseStatusRole).toString();
    if (!status.isEmpty()) {
      item->setIcon(QIcon(themeIconPath(iconNameForStatus(status))));
    }
  }

  // 3. 刷新步骤树图标
  std::function<void(QStandardItem*)> refreshStepIcons;
  refreshStepIcons = [this, &refreshStepIcons](QStandardItem* parent) {
    for (int i = 0; i < parent->rowCount(); ++i) {
      auto* stepItem = parent->child(i, kColStep);
      if (stepItem) {
        QVariantMap data = stepItem->data(StepDataRole).toMap();
        if (!data.isEmpty()) {
          QString status = data.value(QStringLiteral("status")).toString();
          if (!status.isEmpty()) {
            stepItem->setIcon(QIcon(themeIconPath(iconNameForStatus(status))));
          }
        }
        if (stepItem->hasChildren()) {
          refreshStepIcons(stepItem);
        }
      }
    }
  };
  refreshStepIcons(step_model_->invisibleRootItem());
}

}  // namespace etest::app
