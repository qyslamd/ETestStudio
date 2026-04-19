#include <gtest/gtest.h>
#include <Qsci/qsciscintilla.h>
#include <Qsci/qscilexercpp.h>
#include <QApplication>

// 初始化Qt环境，避免QWidget创建报错
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// 用例1：基础实例化测试，验证QScintilla库可以正常加载
TEST(QsciHighlightTest, InstanceTest) {
    QsciScintilla editor;
    EXPECT_NE(&editor, nullptr);
}

// 用例2：Lexer加载测试，验证C++语法解析器可以正常工作
TEST(QsciHighlightTest, LexerLoadTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    EXPECT_EQ(editor.lexer(), lexer);
}

// 用例3：关键字高亮测试，验证C++关键字匹配正确样式
TEST(QsciHighlightTest, KeywordHighlightTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    editor.setText("int main() { return 0; }");
    
    // 验证关键字样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 0), 
              QsciLexerCPP::Keyword);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 11), 
              QsciLexerCPP::Keyword);
}

// 用例4：字符串高亮测试，验证双引号/单引号字符串匹配正确样式
TEST(QsciHighlightTest, StringHighlightTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    editor.setText("\"test string\" 'c'");
    // 验证双引号字符串样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 1), 
              QsciLexerCPP::DoubleQuotedString);
    // 验证单引号字符样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 14), 
              QsciLexerCPP::SingleQuotedString);
}

// 用例5：注释高亮测试，验证单行/多行注释匹配正确样式
TEST(QsciHighlightTest, CommentHighlightTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    editor.setText("// line comment\n/* block comment */");
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 3), 
              QsciLexerCPP::CommentLine);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 18), 
              QsciLexerCPP::Comment);
}

// 用例6：数字高亮测试，验证整数/浮点数匹配正确样式
TEST(QsciHighlightTest, NumberHighlightTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    editor.setText("123 0.456 0xFF 3.14e5");
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 1), 
              QsciLexerCPP::Number);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 5), 
              QsciLexerCPP::Number);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 9), 
              QsciLexerCPP::Number);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 14), 
              QsciLexerCPP::Number);
}

// 用例7：样式配置测试，验证自定义配色可以正常生效
TEST(QsciHighlightTest, ColorConfigTest) {
    QsciLexerCPP lexer;
    QColor testRed(255, 0, 0);
    QColor testGreen(0, 255, 0);
    QColor testBlue(0, 0, 255);
    
    lexer.setColor(testRed, QsciLexerCPP::Keyword);
    lexer.setColor(testGreen, QsciLexerCPP::Comment);
    lexer.setColor(testBlue, QsciLexerCPP::DoubleQuotedString);
    
    EXPECT_EQ(lexer.color(QsciLexerCPP::Keyword), testRed);
    EXPECT_EQ(lexer.color(QsciLexerCPP::Comment), testGreen);
    EXPECT_EQ(lexer.color(QsciLexerCPP::DoubleQuotedString), testBlue);
}

// 用例8：新增UTF-8中文内容高亮测试
TEST(QsciHighlightTest, ChineseContentTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    
    // 测试中文注释
    editor.setText("// 这是中文注释内容\n\"这是中文字符串\"");
    
    // 验证中文注释样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 3), 
              QsciLexerCPP::CommentLine);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 10), 
              QsciLexerCPP::CommentLine);
    
    // 验证中文双引号字符串样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 20), 
              QsciLexerCPP::DoubleQuotedString);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 28), 
              QsciLexerCPP::DoubleQuotedString);
}

// 用例9：新增特殊编码/转义字符高亮测试
TEST(QsciHighlightTest, SpecialCharTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    
    // 测试转义字符、raw string、Unicode字符
    editor.setText("R\"(raw string \\n \\t)\" \"string with \\u4e2d\\u6587\" \"\\nescape \\n \\\" \\' \\\\\"");
    
    // 验证raw string样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 3), 
              QsciLexerCPP::RawString);
    
    // 验证普通字符串中的Unicode字符样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 30), 
              QsciLexerCPP::DoubleQuotedString);
    
    // 验证转义字符样式
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 50), 
              QsciLexerCPP::EscapeSequence);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 53), 
              QsciLexerCPP::EscapeSequence);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 56), 
              QsciLexerCPP::EscapeSequence);
}

// 用例10：预处理指令高亮测试
TEST(QsciHighlightTest, PreprocessorTest) {
    QsciScintilla editor;
    QsciLexerCPP* lexer = new QsciLexerCPP(&editor);
    editor.setLexer(lexer);
    editor.setText("#include <iostream>\n#define TEST 123\n#ifdef _DEBUG\n#endif");
    
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 1), 
              QsciLexerCPP::PreProcessor);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 20), 
              QsciLexerCPP::PreProcessor);
    EXPECT_EQ(editor.SendScintilla(QsciScintilla::SCI_GETSTYLEAT, 35), 
              QsciLexerCPP::PreProcessor);
}
