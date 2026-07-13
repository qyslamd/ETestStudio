#include "EtlogViewerWidget.h"

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

#include "ThemeManager.h"

namespace etest::app {

using etest::core_ui::ThemeManager;

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

  empty_label_->parentWidget()->setVisible(false);
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
  detail_form->setContentsMargins(12, 24, 12, 12);
  detail_form->setSpacing(8);
  outer_layout->addStretch(1);
  outer_layout->addWidget(form_widget);
  outer_layout->addStretch(1);

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

  detail_message_ = new QTextEdit(this);
  detail_message_->setReadOnly(true);
  detail_message_->setMaximumHeight(120);
  detail_message_->setFrameShape(QFrame::NoFrame);
  detail_message_->setPlaceholderText(QStringLiteral("-"));
  detail_form->addRow(QStringLiteral("消息:"), detail_message_);

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

  title_label_->setText(QStringLiteral("\xF0\x9F\x93\x8A %1").arg(suiteName));

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
  parts << QStringLiteral("\xE2\x8F\xB1 %1ms").arg(dur);

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
    int dur = caseObj["durationMs"].toInt();

    QString icon = (status == "PASS") ? QStringLiteral("\xE2\x9C\x85")
                                      : QStringLiteral("\xE2\x9D\x8C");
    QString text = QStringLiteral("%1 %2 (%3ms)").arg(icon).arg(name).arg(dur);
    auto* item = new QListWidgetItem(text, case_list_);
    Q_UNUSED(item);
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
  bool hasFail = false;
  QString firstNonPass;
  for (const auto& s : steps) {
    QString st = s.toObject()["status"].toString();
    if (st == "FAIL" || st == "ERROR") hasFail = true;
    if (st != "PASS" && firstNonPass.isEmpty()) firstNonPass = st;
  }
  if (hasFail) return QStringLiteral("FAIL");
  if (!firstNonPass.isEmpty()) return firstNonPass;
  return QStringLiteral("PASS");
}

QList<QStandardItem*> EtlogViewerWidget::createStepRow(const QJsonObject& step,
                                                        const QString& path) {
  QString status = step["status"].toString();
  QString cmd = step["command"].toString();
  QString target = step["target"].toString();
  int elapsed = step["elapsedMs"].toInt();

  auto statusIcon = [&]() -> QString {
    if (status == "PASS") return QStringLiteral("\xE2\x9C\x85");
    if (status == "FAIL") return QStringLiteral("\xE2\x9D\x8C");
    if (status == "ERROR") return QStringLiteral("\xE2\x9D\x97");
    if (status == "TIMEOUT") return QStringLiteral("\xE2\x8F\xB1");
    if (status == "SKIPPED") return QStringLiteral("\xE2\x8F\xAD");
    return QStringLiteral("\xE2\x8F\xB3");
  };

  auto* item0 = new QStandardItem(
      QStringLiteral("%1 %2").arg(statusIcon()).arg(cmd));
  item0->setEditable(false);
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
  empty_label_->parentWidget()->setVisible(true);
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

}  // namespace etest::app
