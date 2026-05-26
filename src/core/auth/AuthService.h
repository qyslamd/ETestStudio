#ifndef ETEST_CORE_AUTH_AUTH_SERVICE_H_
#define ETEST_CORE_AUTH_AUTH_SERVICE_H_

#include <QObject>
#include <QSet>
#include "AuthTypes.h"

namespace etest::core::auth {

class AuthService : public QObject {
  Q_OBJECT

 public:
  static AuthService& instance();

  bool login(const QString& username, const QString& password);
  void logout();
  bool isLoggedIn() const;
  User currentUser() const;
  UserRole currentRole() const;
  bool hasPermission(const QString& permission) const;

 signals:
  void loginSucceeded(const User& user);
  void loginFailed(const QString& reason);
  void loggedOut();

 private:
  AuthService();
  ~AuthService() override = default;
  AuthService(const AuthService&) = delete;
  AuthService& operator=(const AuthService&) = delete;
  QSet<QString> permissionsForRole(UserRole role) const;

  bool loggedIn_ = false;
  User currentUser_;
};

}  // namespace etest::core::auth

#endif  // ETEST_CORE_AUTH_AUTH_SERVICE_H_
