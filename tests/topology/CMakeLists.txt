# 连接清理模块测试
add_etest(
    NAME test_topology_connection_cleanup
    SOURCES connection_cleanup_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)

# 场景渲染导出测试
add_etest(
    NAME test_topology_scene_renderer
    SOURCES scene_renderer_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test Widgets
    LABELS TOPOLOGY
)

# 撤销命令回归测试
add_etest(
    NAME test_topology_undo_commands
    SOURCES undo_commands_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)

# 文档重命名引用同步测试
add_etest(
    NAME test_topology_document_rename
    SOURCES document_rename_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)
