#ifndef ETEST_CORE_AUTH_AUTH_TYPES_H_
#define ETEST_CORE_AUTH_AUTH_TYPES_H_

#include <QString>

namespace etest::core::auth {

enum class UserRole { Admin = 0, User };

struct User {
  QString userName;
  QString password;
  UserRole role;

  User() : userName(), password(), role(UserRole::User) {}
  User(const QString& userName, const QString& password, UserRole role)
      : userName(userName), password(password), role(role) {}
};

inline constexpr const char* PERM_USER_MANAGE = "user.manage";

}  // namespace etest::core::auth

#endif  // ETEST_CORE_AUTH_AUTH_TYPES_H_
