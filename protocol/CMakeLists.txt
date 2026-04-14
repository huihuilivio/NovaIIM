# protocol - 协议层（序列化）

nova_collect_sources(${CMAKE_CURRENT_SOURCE_DIR} PROTOCOL_SOURCES)

if(PROTOCOL_SOURCES)
    add_library(nova_protocol STATIC ${PROTOCOL_SOURCES})
    nova_set_compiler_options(nova_protocol)
    target_include_directories(nova_protocol PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    # 占位：源文件就绪后移除此块
    add_library(nova_protocol INTERFACE)
    target_include_directories(nova_protocol INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
