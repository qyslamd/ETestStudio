#include <gtest/gtest.h>
#include <xlsxdocument.h>
#include <xlsxformat.h>
#include <xlsxworksheet.h>
#include <QCoreApplication>
#include <QDate>
#include <QFile>

using namespace QXlsx;

// 测试Excel基础创建与保存
TEST(QXlsxTest, CreateSaveTest) {
  const QString test_file = "qxlsx_test_output.xlsx";

  Document xlsx;
  xlsx.write("A1", "test string");
  xlsx.write("A2", 12345);
  xlsx.write("A3", 3.1415926);
  xlsx.write("A4", QDate(2026, 4, 20));

  EXPECT_TRUE(xlsx.saveAs(test_file));
  EXPECT_TRUE(QFile::exists(test_file));

  QFile::remove(test_file);
}

// 测试Excel读取功能
TEST(QXlsxTest, ReadTest) {
  const QString test_file = "qxlsx_read_test.xlsx";

  // 先写入测试数据
  Document write_xlsx;
  write_xlsx.write("B2", "read test string");
  write_xlsx.write("B3", 67890);
  write_xlsx.write("B4", 9.87654);
  ASSERT_TRUE(write_xlsx.saveAs(test_file));

  // 读取验证
  Document read_xlsx(test_file);
  EXPECT_TRUE(read_xlsx.load());

  EXPECT_EQ(read_xlsx.read("B2").toString(), "read test string");
  EXPECT_EQ(read_xlsx.read("B3").toInt(), 67890);
  EXPECT_NEAR(read_xlsx.read("B4").toDouble(), 9.87654, 0.0001);

  QFile::remove(test_file);
}

// 测试多工作表功能
TEST(QXlsxTest, MultiSheetTest) {
  const QString test_file = "qxlsx_sheet_test.xlsx";

  Document xlsx;
  xlsx.addSheet("Sheet1");
  xlsx.addSheet("Sheet2");
  xlsx.addSheet("Sheet3");

  xlsx.selectSheet("Sheet1");
  xlsx.write("A1", "sheet1 content");

  xlsx.selectSheet("Sheet2");
  xlsx.write("A1", "sheet2 content");

  xlsx.selectSheet("Sheet3");
  xlsx.write("A1", "sheet3 content");

  ASSERT_TRUE(xlsx.saveAs(test_file));

  // 读取验证
  Document read_xlsx(test_file);
  EXPECT_TRUE(read_xlsx.load());

  read_xlsx.selectSheet("Sheet1");
  EXPECT_EQ(read_xlsx.read("A1").toString(), "sheet1 content");

  read_xlsx.selectSheet("Sheet2");
  EXPECT_EQ(read_xlsx.read("A1").toString(), "sheet2 content");

  read_xlsx.selectSheet("Sheet3");
  EXPECT_EQ(read_xlsx.read("A1").toString(), "sheet3 content");

  QFile::remove(test_file);
}

// 测试单元格样式设置
TEST(QXlsxTest, CellFormatTest) {
  const QString test_file = "qxlsx_format_test.xlsx";

  Document xlsx;
  Format format;
  format.setFontBold(true);
  format.setFontColor(QColor(Qt::red));
  format.setHorizontalAlignment(Format::AlignHCenter);
  format.setVerticalAlignment(Format::AlignVCenter);

  xlsx.write("C3", "formatted text", format);
  ASSERT_TRUE(xlsx.saveAs(test_file));

  EXPECT_TRUE(QFile::exists(test_file));
  QFile::remove(test_file);
}

int main(int argc, char** argv) {
  QCoreApplication a(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
