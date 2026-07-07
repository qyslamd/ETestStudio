# 测试程序数据序列化测试
add_etest(
    NAME test_program_data
    SOURCES test_program_data.cpp
    LIBS etest_program
    QT_MODULES Core Test Widgets
    LABELS TEST_PROGRAM
)
