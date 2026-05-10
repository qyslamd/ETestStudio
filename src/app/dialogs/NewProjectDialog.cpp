#include "NewProjectDialog.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

using namespace etest::core::config;

namespace etest::app {

NewProjectDialog::NewProjectDialog(QWidget* parent) : AnimationDialog(parent) {
  initUi();
  initSignals();
}

NewProjectDialog::~NewProjectDialog() = default;

QString NewProjectDialog::projectName() const {
  return name_edit_->text().trimmed();
}

QString NewProjectDialog::projectLocation() const {
  // 返回绝对路径
  return QDir(location_edit_->text().trimmed()).absolutePath();
}

QString NewProjectDialog::fullProjectPath() const {
  // 返回绝对路径
  return QDir(QDir(projectLocation()).absolutePath()).filePath(projectName());
}

void NewProjectDialog::initUi() {
  setWindowTitle(QStringLiteral("新建项目"));

  auto* contentWidget = new QWidget(this);
  contentWidget->setObjectName("dlg_contentWidget");
  contentWidget->setStyleSheet(
      "QWidget#dlg_contentWidget { background-color: white;border: 1px solid "
      "#ccc; border-radius: 8px; }");
  contentWidget->setMinimumWidth(420);

  auto* mainLayout = new QVBoxLayout(contentWidget);

  // 项目名称
  auto* nameLabel = new QLabel(QStringLiteral("项目名称："), contentWidget);
  name_edit_ = new QLineEdit(contentWidget);
  name_edit_->setPlaceholderText(QStringLiteral("输入项目名称"));
  mainLayout->addWidget(nameLabel);
  mainLayout->addWidget(name_edit_);

  // 项目位置
  auto* locationLabel = new QLabel(QStringLiteral("项目位置："), contentWidget);
  auto* locationLayout = new QHBoxLayout();
  location_edit_ = new QLineEdit(contentWidget);
  location_edit_->setPlaceholderText(QStringLiteral("选择项目存储位置"));
  browse_button_ = new QPushButton(QStringLiteral("浏览..."), contentWidget);
  locationLayout->addWidget(location_edit_);
  locationLayout->addWidget(browse_button_);
  mainLayout->addWidget(locationLabel);
  mainLayout->addLayout(locationLayout);

  // 路径预览
  preview_label_ = new QLabel(contentWidget);
  preview_label_->setObjectName("previewLabel");
  mainLayout->addWidget(preview_label_);

  // 错误提示
  error_label_ = new QLabel(contentWidget);
  error_label_->setObjectName("errorLabel");
  error_label_->setWordWrap(true);
  mainLayout->addWidget(error_label_);

  // 按钮
  auto* buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();
  cancel_button_ = new QPushButton(QStringLiteral("取消"), contentWidget);
  create_button_ = new QPushButton(QStringLiteral("创建"), contentWidget);
  create_button_->setEnabled(false);
  create_button_->setDefault(true);
  buttonLayout->addWidget(cancel_button_);
  buttonLayout->addWidget(create_button_);
  mainLayout->addLayout(buttonLayout);

  setWidget(contentWidget);

  // 默认位置
  auto& cfg = ConfigManager::instance();
  QString lastPath = cfg.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
  if (lastPath.isEmpty()) {
    lastPath = QDir::homePath();
  }
  location_edit_->setText(lastPath);

  validateInputs();
}

void NewProjectDialog::initSignals() {
  connect(name_edit_, &QLineEdit::textChanged, this,
          &NewProjectDialog::onTextChanged);
  connect(location_edit_, &QLineEdit::textChanged, this,
          &NewProjectDialog::onTextChanged);
  connect(browse_button_, &QPushButton::clicked, this,
          &NewProjectDialog::onBrowseClicked);
  connect(create_button_, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
}

void NewProjectDialog::onBrowseClicked() {
  QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("选择项目位置"), location_edit_->text());
  if (!dir.isEmpty()) {
    location_edit_->setText(dir);
  }
}

void NewProjectDialog::onTextChanged() {
  validateInputs();
  updatePreview();
}

void NewProjectDialog::validateInputs() {
  QString name = name_edit_->text().trimmed();
  QString location = location_edit_->text().trimmed();

  if (name.isEmpty()) {
    error_label_->setText(QStringLiteral("请输入项目名称"));
    create_button_->setEnabled(false);
    return;
  }

  // 平台无关的非法字符检查
  static const QString invalidChars = "\\/:*?\"<>|";
  for (const QChar& c : name) {
    if (invalidChars.contains(c)) {
      error_label_->setText(
          QStringLiteral("项目名称包含非法字符：\\/:*?\"<>|"));
      create_button_->setEnabled(false);
      return;
    }
  }

  if (location.isEmpty()) {
    error_label_->setText(QStringLiteral("请选择项目位置"));
    create_button_->setEnabled(false);
    return;
  }

  // 使用绝对路径进行验证
  QString absLocation = projectLocation();  // 已经是绝对路径

  QDir dir(absLocation);
  if (!dir.exists()) {
    error_label_->setText(QStringLiteral("所选位置路径不存在"));
    create_button_->setEnabled(false);
    return;
  }

  // 检查目录是否可写
  QFileInfo dirInfo(absLocation);
  if (!dirInfo.isWritable()) {
    error_label_->setText(QStringLiteral("所选位置不可写，请检查权限"));
    create_button_->setEnabled(false);
    return;
  }

  QString fullPath = fullProjectPath();  // 使用绝对路径
  if (QDir(fullPath).exists()) {
    error_label_->setText(QStringLiteral("目标目录已存在：%1").arg(fullPath));
    create_button_->setEnabled(false);
    return;
  }

  error_label_->clear();
  create_button_->setEnabled(true);
}

void NewProjectDialog::updatePreview() {
  QString name = name_edit_->text().trimmed();
  QString location = location_edit_->text().trimmed();

  if (name.isEmpty() || location.isEmpty()) {
    preview_label_->clear();
    return;
  }

  preview_label_->setText(
      QStringLiteral("项目路径：%1").arg(fullProjectPath()));
}

}  // namespace etest::app