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
