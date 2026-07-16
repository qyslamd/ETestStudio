add_library(qcustomplot STATIC qcustomplot.cpp)
target_include_directories(qcustomplot PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(qcustomplot PUBLIC Qt5::Core Qt5::Widgets Qt5::PrintSupport)
