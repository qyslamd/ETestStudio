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

# 属性面板端口应用测试
add_etest(
    NAME test_topology_property_panel_apply
    SOURCES property_panel_apply_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test Widgets
    LABELS TOPOLOGY
)

# 设备模板管理测试
add_etest(
    NAME test_topology_device_template_manager
    SOURCES device_template_manager_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)

# 拓扑 JSON 反序列化校验测试
add_etest(
    NAME test_topology_json_serializer_validation
    SOURCES json_serializer_validation_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)

# 拓扑导出控制器测试
add_etest(
    NAME test_topology_export_controller
    SOURCES export_controller_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test Widgets
    LABELS TOPOLOGY
)

# 拓扑清理控制器测试
add_etest(
    NAME test_topology_cleanup_controller
    SOURCES cleanup_controller_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)

# 拓扑编辑器移动撤销重做测试
add_etest(
    NAME test_topology_editor_move_undo
    SOURCES editor_move_undo_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test Widgets
    LABELS TOPOLOGY
)

# M2: 设备 id + 端口绑定 + 信号测试
add_etest(
    NAME test_topology_m2
    SOURCES topology_m2_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)

# M3: 端口绑帧访问器 + 撤销重做测试
add_etest(
    NAME test_topology_m3
    SOURCES topology_m3_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)
