# run_app.cmake - 运行脚本生成宏
# 封装 configure_file + chmod，统一生成 Windows(.bat)/Linux(.sh) 运行脚本

# configure_run_script(<output_basename>)
#   output_basename: 输出文件名（不含扩展名），如 "run_app" / "run_${TARGET_NAME}"
#   依赖调用方作用域中的 TARGET_NAME 变量（供模板 @TARGET_NAME@ 替换）
#   依赖全局 QT_LIB_DIR / QT_PLUGINS_DIR / CMAKE_RUNTIME_OUTPUT_DIRECTORY
macro(configure_run_script output_basename)
    if(WIN32)
        set(_template "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in")
        set(_output "${CMAKE_BINARY_DIR}/bin/${output_basename}.bat")
    elseif(UNIX)
        set(_template "${CMAKE_SOURCE_DIR}/cmake/run_app.sh.in")
        set(_output "${CMAKE_BINARY_DIR}/bin/${output_basename}.sh")
    endif()

    if(_template)
        configure_file("${_template}" "${_output}" @ONLY)
        # Linux 下 .sh 需执行权限
        if(UNIX)
            file(CHMOD "${_output}"
                PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_EXECUTE
                            WORLD_READ WORLD_EXECUTE)
        endif()
    endif()

    unset(_template)
    unset(_output)
endmacro()
