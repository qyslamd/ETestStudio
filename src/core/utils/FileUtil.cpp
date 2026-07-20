#include "utils/FileUtil.h"

#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QFileInfo>

#include <filesystem>

#include "common/FileException.h"

namespace etest {
namespace core {
namespace utils {

namespace {

void throwFileException(::etest::core::common::FileException::Code code,
                        const QString& message,
                        const QString& path = QString()) {
  if (path.isEmpty()) {
    throw ::etest::core::common::FileException(code, message);
  } else {
    throw ::etest::core::common::FileException(code, message, path);
  }
}

}  // namespace

bool FileUtil::exists(const QString& path) {
  return std::filesystem::exists(toFsPath(path));
}

bool FileUtil::isFile(const QString& path) {
  return std::filesystem::is_regular_file(toFsPath(path));
}

bool FileUtil::isDirectory(const QString& path) {
  return std::filesystem::is_directory(toFsPath(path));
}

bool FileUtil::createDirectory(const QString& path) {
  try {
    return std::filesystem::create_directories(toFsPath(path));
  } catch (const std::filesystem::filesystem_error& e) {
    throwFileException(::etest::core::common::FileException::Code::kIoError,
                       QString::fromStdString(e.what()),
                       path);
    return false;
  }
}

bool FileUtil::remove(const QString& path) {
  try {
    return std::filesystem::remove_all(toFsPath(path)) > 0;
  } catch (const std::filesystem::filesystem_error& e) {
    throwFileException(::etest::core::common::FileException::Code::kIoError,
                       QString::fromStdString(e.what()),
                       path);
    return false;
  }
}

bool FileUtil::copy(const QString& src, const QString& dst) {
  try {
    std::filesystem::copy_file(toFsPath(src),
                                toFsPath(dst),
                               std::filesystem::copy_options::overwrite_existing);
    return true;
  } catch (const std::filesystem::filesystem_error& e) {
    throwFileException(::etest::core::common::FileException::Code::kIoError,
                       QString::fromStdString(e.what()),
                       src);
    return false;
  }
}

bool FileUtil::move(const QString& src, const QString& dst) {
  try {
    std::filesystem::rename(toFsPath(src), toFsPath(dst));
    return true;
  } catch (const std::filesystem::filesystem_error& e) {
    throwFileException(::etest::core::common::FileException::Code::kIoError,
                       QString::fromStdString(e.what()),
                       src);
    return false;
  }
}

qint64 FileUtil::fileSize(const QString& path) {
  try {
    return static_cast<qint64>(std::filesystem::file_size(toFsPath(path)));
  } catch (const std::filesystem::filesystem_error& e) {
    throwFileException(::etest::core::common::FileException::Code::kIoError,
                       QString::fromStdString(e.what()),
                       path);
    return -1;
  }
}

QString FileUtil::readTextFile(const QString& path) {
  QFile file(path);
  if (!file.exists()) {
    throwFileException(::etest::core::common::FileException::Code::kFileNotFound,
                       "File not found",
                       path);
  }

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    throwFileException(::etest::core::common::FileException::Code::kPermissionDenied,
                       "Cannot open file for reading",
                       path);
  }

  QTextStream in(&file);
  in.setCodec("UTF-8");
  QString content = in.readAll();
  file.close();
  return content;
}

bool FileUtil::writeTextFile(const QString& path, const QString& content) {
  QFileInfo fileInfo(path);
  QString dirPath = fileInfo.absolutePath();

  if (!exists(dirPath)) {
    createDirectory(dirPath);
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    throwFileException(::etest::core::common::FileException::Code::kPermissionDenied,
                       "Cannot open file for writing",
                       path);
    return false;
  }

  QTextStream out(&file);
  out.setCodec("UTF-8");
  out << content;
  file.close();
  return true;
}

QString FileUtil::computeMD5(const QString& path) {
  QFile file(path);
  if (!file.exists()) {
    throwFileException(::etest::core::common::FileException::Code::kFileNotFound,
                       "File not found",
                       path);
  }

  if (!file.open(QIODevice::ReadOnly)) {
    throwFileException(::etest::core::common::FileException::Code::kPermissionDenied,
                       "Cannot open file for reading",
                       path);
  }

  QCryptographicHash hash(QCryptographicHash::Md5);
  hash.addData(&file);
  file.close();

  return QString::fromLatin1(hash.result().toHex());
}

QString FileUtil::computeSHA256(const QString& path) {
  QFile file(path);
  if (!file.exists()) {
    throwFileException(::etest::core::common::FileException::Code::kFileNotFound,
                       "File not found",
                       path);
  }

  if (!file.open(QIODevice::ReadOnly)) {
    throwFileException(::etest::core::common::FileException::Code::kPermissionDenied,
                       "Cannot open file for reading",
                       path);
  }

  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(&file);
  file.close();

  return QString::fromLatin1(hash.result().toHex());
}

QStringList FileUtil::listFiles(const QString& dirPath, bool recursive) {
  QStringList result;

  try {
    if (recursive) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(
               toFsPath(dirPath))) {
        if (entry.is_regular_file()) {
          result.append(QString::fromStdString(entry.path().u8string()));
        }
      }
    } else {
      for (const auto& entry :
           std::filesystem::directory_iterator(toFsPath(dirPath))) {
        if (entry.is_regular_file()) {
          result.append(QString::fromStdString(entry.path().u8string()));
        }
      }
    }
  } catch (const std::filesystem::filesystem_error& e) {
    throwFileException(::etest::core::common::FileException::Code::kIoError,
                       QString::fromStdString(e.what()),
                       dirPath);
  }

  return result;
}

QString FileUtil::fileName(const QString& path) {
  return QFileInfo(path).fileName();
}

QString FileUtil::fileExtension(const QString& path) {
  return QFileInfo(path).suffix();
}

QString FileUtil::parentPath(const QString& path) {
  return QFileInfo(path).absolutePath();
}

}  // namespace utils
}  // namespace core
}  // namespace etest