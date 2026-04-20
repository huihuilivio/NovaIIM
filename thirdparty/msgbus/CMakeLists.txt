# MsgBus — High-Performance Lock-Free Message Bus (header-only)
# https://github.com/huihuilivio/MsgBus
# License: MIT

cmake_minimum_required(VERSION 3.14)

if(NOT TARGET msgbus)
    add_library(msgbus INTERFACE)
    target_include_directories(msgbus SYSTEM INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()
