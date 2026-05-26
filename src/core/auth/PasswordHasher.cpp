#include "PasswordHasher.h"

namespace etest::core::auth {

QString PlainTextHasher::hash(const QString& password) const {
  return password;
}

bool PlainTextHasher::verify(const QString& password,
                             const QString& storedHash) const {
  return password == storedHash;
}

std::unique_ptr<PasswordHasher> HasherFactory::create(
    const QString& algorithm) {
  if (algorithm == "plain") {
    return std::make_unique<PlainTextHasher>();
  }
  return std::make_unique<PlainTextHasher>();
}

}  // namespace etest::core::auth
