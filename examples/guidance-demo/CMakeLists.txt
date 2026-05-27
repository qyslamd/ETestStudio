set(TARGET_NAME "guidance-demo")

add_executable(${TARGET_NAME} 
    utility_qt.cpp
    utility_qt.h
    window_mover.cpp
    window_mover.h
    guidance_config.cpp                                                                                                                                                                                                
    guidance_config.h                                                                                                                                                                                                  
    guidance_controller.cpp                                                                                                                                                                                            
    guidance_controller.h                                                                                                                                                                                              
    guidance_homepage.cpp                                                                                                                                                                                              
    guidance_homepage.h                                                                                                                                                                                                
    guidance_homepage.ui                                                                                                                                                                                               
    guidance_presentation.cpp                                                                                                                                                                                          
    guidance_presentation.h                                                                                                                                                                                            
    main.cpp  
)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

#=======================================
# 生成运行脚本，自动设置Qt运行时环境变量
if(WIN32)
    # QT_BIN_DIR 和 QT_PLUGINS_DIR 在顶层CMakeLists.txt中设置
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_BINARY_DIR}/bin/run_${TARGET_NAME}.bat"
        @ONLY
    )
endif()