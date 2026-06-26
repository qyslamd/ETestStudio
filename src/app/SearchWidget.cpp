#include "SearchWidget.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QRegularExpression>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "ThemeManager.h"

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
  mainLayout->setSpacing(2);

  auto* inputRow = new QHBoxLayout();
  inputRow->setContentsMargins(8, 6, 8, 4);
  inputRow->setSpacing(4);

  search_input_ = new QLineEdit(this);
  search_input_->setPlaceholderText(QStringLiteral("搜索"));
  search_input_->setClearButtonEnabled(true);

  search_button_ = new QPushButton(this);
  search_button_->setToolTip(QStringLiteral("搜索"));
  search_button_->setFixedSize(26, 26);
  search_button_->setIcon(AppIconProvider::instance().icon("search"));
  search_button_->setIconSize(QSize(16, 16));

  case_sensitive_btn_ = new QToolButton(this);
  case_sensitive_btn_->setText(QStringLiteral("Aa"));
  case_sensitive_btn_->setToolTip(QStringLiteral("区分大小写"));
  case_sensitive_btn_->setCheckable(true);
  case_sensitive_btn_->setFixedSize(28, 26);
  case_sensitive_btn_->setObjectName("searchCaseSensitiveBtn");

  whole_word_btn_ = new QToolButton(this);
  whole_word_btn_->setText(QStringLiteral("ab"));
  whole_word_btn_->setToolTip(QStringLiteral("全词匹配"));
  whole_word_btn_->setCheckable(true);
  whole_word_btn_->setFixedSize(28, 26);
  whole_word_btn_->setObjectName("searchWholeWordBtn");
  QFont wwFont = whole_word_btn_->font();
  wwFont.setUnderline(true);
  whole_word_btn_->setFont(wwFont);

  inputRow->addWidget(search_input_);
  inputRow->addWidget(search_button_);
  inputRow->addWidget(case_sensitive_btn_);
  inputRow->addWidget(whole_word_btn_);

  mainLayout->addLayout(inputRow);

  auto* separator = new QFrame(this);
  separator->setFrameShape(QFrame::HLine);
  separator->setFixedHeight(1);
  mainLayout->addWidget(separator);

  status_label_ = new QLabel(this);
  status_label_->setContentsMargins(8, 2, 8, 4);
  status_label_->hide();

  mainLayout->addWidget(status_label_);

  splitter_ = new QSplitter(Qt::Vertical, this);
  splitter_->setChildrenCollapsible(false);

  // 上方：文件名匹配列表
  auto* fileSection = new QWidget(splitter_);
  auto* fileLayout = new QVBoxLayout(fileSection);
  fileLayout->setContentsMargins(0, 0, 0, 0);
  fileLayout->setSpacing(0);

  auto* fileSectionLabel = new QLabel(QStringLiteral("文件"), fileSection);
  fileSectionLabel->setObjectName(QStringLiteral("fileSectionLabel"));
  fileLayout->addWidget(fileSectionLabel);

  file_list_ = new QListWidget(fileSection);
  file_list_->setUniformItemSizes(true);
  file_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  fileLayout->addWidget(file_list_);

  splitter_->addWidget(fileSection);

  // 下方：内容匹配结果树
  auto* contentSection = new QWidget(splitter_);
  auto* contentLayout = new QVBoxLayout(contentSection);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  auto* contentSectionLabel =
      new QLabel(QStringLiteral("内容"), contentSection);
  contentSectionLabel->setObjectName(QStringLiteral("contentSectionLabel"));
  contentLayout->addWidget(contentSectionLabel);

  result_tree_ = new QTreeWidget(contentSection);
  result_tree_->setHeaderHidden(true);
  result_tree_->setIndentation(12);
  result_tree_->setRootIsDecorated(true);
  result_tree_->setUniformRowHeights(true);
  result_tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  contentLayout->addWidget(result_tree_);

  splitter_->addWidget(contentSection);

  splitter_->setStretchFactor(0, 1);
  splitter_->setStretchFactor(1, 2);

  mainLayout->addWidget(splitter_);
}

