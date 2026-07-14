#ifndef ETEST_APP_APPSTATUSBARCONTROLLER_H_
#define ETEST_APP_APPSTATUSBARCONTROLLER_H_

#include <QLabel>
#include <QObject>
#include <QStatusBar>

namespace etest::app {

class AppStatusBarController : public QObject {
  Q_OBJECT
 public:
  explicit AppStatusBarController(QObject* parent = nullptr);

  void setup(QStatusBar* status_bar);

 public slots:
  void setProject(const QString& name);
  void setEngineState(const QString& text);
  void setExecStats(int pass, int fail, int elapsed);
  void setCursorPos(int line, int col);
  void setEncoding(const QString& enc);
  void setEol(const QString& eol);
  void setLanguage(const QString& lang);
  void setMessage(const QString& msg);
  void setErrorsWarnings(int errors, int warnings);

 private:
  QStatusBar* status_bar_ = nullptr;

  QLabel* label_message_ = nullptr;
  QLabel* label_project_ = nullptr;
  QLabel* label_errors_ = nullptr;
  QLabel* label_cursor_ = nullptr;
  QLabel* label_encoding_ = nullptr;
  QLabel* label_eol_ = nullptr;
  QLabel* label_language_ = nullptr;
  QLabel* label_engine_state_ = nullptr;
  QLabel* label_exec_stats_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_APPSTATUSBARCONTROLLER_H_
