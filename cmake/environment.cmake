# environment.cmake - 外部工具环境检查
#
# 检测构建所需的外部工具：Python / Node.js / npm
# 将结果缓存为 NOVA_PYTHON / NOVA_NODE / NOVA_NPM 变量

# ---- Python ----
find_program(NOVA_PYTHON NAMES python3 python)
if(NOVA_PYTHON)
    execute_process(
        COMMAND ${NOVA_PYTHON} --version
        OUTPUT_VARIABLE _py_version OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    message(STATUS "Found Python: ${NOVA_PYTHON} (${_py_version})")
else()
    message(WARNING "Python not found — MySQL client download script will be unavailable")
endif()

# ---- Node.js ----
find_program(NOVA_NODE NAMES node)
if(NOVA_NODE)
    execute_process(
        COMMAND ${NOVA_NODE} --version
        OUTPUT_VARIABLE _node_version OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    message(STATUS "Found Node.js: ${NOVA_NODE} (${_node_version})")
else()
    if(NOVA_BUILD_CLIENT)
        message(FATAL_ERROR "Node.js not found — required to build desktop web frontend")
    else()
        message(WARNING "Node.js not found — desktop web frontend cannot be built")
    endif()
endif()

# ---- npm ----
find_program(NOVA_NPM NAMES npm)
if(NOVA_NPM)
    execute_process(
        COMMAND ${NOVA_NPM} --version
        OUTPUT_VARIABLE _npm_version OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    message(STATUS "Found npm: ${NOVA_NPM} (${_npm_version})")
else()
    if(NOVA_BUILD_CLIENT)
        message(FATAL_ERROR "npm not found — required to build desktop web frontend")
    else()
        message(WARNING "npm not found — desktop web frontend cannot be built")
    endif()
endif()
