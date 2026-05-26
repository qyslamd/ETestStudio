#include "UserManager.h"
#include "PasswordHasher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace etest::core::auth {

UserManager& UserManager::instance() {
  static UserManager inst;
  return inst;
}

QString UserManager::dataFilePath() const {
  return QDir(QStandardPaths::writableLocation(
      QStandardPaths::AppDataLocation))
      .absoluteFilePath("users.json");
}

bool UserManager::loadUsers() {
  users_.clear();
  QString path = dataFilePath();
  if (!QFile::exists(path)) {
    auto hasher = HasherFactory::create(HASHER_PLAIN);
    users_.append(User{"admin", hasher->hash("admin123"), UserRole::Admin});
    return saveUsers();
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    lastError_ = "无法打开用户文件";
    return false;
  }
  QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromBase64(file.readAll()));
  file.close();
  if (!doc.isArray()) {
    lastError_ = "用户文件格式错误";
    return false;
  }
  for (const auto& val : doc.array()) {
    QJsonObject obj = val.toObject();
    users_.append(User{obj["UserName"].toString(),
                       obj["Password"].toString(),
                       static_cast<UserRole>(obj["Role"].toInt())});
  }
  return true;
}

bool UserManager::saveUsers() {
  QDir().mkpath(QFileInfo(dataFilePath()).absolutePath());
  QFile file(dataFilePath());
  if (!file.open(QIODevice::WriteOnly)) {
    lastError_ = "无法写入用户文件";
    return false;
  }
  QJsonArray arr;
  for (const auto& u : users_) {
    QJsonObject obj;
    obj["UserName"] = u.userName;
    obj["Password"] = u.password;
    obj["Role"] = static_cast<int>(u.role);
    arr.append(obj);
  }
  file.write(QJsonDocument(arr).toJson().toBase64());
  file.close();
  return true;
}

QList<User> UserManager::allUsers() const { return users_; }

User UserManager::authenticate(const QString& username,
                               const QString& password) const {
  auto hasher = HasherFactory::create(HASHER_PLAIN);
  for (const auto& u : users_) {
    if (u.userName == username && hasher->verify(password, u.password))
      return u;
  }
  lastError_ = "用户名或密码错误";
  return User();
}

bool UserManager::addUser(const QString& username, const QString& password,
                          UserRole role) {
  if (findUserIndex(username) >= 0) {
    lastError_ = "用户已存在";
    return false;
  }
  auto hasher = HasherFactory::create(HASHER_PLAIN);
  users_.append(User{username, hasher->hash(password), role});
  return saveUsers();
}

bool UserManager::updateUser(const QString& username,
                             const QString& newPassword, UserRole newRole) {
  int idx = findUserIndex(username);
  if (idx < 0) {
    lastError_ = "用户不存在";
    return false;
  }
  auto& u = users_[idx];
  auto hasher = HasherFactory::create(HASHER_PLAIN);
  if (!newPassword.isEmpty())
    u.password = hasher->hash(newPassword);
  u.role = newRole;
  return saveUsers();
}

bool UserManager::deleteUser(const QString& username) {
  if (username == "admin") {
    lastError_ = "admin 用户不可删除";
    return false;
  }
  int idx = findUserIndex(username);
  if (idx < 0) {
    lastError_ = "用户不存在";
    return false;
  }
  users_.removeAt(idx);
  return saveUsers();
}

QString UserManager::lastError() const { return lastError_; }

int UserManager::findUserIndex(const QString& username) const {
  for (int i = 0; i < users_.size(); ++i) {
    if (users_[i].userName == username)
      return i;
  }
  return -1;
}

}  // namespace etest::core::auth
