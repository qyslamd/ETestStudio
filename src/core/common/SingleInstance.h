#ifndef ETEST_CORE_COMMON_SINGLE_INSTANCE_H_
#define ETEST_CORE_COMMON_SINGLE_INSTANCE_H_

#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QSharedMemory>
#include <QStringList>

class QWidget;

namespace etest::core::common {

class SingleInstance : public QObject {
  Q_OBJECT

 public:
  explicit SingleInstance(const QString& appKey,
                          QObject* parent = nullptr);
  ~SingleInstance() override;

  bool isAppAlreadyRunning();
  bool connectToExistingInstance(const QStringList& arguments = {});
  bool startListening();

  void setActivationWindow(QWidget* window);
  QWidget* activationWindow() const;

  void activateWindow();

 signals:
  void newInstanceLaunched(const QStringList& arguments);
  void showApplication();

 private slots:
  void handleNewConnection();
  void handleSocketReadyRead();
  void handleSocketError(QLocalSocket::LocalSocketError err);

 private:
  void sendMessageToExistingInstance(const QByteArray& message);
  QStringList parseMessage(const QByteArray& message);

  QString app_key_;
  QWidget* activate_window_ = nullptr;
  QLocalServer* local_server_ = nullptr;
  QSharedMemory shared_memory_;
  QString server_name_;
  static const int CONNECTION_TIMEOUT = 1000;
};

}  // namespace etest::core::common

#endif  // ETEST_CORE_COMMON_SINGLE_INSTANCE_H_
