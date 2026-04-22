#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QThread>

#include "utils/FileUtil.h"
#include "utils/TimeUtil.h"
#include "common/FileException.h"
#include "common/TimeException.h"

using etest::core::utils::FileUtil;
using etest::core::utils::TimeUtil;
using etest::core::common::FileException;
using etest::core::common::TimeException;

class FileUtilTest : public ::testing::Test {
 protected:
  QTemporaryDir tempDir;
};

TEST_F(FileUtilTest, Exists) {
  QString tempFile = tempDir.filePath("test.txt");
  EXPECT_FALSE(FileUtil::exists(tempFile));

  QFile file(tempFile);
  file.open(QIODevice::WriteOnly);
  file.close();

  EXPECT_TRUE(FileUtil::exists(tempFile));
}

TEST_F(FileUtilTest, IsFileAndIsDirectory) {
  QString tempFile = tempDir.filePath("test.txt");
  QFile file(tempFile);
  file.open(QIODevice::WriteOnly);
  file.close();

  EXPECT_TRUE(FileUtil::isFile(tempFile));
  EXPECT_FALSE(FileUtil::isDirectory(tempFile));
  EXPECT_TRUE(FileUtil::isDirectory(tempDir.path()));
}

TEST_F(FileUtilTest, CreateDirectory) {
  QString newDir = tempDir.filePath("subdir/nested");
  EXPECT_TRUE(FileUtil::createDirectory(newDir));
  EXPECT_TRUE(FileUtil::exists(newDir));
  EXPECT_TRUE(FileUtil::isDirectory(newDir));
}

TEST_F(FileUtilTest, FileSize) {
  QString tempFile = tempDir.filePath("test.txt");
  QFile file(tempFile);
  file.open(QIODevice::WriteOnly);
  file.write("Hello World");
  file.close();

  EXPECT_EQ(FileUtil::fileSize(tempFile), 11);
}

TEST_F(FileUtilTest, ReadWriteTextFile) {
  QString tempFile = tempDir.filePath("test.txt");
  QString content = "Hello World!\n测试中文";

  EXPECT_TRUE(FileUtil::writeTextFile(tempFile, content));
  EXPECT_EQ(FileUtil::readTextFile(tempFile), content);
}

TEST_F(FileUtilTest, CopyMove) {
  QString srcFile = tempDir.filePath("source.txt");
  QString dstFile = tempDir.filePath("dest.txt");
  QString moveFile = tempDir.filePath("moved.txt");

  QFile file(srcFile);
  file.open(QIODevice::WriteOnly);
  file.write("test content");
  file.close();

  EXPECT_TRUE(FileUtil::copy(srcFile, dstFile));
  EXPECT_TRUE(FileUtil::exists(dstFile));
  EXPECT_TRUE(FileUtil::exists(srcFile));

  EXPECT_TRUE(FileUtil::move(dstFile, moveFile));
  EXPECT_TRUE(FileUtil::exists(moveFile));
  EXPECT_FALSE(FileUtil::exists(dstFile));
}

TEST_F(FileUtilTest, Remove) {
  QString tempFile = tempDir.filePath("test.txt");
  QFile file(tempFile);
  file.open(QIODevice::WriteOnly);
  file.close();

  EXPECT_TRUE(FileUtil::exists(tempFile));
  EXPECT_TRUE(FileUtil::remove(tempFile));
  EXPECT_FALSE(FileUtil::exists(tempFile));
}

TEST_F(FileUtilTest, FileNameAndExtension) {
  QString path = "/home/user/document.txt";
  EXPECT_EQ(FileUtil::fileName(path), "document.txt");
  EXPECT_EQ(FileUtil::fileExtension(path), "txt");
}

TEST_F(FileUtilTest, ComputeMD5) {
  QString tempFile = tempDir.filePath("test.txt");
  QFile file(tempFile);
  file.open(QIODevice::WriteOnly);
  file.write("Hello World");
  file.close();

  QString md5 = FileUtil::computeMD5(tempFile);
  EXPECT_EQ(md5.size(), 32);
}

