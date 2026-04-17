# protocol - 协议层（header-only，客户端 / 服务端共用）
#
# 使用方式: target_link_libraries(xxx PUBLIC/PRIVATE nova_protocol)
# 头文件:   #include <nova/packet.h>      // 帧编解码 + Cmd 枚举
#           #include <nova/protocol.h>    // 消息体结构 + 序列化

add_library(nova_protocol INTERFACE)
target_include_directories(nova_protocol INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}    # protocol/ → 可 #include <nova/packet.h>
)
target_link_libraries(nova_protocol INTERFACE ylt)
