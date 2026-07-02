# 协议位布局视图行为测试
add_etest(
    NAME test_icd_bit_layout_view
    SOURCES test_icd_bit_layout_view.cpp
    LIBS etest_protocol
    QT_MODULES Core Test Widgets
    LABELS PROTOCOL
)

# 帧报文预览解析测试
add_etest(
    NAME test_frame_preview
    SOURCES test_frame_preview.cpp
    LIBS etest_protocol
    QT_MODULES Core Test
    LABELS PROTOCOL
)
