#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "user32.lib")
#endif
#include "SingleInstance.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QWidget>

#include "logger/Logger.h"

using namespace etest::core::logger;

namespace etest::core::common {

SingleInstance::SingleInstance(const QString& appKey, QObject* parent)
    : QObject(parent), app_key_(appKey), shared_memory_(appKey) {
  server_name_ =
      QString("%1-%2").arg(appKey).arg(QCoreApplication::applicationPid());
}

SingleInstance::~SingleInstance() {
  if (local_server_ && local_server_->isListening()) {
    local_server_->close();
  }
  delete local_server_;

  if (shared_memory_.isAttached()) {
    shared_memory_.detach();
  }
}

bool SingleInstance::isAppAlreadyRunning() {
  QLocalSocket socket;
  socket.connectToServer(app_key_, QIODevice::ReadWrite);
  if (socket.waitForConnected(CONNECTION_TIMEOUT)) {
    socket.disconnectFromServer();
    return true;
  }

  if (shared_memory_.attach()) {
    shared_memory_.detach();
    return true;
  }
  return false;
}

bool SingleInstance::connectToExistingInstance(const QStringList& arguments) {
  QLocalSocket socket;
  socket.connectToServer(app_key_, QIODevice::WriteOnly);
  if (!socket.waitForConnected(CONNECTION_TIMEOUT)) {
    return false;
  }

  QByteArray message;
  QDataStream stream(&message, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_5_12);

  stream << (quint32)1;
  stream << (quint32)arguments.size();
  for (const auto& arg : arguments) {
    stream << arg;
  }
  qint64 bytesWritten = socket.write(message);
  socket.flush();
  socket.waitForBytesWritten(CONNECTION_TIMEOUT);
  socket.disconnectFromServer();
  return bytesWritten > 0;
}

bool SingleInstance::startListening() {
  QLocalServer::removeServer(app_key_);

  local_server_ = new QLocalServer(this);
  connect(local_server_, &QLocalServer::newConnection, this,
          &SingleInstance::handleNewConnection);

  if (!local_server_->listen(app_key_)) {
    LOG_WARN("SingleInstance", "无法启动本地服务: {}",
             local_server_->errorString().toStdString());
    if (local_server_->serverError() == QAbstractSocket::AddressInUseError) {
      QLocalServer::removeServer(app_key_);
      if (!local_server_->listen(app_key_)) {
        LOG_ERROR("SingleInstance", "重试监听失败: {}",
                  local_server_->errorString().toStdString());
        return false;
      }
    } else {
      return false;
    }
  }

  if (!shared_memory_.create(1)) {
    LOG_WARN("SingleInstance", "无法创建共享内存，但本地服务已启动: {}",
             shared_memory_.errorString().toStdString());
  }

  LOG_INFO("SingleInstance", "单实例服务器启动成功");
  return true;
}

void SingleInstance::setActivationWindow(QWidget* window) {
  activate_window_ = window;
}

QWidget* SingleInstance::activationWindow() const {
  return activate_window_;
}

void SingleInstance::handleNewConnection() {
  auto* socket = local_server_->nextPendingConnection();
  if (socket) {
    connect(socket, &QLocalSocket::readyRead, this,
            &SingleInstance::handleSocketReadyRead);
    connect(socket, &QLocalSocket::disconnected, socket,
            &QLocalSocket::deleteLater);
    connect(socket,
            QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, &SingleInstance::handleSocketError);
  }
}

void SingleInstance::handleSocketReadyRead() {
  auto* socket = qobject_cast<QLocalSocket*>(QObject::sender());
  if (!socket) {
    return;
  }
  auto data = socket->readAll();
  QStringList arguments = parseMessage(data);
  if (!arguments.isEmpty()) {
    emit newInstanceLaunched(arguments);
    activateWindow();
  }
}

void SingleInstance::handleSocketError(QLocalSocket::LocalSocketError errCode) {
  auto* socket = qobject_cast<QLocalSocket*>(QObject::sender());
  if (socket) {
    LOG_WARN("SingleInstance", "Socket错误: {}, 错误码: {}",
             socket->errorString().toStdString(), (int)errCode);
  }
}

QStringList SingleInstance::parseMessage(const QByteArray& message) {
  QStringList arguments;
  QDataStream stream(message);
  stream.setVersion(QDataStream::Qt_5_12);

  quint32 protocolVersion = 0;
  quint32 argCount = 0;
  stream >> protocolVersion;
  if (protocolVersion != 1) {
    LOG_WARN("SingleInstance", "不支持的协议版本: {}", protocolVersion);
    return arguments;
  }
  stream >> argCount;
  for (quint32 i = 0; i < argCount; i++) {
    QString arg;
    stream >> arg;
    arguments.append(arg);
  }
  return arguments;
}

void SingleInstance::activateWindow() {
  if (!activate_window_) {
    emit showApplication();
    return;
  }
#ifdef _WIN32
  HWND hwnd = (HWND)activate_window_->winId();
  if (IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  }
  SetForegroundWindow(hwnd);
  BringWindowToTop(hwnd);
  SetActiveWindow(hwnd);
#else
  activate_window_->raise();
  activate_window_->activateWindow();
  activate_window_->showNormal();
#endif

  emit showApplication();
}

}  // namespace etest::core::common
