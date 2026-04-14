# compiler.cmake - 编译器选项配置

function(nova_set_compiler_options target)
    target_compile_features(${target} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /utf-8
            /permissive-
            /external:W0      # 第三方 SYSTEM 头文件不产生警告
        )
        # 防止 <windows.h> 定义 min/max 宏，与 std::min/std::max 冲突
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    endif()
endfunction()
