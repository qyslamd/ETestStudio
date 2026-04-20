#include <gtest/gtest.h>
#include <hpdf.h>
#include <fstream>
#include <string>

// 测试PDF创建与保存基础功能
TEST(LibHaruTest, CreateSaveTest) {
    const char* test_file = "libharu_test_output.pdf";
    
    HPDF_Doc pdf = HPDF_New(NULL, NULL);
    ASSERT_NE(pdf, nullptr);
    
    // 添加页面
    HPDF_Page page = HPDF_AddPage(pdf);
    ASSERT_NE(page, nullptr);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    
    // 设置元信息
    HPDF_SetInfoAttr(pdf, HPDF_INFO_TITLE, "Test PDF");
    HPDF_SetInfoAttr(pdf, HPDF_INFO_AUTHOR, "ETest Dev");
    HPDF_SetInfoAttr(pdf, HPDF_INFO_SUBJECT, "LibHaru Test");
    
    // 保存PDF
    HPDF_SaveToFile(pdf, test_file);
    HPDF_Free(pdf);
    
    // 验证文件存在
    std::ifstream file(test_file, std::ios::binary);
    EXPECT_TRUE(file.good());
    file.close();
    
    std::remove(test_file);
}

// 测试文本绘制功能
TEST(LibHaruTest, TextDrawTest) {
    const char* test_file = "libharu_text_test.pdf";
    
    HPDF_Doc pdf = HPDF_New(NULL, NULL);
    ASSERT_NE(pdf, nullptr);
    
    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    
    // 设置字体
    HPDF_Font font = HPDF_GetFont(pdf, "Helvetica", NULL);
    ASSERT_NE(font, nullptr);
    
    HPDF_Page_SetFontAndSize(page, font, 24);
    HPDF_Page_BeginText(page);
    HPDF_Page_TextOut(page, 100, 700, "Hello LibHaru!");
    HPDF_Page_EndText(page);
    
    // 设置颜色
    HPDF_Page_SetRGBFill(page, 1.0f, 0.0f, 0.0f);
    HPDF_Page_BeginText(page);
    HPDF_Page_TextOut(page, 100, 650, "Red Text");
    HPDF_Page_EndText(page);
    
    HPDF_SaveToFile(pdf, test_file);
    HPDF_Free(pdf);
    
    std::ifstream file(test_file, std::ios::binary);
    EXPECT_TRUE(file.good());
    file.close();
    
    std::remove(test_file);
}

// 测试图形绘制功能
TEST(LibHaruTest, ShapeDrawTest) {
    const char* test_file = "libharu_shape_test.pdf";
    
    HPDF_Doc pdf = HPDF_New(NULL, NULL);
    ASSERT_NE(pdf, nullptr);
    
    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    
    // 绘制矩形
    HPDF_Page_SetRGBFill(page, 0.0f, 0.5f, 0.0f);
    HPDF_Page_Rectangle(page, 100, 500, 200, 100);
    HPDF_Page_Fill(page);
    
    // 绘制直线
    HPDF_Page_SetRGBStroke(page, 0.0f, 0.0f, 1.0f);
    HPDF_Page_SetLineWidth(page, 2.0f);
    HPDF_Page_MoveTo(page, 100, 450);
    HPDF_Page_LineTo(page, 300, 450);
    HPDF_Page_Stroke(page);
    
    HPDF_SaveToFile(pdf, test_file);
    HPDF_Free(pdf);
    
    std::ifstream file(test_file, std::ios::binary);
    EXPECT_TRUE(file.good());
    file.close();
    
    std::remove(test_file);
}

// 测试多页面功能
TEST(LibHaruTest, MultiPageTest) {
    const char* test_file = "libharu_multipage_test.pdf";
    
    HPDF_Doc pdf = HPDF_New(NULL, NULL);
    ASSERT_NE(pdf, nullptr);
    
    // 创建3个页面
    for (int i = 0; i < 3; ++i) {
        HPDF_Page page = HPDF_AddPage(pdf);
        HPDF_Font font = HPDF_GetFont(pdf, "Helvetica", NULL);
        HPDF_Page_SetFontAndSize(page, font, 20);
        HPDF_Page_BeginText(page);
        char text[32];
        sprintf(text, "Page %d", i + 1);
        HPDF_Page_TextOut(page, 100, 700, text);
        HPDF_Page_EndText(page);
    }
    
    HPDF_SaveToFile(pdf, test_file);
    HPDF_Free(pdf);
    
    std::ifstream file(test_file, std::ios::binary);
    EXPECT_TRUE(file.good());
    file.close();
    
    std::remove(test_file);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
