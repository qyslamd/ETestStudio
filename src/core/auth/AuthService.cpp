#include "AuthService.h"
#include "UserManager.h"

namespace etest::core::auth {

AuthService& AuthService::instance() {
  static AuthService inst;
  return inst;
}

AuthService::AuthService() : QObject(nullptr) {}

bool AuthService::login(const QString& username, const QString& password) {
  UserManager::instance().loadUsers();
  User user = UserManager::instance().authenticate(username, password);
  if (user.userName.isEmpty()) {
    emit loginFailed(UserManager::instance().lastError());
    return false;
  }
  loggedIn_ = true;
  currentUser_ = user;
  emit loginSucceeded(user);
  return true;
}

void AuthService::logout() {
  loggedIn_ = false;
  currentUser_ = User();
  emit loggedOut();
}

bool AuthService::isLoggedIn() const { return loggedIn_; }
User AuthService::currentUser() const { return currentUser_; }
UserRole AuthService::currentRole() const { return currentUser_.role; }

bool AuthService::hasPermission(const QString& permission) const {
  if (!loggedIn_) return false;
  return permissionsForRole(currentUser_.role).contains(permission);
}

QSet<QString> AuthService::permissionsForRole(UserRole role) const {
  QSet<QString> perms;
  if (role == UserRole::Admin)
    perms.insert(PERM_USER_MANAGE);
  return perms;
}

}  // namespace etest::core::auth
