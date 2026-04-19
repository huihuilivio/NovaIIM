# client/mobile — NovaIIM 移动端 Bridge
#
# iOS:     Objective-C++ 封装 → Swift/ObjC 调用
# Android: JNI 封装 → Kotlin/Java 调用
#
# 共同依赖: nova_client 共享库

cmake_minimum_required(VERSION 3.28)

# ---- iOS Framework ----
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR NOVA_BUILD_IOS)
    add_library(nova_ios SHARED
        ios/NovaClient.mm
    )

    target_link_libraries(nova_ios PRIVATE nova_client)

    set_target_properties(nova_ios PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER com.nova.client
        PUBLIC_HEADER "ios/NovaClient.h"
    )

    nova_set_output_dirs(nova_ios)
endif()

# ---- Android JNI ----
if(CMAKE_SYSTEM_NAME STREQUAL "Android" OR NOVA_BUILD_ANDROID)
    add_library(nova_jni SHARED
        android/nova_jni.cpp
    )

    target_link_libraries(nova_jni PRIVATE
        nova_client
        log   # Android log
    )

    nova_set_output_dirs(nova_jni)
endif()
