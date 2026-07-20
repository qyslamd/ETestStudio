#ifndef ETEST_CORE_UTILS_FILE_UTIL_H_
#define ETEST_CORE_UTILS_FILE_UTIL_H_

#include <QString>
#include <QStringList>

#include <filesystem>

namespace etest {
namespace core {
namespace utils {

inline std::filesystem::path toFsPath(const QString& qpath) {
#ifdef Q_OS_WIN
  return std::filesystem::path(qpath.toStdWString());
#else
  return std::filesystem::path(qpath.toUtf8().constData());
#endif
}

class FileUtil {
 public:
  static bool exists(const QString& path);
  static bool isFile(const QString& path);
  static bool isDirectory(const QString& path);
  static bool createDirectory(const QString& path);
  static bool remove(const QString& path);
  static bool copy(const QString& src, const QString& dst);
  static bool move(const QString& src, const QString& dst);
  static qint64 fileSize(const QString& path);
  static QString readTextFile(const QString& path);
  static bool writeTextFile(const QString& path, const QString& content);
  static QString computeMD5(const QString& path);
  static QString computeSHA256(const QString& path);

  static QStringList listFiles(const QString& dirPath, bool recursive = false);
  static QString fileName(const QString& path);
  static QString fileExtension(const QString& path);
  static QString parentPath(const QString& path);
};

}  // namespace utils
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_UTILS_FILE_UTIL_H_