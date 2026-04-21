#===============================
# 打印包的变量信息，正则表达式匹配
# 比如你想看和 Qt5开头的变量
# 就可以使用 print_package_variables(Qt5)
function(print_package_variables PREFIX)
    message(STATUS "=== Variables for ${PREFIX} ===")
    get_cmake_property(variable_names VARIABLES)
    foreach(var ${variable_names})
        if(var MATCHES "^${PREFIX}")
            message(STATUS "${var}: ${${var}}")
        endif()
    endforeach()
    message(STATUS "=== End of variables for ${PREFIX} ===")
endfunction()


#===============================
# 打印 目标的属性
# 比如你想看 Qt5::Core 的 LINK_LIBRARIES 属性，
# 你就可以直接 print_target_properry(Qt5::Core LINK_LIBRARIES)
function(print_target_properry TARGET PROPERTY)
    set(_property_var "")
    get_target_property(_property_var ${TARGET} ${PROPERTY})
    message(STATUS "=== Property '${PROPERTY}' for target ${TARGET} ===")
    message(STATUS "\t" ${_property_var})
endfunction()
