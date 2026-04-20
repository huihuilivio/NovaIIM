# client/desktop — NovaIIM 桌面客户端
#
# 按平台分发到对应子目录

cmake_minimum_required(VERSION 3.28)

if(WIN32)
    add_subdirectory(win)
endif()
# TODO: macOS / Linux 平台子目录
