#include "SearchWidget.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTextStream>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "logger/Logger.h"

using namespace etest::core::logger;

namespace etest::app {

SearchWidget::SearchWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void SearchWidget::setSearchRoot(const QString& path) {
  search_root_ = path;
}

QString SearchWidget::searchRoot() const {
  return search_root_;
}

void SearchWidget::setFocusOnSearchInput() {
  search_input_->setFocus();
  search_input_->selectAll();
}

void SearchWidget::initUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  auto* inputRow = new QHBoxLayout();
  inputRow->setContentsMargins(8, 6, 8, 4);
  inputRow->setSpacing(4);

  search_input_ = new QLineEdit(this);
  search_input_->setPlaceholderText(QStringLiteral("搜索"));
  search_input_->setClearButtonEnabled(true);

  search_button_ = new QPushButton(this);
  search_button_->setToolTip(QStringLiteral("搜索"));
  search_button_->setFixedSize(26, 26);
  QIcon searchIcon;
  searchIcon.addFile(":/resources/icons/svg/search_dark.svg", QSize(),
                     QIcon::Normal, QIcon::Off);
  search_button_->setIcon(searchIcon);
  search_button_->setIconSize(QSize(16, 16));

  case_sensitive_check_ = new QCheckBox(QStringLiteral("Aa"), this);
  case_sensitive_check_->setToolTip(QStringLiteral("区分大小写"));
  case_sensitive_check_->setFixedSize(30, 26);

  inputRow->addWidget(search_input_);
  inputRow->addWidget(search_button_);
  inputRow->addWidget(case_sensitive_check_);

  mainLayout->addLayout(inputRow);

  status_label_ = new QLabel(this);
  status_label_->setContentsMargins(8, 2, 8, 4);
  status_label_->hide();

  mainLayout->addWidget(status_label_);

  result_tree_ = new QTreeWidget(this);
  result_tree_->setHeaderHidden(true);
  result_tree_->setIndentation(12);
  result_tree_->setRootIsDecorated(true);
  result_tree_->setUniformRowHeights(true);
  result_tree_->setSelectionMode(QAbstractItemView::SingleSelection);

  mainLayout->addWidget(result_tree_);
}

void SearchWidget::initSignals() {
  connect(search_input_, &QLineEdit::returnPressed, this,
          &SearchWidget::performSearch);
  connect(search_button_, &QPushButton::clicked, this,
          &SearchWidget::performSearch);

  connect(result_tree_, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem* item) {
            if (item->parent() == nullptr) return;

            QString filePath = item->data(0, Qt::UserRole).toString();
            int line = item->data(0, Qt::UserRole + 1).toInt();
            if (!filePath.isEmpty() && line > 0) {
              emit fileOpenRequested(filePath, line);
            }
          });
}

void SearchWidget::performSearch() {
  QString query = search_input_->text();
  if (query.isEmpty()) {
    clearResults();
    return;
  }

  if (search_root_.isEmpty()) {
    clearResults();
    status_label_->setText(QStringLiteral("未打开项目"));
    status_label_->show();
    return;
  }

  clearResults();

  bool caseSensitive = case_sensitive_check_->isChecked();
  Qt::CaseSensitivity cs =
      caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

  QStringList skipDirs = {".git",    ".svn",          "build",
                          "cmake-build-", "node_modules",  "__pycache__",
                          ".vs"};

  QStringList binarySuffixes = {
      "o",   "obj", "exe", "dll", "so",  "a",    "lib",  "png",
      "jpg", "jpeg", "gif", "bmp", "ico", "pdf",  "zip",  "tar",
      "gz",  "7z",  "ttf", "woff", "woff2", "db", "sqlite"};

  int totalMatches = 0;
  int filesWithMatches = 0;
  bool truncated = false;

  QDirIterator it(search_root_, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    QString filePath = it.next();
    QFileInfo fi(filePath);

    bool skip = false;
    QString absPath = fi.absolutePath();
    for (const auto& pattern : skipDirs) {
      if (absPath.contains(pattern)) {
        skip = true;
        break;
      }
    }
    if (skip) continue;

    QString suffix = fi.suffix().toLower();
    if (binarySuffixes.contains(suffix)) continue;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    QTreeWidgetItem* fileItem = nullptr;
    int lineNumber = 0;
    int fileMatches = 0;

    while (!stream.atEnd()) {
      QString line = stream.readLine();
      ++lineNumber;

      if (line.contains(query, cs)) {
        if (!fileItem) {
          fileItem = new QTreeWidgetItem(result_tree_);
          fileItem->setText(0, fi.fileName());
          fileItem->setToolTip(0, filePath);
          ++filesWithMatches;
        }

        QString displayLine = line.trimmed();
        if (displayLine.length() > 120) {
          displayLine = displayLine.left(117) + "...";
        }

        auto* matchItem = new QTreeWidgetItem(fileItem);
        matchItem->setText(
            0, QStringLiteral("%1: %2").arg(lineNumber).arg(displayLine));
        matchItem->setData(0, Qt::UserRole, filePath);
        matchItem->setData(0, Qt::UserRole + 1, lineNumber);

        ++fileMatches;
        ++totalMatches;

        if (totalMatches >= kMaxResults) {
          truncated = true;
          break;
        }
      }
    }

    if (fileItem) {
      fileItem->setText(
          0, QStringLiteral("%1 (%2)").arg(fi.fileName()).arg(fileMatches));
      fileItem->setExpanded(true);
    }

    if (truncated) break;
  }

  QString statusText;
  if (truncated) {
    statusText = QStringLiteral("找到超过 %1 个结果（%2 个文件中），结果已截断")
                     .arg(kMaxResults)
                     .arg(filesWithMatches);
  } else {
    statusText = QStringLiteral("找到 %1 个结果（%2 个文件中）")
                     .arg(totalMatches)
                     .arg(filesWithMatches);
  }
  status_label_->setText(statusText);
  status_label_->show();

  LOG_INFO("SEARCH", "搜索 '{}' 找到 {} 个结果（{} 个文件中）",
           query.toStdString(), totalMatches, filesWithMatches);
}

void SearchWidget::clearResults() {
  result_tree_->clear();
  status_label_->hide();
}

}  // namespace etest::app
