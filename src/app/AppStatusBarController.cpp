#include "AppStatusBarController.h"

#include <QLabel>
#include <QStatusBar>

namespace etest::app {

AppStatusBarController::AppStatusBarController(QObject* parent)
    : QObject(parent) {}

void AppStatusBarController::setup(QStatusBar* status_bar) {
  status_bar_ = status_bar;
  if (!status_bar_) {
    return;
  }

  // 左侧区域（addWidget 从左到右排列）
  label_message_ = new QLabel;
  label_message_->setText(QStringLiteral("就绪"));
  status_bar_->addWidget(label_message_);

  label_project_ = new QLabel;
  label_project_->setText(QStringLiteral("无打开项目"));
  status_bar_->addWidget(label_project_);

  label_errors_ = new QLabel;
  label_errors_->setText(QStringLiteral("0 错误, 0 警告"));
  status_bar_->addWidget(label_errors_);

  // 右侧区域（addPermanentWidget 添加到右侧，顺序从左到右）
  label_language_ = new QLabel;
  label_language_->setText(QStringLiteral("纯文本"));
  status_bar_->addPermanentWidget(label_language_);

  label_eol_ = new QLabel;
  label_eol_->setText(QStringLiteral("CRLF"));
  status_bar_->addPermanentWidget(label_eol_);

  label_encoding_ = new QLabel;
  label_encoding_->setText(QStringLiteral("UTF-8"));
  status_bar_->addPermanentWidget(label_encoding_);

  label_cursor_ = new QLabel;
  label_cursor_->setText(QStringLiteral("行 1, 列 1"));
  status_bar_->addPermanentWidget(label_cursor_);

  label_engine_state_ = new QLabel(QStringLiteral("空闲"));
  status_bar_->addPermanentWidget(label_engine_state_);

  label_exec_stats_ = new QLabel(QStringLiteral("✅ 0  ❌ 0  ⏱ 0s"));
  status_bar_->addPermanentWidget(label_exec_stats_);

  status_bar_->clearMessage();
}

void AppStatusBarController::setMessage(const QString& msg) {
  if (label_message_) {
    label_message_->setText(msg);
  }
}

void AppStatusBarController::setProject(const QString& name) {
  if (label_project_) {
    label_project_->setText(name);
  }
}

void AppStatusBarController::setEngineState(const QString& text) {
  if (label_engine_state_) {
    label_engine_state_->setText(text);
  }
}

void AppStatusBarController::setExecStats(int pass, int fail, int elapsed) {
  if (label_exec_stats_) {
    label_exec_stats_->setText(
        QStringLiteral("✅ %1  ❌ %2  ⏱ %3s")
            .arg(pass)
            .arg(fail)
            .arg(elapsed));
  }
}

void AppStatusBarController::setCursorPos(int line, int col) {
  if (label_cursor_) {
    label_cursor_->setText(
        QStringLiteral("行 %1, 列 %2").arg(line).arg(col));
  }
}

void AppStatusBarController::setEncoding(const QString& enc) {
  if (label_encoding_) {
    label_encoding_->setText(enc);
  }
}

void AppStatusBarController::setEol(const QString& eol) {
  if (label_eol_) {
    label_eol_->setText(eol);
  }
}

void AppStatusBarController::setLanguage(const QString& lang) {
  if (label_language_) {
    label_language_->setText(lang);
  }
}

void AppStatusBarController::setErrorsWarnings(int errors, int warnings) {
  if (label_errors_) {
    label_errors_->setText(
        QStringLiteral("%1 错误, %2 警告").arg(errors).arg(warnings));
  }
}

}  // namespace etest::app
