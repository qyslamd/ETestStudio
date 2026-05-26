#pragma once

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