void SearchWidget::initSignals() {
  connect(search_input_, &QLineEdit::returnPressed, this,
          &SearchWidget::performSearch);
  connect(search_button_, &QPushButton::clicked, this,
          &SearchWidget::performSearch);

  // Theme change: refresh search icon
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this](bool) {
            search_button_->setIcon(AppIconProvider::instance().icon("search"));
          });

  connect(result_tree_, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem* item) {
            if (item->parent() == nullptr) return;

            QString filePath = item->data(0, Qt::UserRole).toString();
            int line = item->data(0, Qt::UserRole + 1).toInt();
            if (!filePath.isEmpty() && line > 0) {
              emit fileOpenRequested(filePath, line);
            }
          });

  connect(file_list_, &QListWidget::itemClicked, this,
          [this](QListWidgetItem* item) {
            QString filePath = item->data(Qt::UserRole).toString();
            if (!filePath.isEmpty()) {
              emit fileOpenRequested(filePath, 0);
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

  bool caseSensitive = case_sensitive_btn_->isChecked();
  Qt::CaseSensitivity cs =
      caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

  bool wholeWord = whole_word_btn_->isChecked();
  QRegularExpression regex;
  if (wholeWord) {
    QString pattern =
        QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(query));
    regex.setPattern(pattern);
    regex.setPatternOptions(caseSensitive
                                ? QRegularExpression::NoPatternOption
                                : QRegularExpression::CaseInsensitiveOption);
  }

  QStringList skipDirs = {".git",    ".svn",          "build",
                          "cmake-build-", "node_modules",  "__pycache__",
                          ".vs"};

  QStringList binarySuffixes = {
      "o",   "obj", "exe", "dll", "so",  "a",    "lib",  "png",
      "jpg", "jpeg", "gif", "bmp", "ico", "pdf",  "zip",  "tar",
      "gz",  "7z",  "ttf", "woff", "woff2", "db", "sqlite"};

  int totalMatches = 0;
  int filesWithMatches = 0;
  int fileMatches = 0;
  bool truncated = false;
  bool fileTruncated = false;

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

    // 文件名匹配（不跳过二进制文件，仅匹配名字）
    bool nameMatched = wholeWord ? regex.match(fi.fileName()).hasMatch()
                                 : fi.fileName().contains(query, cs);
    if (!fileTruncated && nameMatched) {
      QString relPath = QDir(search_root_).relativeFilePath(filePath);
      auto* item = new QListWidgetItem(file_list_);
      item->setText(relPath);
      item->setToolTip(filePath);
      item->setData(Qt::UserRole, filePath);
      item->setIcon(
          AppIconProvider::instance().icon(fileIconName(suffix)));
      ++fileMatches;
      if (fileMatches >= kMaxFileResults) {
        fileTruncated = true;
      }
    }

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

      bool lineMatched = wholeWord ? regex.match(line).hasMatch()
                                   : line.contains(query, cs);
      if (lineMatched) {
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

  QStringList statusParts;

  if (fileTruncated) {
    statusParts << QStringLiteral("文件名匹配超过 %1 个，已截断").arg(kMaxFileResults);
  } else if (fileMatches > 0) {
    statusParts << QStringLiteral("文件名匹配 %1 个").arg(fileMatches);
  }

  if (truncated) {
    statusParts << QStringLiteral("内容匹配超过 %1 个结果（%2 个文件中），已截断")
                       .arg(kMaxResults)
                       .arg(filesWithMatches);
  } else if (totalMatches > 0) {
    statusParts << QStringLiteral("内容匹配 %1 个结果（%2 个文件中）")
                       .arg(totalMatches)
                       .arg(filesWithMatches);
  }

  if (statusParts.isEmpty()) {
    status_label_->setText(QStringLiteral("未找到匹配结果"));
  } else {
    status_label_->setText(statusParts.join(QStringLiteral("，")));
  }
  status_label_->show();

  LOG_INFO("SEARCH", "搜索 '{}' 找到 {} 个文件名匹配, {} 个内容结果（{} 个文件中）",
           query.toStdString(), fileMatches, totalMatches, filesWithMatches);
}

void SearchWidget::clearResults() {
  file_list_->clear();
  result_tree_->clear();
  status_label_->hide();
}

QString SearchWidget::fileIconName(const QString& suffix) const {
  QString s = suffix.toLower();
  if (s == QStringLiteral("eproto")) {
    return QStringLiteral("file_eproto");
  } else if (s == QStringLiteral("etopo")) {
    return QStringLiteral("file_etopo");
  } else if (s == QStringLiteral("json")) {
    return QStringLiteral("file_json");
  } else if (s == QStringLiteral("lua")) {
    return QStringLiteral("file_lua");
  } else if (s == QStringLiteral("xml")) {
    return QStringLiteral("file_xml");
  } else if (s == QStringLiteral("yaml") || s == QStringLiteral("yml")) {
    return QStringLiteral("file_yaml");
  } else if (s == QStringLiteral("cpp") || s == QStringLiteral("cxx") ||
             s == QStringLiteral("cc")) {
    return QStringLiteral("file_cpp");
  } else if (s == QStringLiteral("cmake")) {
    return QStringLiteral("file_cmake");
  } else if (s == QStringLiteral("md")) {
    return QStringLiteral("file_markdown");
  } else if (s == QStringLiteral("py")) {
    return QStringLiteral("file_python");
  } else if (s == QStringLiteral("js")) {
    return QStringLiteral("file_js");
  } else {
    return QStringLiteral("file_generic");
  }
}

}  // namespace etest::app
