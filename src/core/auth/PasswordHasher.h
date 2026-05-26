#ifndef ETEST_CORE_AUTH_PASSWORD_HASHER_H_
#define ETEST_CORE_AUTH_PASSWORD_HASHER_H_

#include <QString>
#include <memory>

namespace etest::core::auth {

inline constexpr const char* HASHER_PLAIN = "plain";

class PasswordHasher {
 public:
  virtual ~PasswordHasher() = default;
  virtual QString hash(const QString& password) const = 0;
  virtual bool verify(const QString& password,
                      const QString& storedHash) const = 0;
};

class PlainTextHasher : public PasswordHasher {
 public:
  QString hash(const QString& password) const override;
  bool verify(const QString& password,
              const QString& storedHash) const override;
};

class HasherFactory {
 public:
  static std::unique_ptr<PasswordHasher> create(const QString& algorithm);
};

}  // namespace etest::core::auth

#endif  // ETEST_CORE_AUTH_PASSWORD_HASHER_H_
