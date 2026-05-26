#include "auth/AuthTypes.h"
#include "auth/PasswordHasher.h"
#include "auth/UserManager.h"
#include "auth/AuthService.h"
#include <gtest/gtest.h>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>

using namespace etest::core::auth;

TEST(AuthTest, PlainTextHasher) {
  PlainTextHasher hasher;
  QString hash = hasher.hash("hello");
  EXPECT_EQ(hash, "hello");
  EXPECT_TRUE(hasher.verify("hello", hash));
  EXPECT_FALSE(hasher.verify("world", hash));
}

TEST(AuthTest, HasherFactory) {
  auto hasher = HasherFactory::create("plain");
  EXPECT_TRUE(hasher->verify("test", hasher->hash("test")));
}

TEST(AuthTest, UserManagerCreateDefaultAdmin) {
  auto& um = UserManager::instance();
  QString path = QDir(QStandardPaths::writableLocation(
      QStandardPaths::AppDataLocation)).absoluteFilePath("users.json");
  QFile::remove(path);
  EXPECT_TRUE(um.loadUsers());
  auto users = um.allUsers();
  ASSERT_GE(users.size(), 1);
  EXPECT_EQ(users[0].userName, "admin");
  EXPECT_EQ(users[0].role, UserRole::Admin);
}

TEST(AuthTest, UserManagerAuthenticate) {
  auto& um = UserManager::instance();
  um.loadUsers();
  User admin = um.authenticate("admin", "admin123");
  EXPECT_EQ(admin.userName, "admin");
  EXPECT_EQ(admin.role, UserRole::Admin);
  User bad = um.authenticate("admin", "wrong");
  EXPECT_TRUE(bad.userName.isEmpty());
}

TEST(AuthTest, UserManagerAddDeleteUser) {
  auto& um = UserManager::instance();
  um.loadUsers();
  EXPECT_TRUE(um.addUser("testuser", "pass123", UserRole::User));
  auto users = um.allUsers();
  auto it = std::find_if(users.begin(), users.end(),
      [](const User& u) { return u.userName == "testuser"; });
  EXPECT_NE(it, users.end());
  EXPECT_FALSE(um.addUser("testuser", "pass456", UserRole::User));
  EXPECT_TRUE(um.deleteUser("testuser"));
  users = um.allUsers();
  it = std::find_if(users.begin(), users.end(),
      [](const User& u) { return u.userName == "testuser"; });
  EXPECT_EQ(it, users.end());
  EXPECT_FALSE(um.deleteUser("admin"));
}

TEST(AuthTest, UserManagerUpdateUser) {
  auto& um = UserManager::instance();
  um.loadUsers();
  EXPECT_TRUE(um.updateUser("admin", "newpass", UserRole::Admin));
  User admin = um.authenticate("admin", "newpass");
  EXPECT_EQ(admin.userName, "admin");
}

TEST(AuthTest, AuthServiceLoginLogout) {
  QString path = QDir(QStandardPaths::writableLocation(
      QStandardPaths::AppDataLocation)).absoluteFilePath("users.json");
  QFile::remove(path);
  auto& service = AuthService::instance();
  UserManager::instance().loadUsers();
  EXPECT_FALSE(service.isLoggedIn());
  EXPECT_FALSE(service.login("admin", "wrong"));
  EXPECT_FALSE(service.isLoggedIn());
  EXPECT_TRUE(service.login("admin", "admin123"));
  EXPECT_TRUE(service.isLoggedIn());
  EXPECT_EQ(service.currentUser().userName, "admin");
  EXPECT_EQ(service.currentRole(), UserRole::Admin);
  EXPECT_TRUE(service.hasPermission(PERM_USER_MANAGE));
  service.logout();
  EXPECT_FALSE(service.isLoggedIn());
  EXPECT_FALSE(service.hasPermission(PERM_USER_MANAGE));
}