TEST_F(FileUtilTest, ComputeSHA256) {
  QString tempFile = tempDir.filePath("test.txt");
  QFile file(tempFile);
  file.open(QIODevice::WriteOnly);
  file.write("Hello World");
  file.close();

  QString sha256 = FileUtil::computeSHA256(tempFile);
  EXPECT_EQ(sha256.size(), 64);
}

TEST_F(FileUtilTest, ExceptionFileNotFound) {
  EXPECT_THROW(FileUtil::readTextFile("/nonexistent/file.txt"),
               FileException);
}

TEST_F(FileUtilTest, ExceptionOnInvalidPath) {
  EXPECT_THROW(FileUtil::fileSize("/invalid/path/file.txt"),
               FileException);
}

class TimeUtilTest : public ::testing::Test {};

TEST_F(TimeUtilTest, CurrentTimestamp) {
  qint64 ts = TimeUtil::currentTimestamp();
  EXPECT_GT(ts, 0);
  EXPECT_GE(ts, 1700000000);
}

TEST_F(TimeUtilTest, CurrentTimestampMillis) {
  qint64 ts = TimeUtil::currentTimestampMillis();
  EXPECT_GT(ts, 0);
}

TEST_F(TimeUtilTest, TimestampToString) {
  qint64 ts = 1704067200;
  QString str = TimeUtil::timestampToString(ts, "yyyy-MM-dd");
  EXPECT_TRUE(str.contains("2024"));
}

TEST_F(TimeUtilTest, TimestampToISO8601) {
  qint64 ts = 1704067200;
  QString iso = TimeUtil::timestampToISO8601(ts);
  EXPECT_TRUE(iso.contains("2024-01-01"));
}

TEST_F(TimeUtilTest, StringToTimestamp) {
  QString str = "2024-01-01 00:00:00";
  qint64 ts = TimeUtil::stringToTimestamp(str, "yyyy-MM-dd hh:mm:ss");
  EXPECT_GT(ts, 0);
}

TEST_F(TimeUtilTest, ISO8601ToTimestamp) {
  QString iso = "2024-01-01T00:00:00";
  qint64 ts = TimeUtil::ISO8601ToTimestamp(iso);
  EXPECT_GT(ts, 0);
}

TEST_F(TimeUtilTest, FormatNow) {
  QString now = TimeUtil::formatNow("yyyy-MM-dd");
  EXPECT_TRUE(now.contains("2024") || now.contains("2025") || now.contains("2026"));
}

TEST_F(TimeUtilTest, FormatNowISO8601) {
  QString now = TimeUtil::formatNowISO8601();
  EXPECT_TRUE(now.contains("T"));
}

TEST_F(TimeUtilTest, Elapsed) {
  qint64 start = 100;
  qint64 end = 200;
  EXPECT_EQ(TimeUtil::elapsed(start, end), 100);
}

TEST_F(TimeUtilTest, FormatDuration) {
  EXPECT_EQ(TimeUtil::formatDuration(0), "0s");
  EXPECT_EQ(TimeUtil::formatDuration(30), "30s");
  EXPECT_TRUE(TimeUtil::formatDuration(60).contains("1m"));
  EXPECT_TRUE(TimeUtil::formatDuration(90).contains("1m"));
  EXPECT_TRUE(TimeUtil::formatDuration(3661).contains("1h"));
  EXPECT_TRUE(TimeUtil::formatDuration(86400).contains("1d"));
}

TEST_F(TimeUtilTest, ExceptionOnInvalidFormat) {
  EXPECT_THROW(TimeUtil::stringToTimestamp("invalid-date", "yyyy-MM-dd"),
               TimeException);
}

TEST_F(TimeUtilTest, ExceptionOnInvalidISO8601) {
  EXPECT_THROW(TimeUtil::ISO8601ToTimestamp("not-iso-8601"),
               TimeException);
}

TEST_F(TimeUtilTest, DefaultFormat) {
  EXPECT_EQ(TimeUtil::defaultFormat(), "yyyy-MM-dd hh:mm:ss");
}

TEST_F(TimeUtilTest, ScopedTimer) {
  {
    TimeUtil::ScopedTimer timer("test_task");
    QThread::sleep(1);
  }
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}