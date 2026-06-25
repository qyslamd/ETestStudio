set(ICD_TEST_DATA_DIR "${CMAKE_CURRENT_LIST_DIR}/data")

# ====== 需要内部头文件访问的测试 ======

add_etest(
    NAME test_builder
    SOURCES test_builder.cpp
    LIBS icd_utility
    LABELS ICD
)

add_etest(
    NAME test_xml_parser
    SOURCES test_xml_parser.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_xml_parser PRIVATE
    ICD_TEST_XML_DIR="${ICD_TEST_DATA_DIR}/xml"
    ICD_TEST_JSON_DIR="${ICD_TEST_DATA_DIR}/json"
)

add_etest(
    NAME test_json_parser
    SOURCES test_json_parser.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_json_parser PRIVATE
    ICD_TEST_JSON_DIR="${ICD_TEST_DATA_DIR}/json"
    ICD_TEST_XML_DIR="${ICD_TEST_DATA_DIR}/xml"
)

add_etest(
    NAME test_loader
    SOURCES test_loader.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_loader PRIVATE
    ICD_TEST_XML_DIR="${ICD_TEST_DATA_DIR}/xml"
    ICD_TEST_JSON_DIR="${ICD_TEST_DATA_DIR}/json"
)

add_etest(
    NAME test_compat_snapshot
    SOURCES test_compat_snapshot.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_compat_snapshot PRIVATE
    ICD_TEST_XML_COMPAT_DIR="${ICD_TEST_DATA_DIR}/xml/compat"
)

# ====== 集成测试：XML → .eproto 导入管线 ======
# 对应 ProtocolManagerWidget::onImportXml() 使用的转换管线

add_etest(
    NAME test_xml_import
    SOURCES test_xml_import.cpp
    LIBS icd_utility nlohmann
    LABELS ICD
)
target_compile_definitions(test_xml_import PRIVATE
    ICD_TEST_XML_DIR="${ICD_TEST_DATA_DIR}/xml"
)

# ====== 批量导入测试：对真实 Schema XML 进行转换验证 ======
# 需要 --schema_dir=path 参数或 SCHEMA_DIR 环境变量
# 不带参数时自动跳过

add_etest(
    NAME test_schema_import
    SOURCES test_schema_import.cpp
    LIBS icd_utility nlohmann
    LABELS ICD SCHEMA
)
# 批量导入测试需要处理数百个 XML 文件，超时放宽到 120 秒
set_tests_properties(test_schema_import PROPERTIES TIMEOUT 120)

add_etest(
    NAME test_eprotox
    SOURCES test_eprotox.cpp
    LIBS icd_utility
    LABELS ICD
)

add_etest(
    NAME test_config_writeback
    SOURCES test_config_writeback.cpp
    LIBS icd_utility
    LABELS ICD
)

# ====== 仅需 public API 的测试 ======

add_etest(
    NAME test_repository
    SOURCES test_repository.cpp
    LIBS icd_utility
    LABELS ICD
)

add_etest(
    NAME test_node_value
    SOURCES test_node_value.cpp
    LIBS icd_utility
    LABELS ICD
)

add_etest(
    NAME test_export_macros
    SOURCES test_export_macros.cpp
    LIBS icd_utility
    LABELS ICD
)
