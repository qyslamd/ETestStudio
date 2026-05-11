add_library(pugixml_local STATIC pugixml.cpp)
target_include_directories(pugixml_local PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
