add_library(icd_utility
    src/core/node.cpp
    src/core/frame.cpp
    src/core/repository.cpp
    src/schema/schema.cpp
    src/schema/builder.cpp
    src/format/xml_parser.cpp
    src/format/json_parser.cpp
    src/format/json_serializer.cpp
    src/format/type_mapping.cpp
    src/format/xml_util.cpp
    src/format/xml_serializer.cpp
    src/loader/loader.cpp
)

target_compile_features(icd_utility PUBLIC cxx_std_17)

target_include_directories(icd_utility PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/src"
)

target_link_libraries(icd_utility PUBLIC
    pugixml_local
    nlohmann
    tl
)
