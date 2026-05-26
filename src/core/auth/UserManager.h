#pragma once

#include <QList>
#include <QString>
#include "AuthTypes.h"

namespace etest::core::auth {

class UserManager {
 public:
  static UserManager& instance();

  bool loadUsers();
  bool saveUsers();
  QList<User> allUsers() const;
  User authenticate(const QString& username,
                    const QString& password) const;
  bool addUser(const QString& username, const QString& password,
               UserRole role = UserRole::User);
  bool updateUser(const QString& username, const QString& newPassword,
                  UserRole newRole);
  bool deleteUser(const QString& username);
  QString lastError() const;

 private:
  UserManager() = default;
  ~UserManager() = default;
  UserManager(const UserManager&) = delete;
  UserManager& operator=(const UserManager&) = delete;
  QString dataFilePath() const;
  int findUserIndex(const QString& username) const;

  QList<User> users_;
  mutable QString lastError_;
};

}  // namespace etest::core::auth
