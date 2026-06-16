# 连接清理模块测试
add_etest(
    NAME test_topology_connection_cleanup
    SOURCES connection_cleanup_test.cpp
    LIBS etest_topology
    QT_MODULES Core Test
    LABELS TOPOLOGY
)
