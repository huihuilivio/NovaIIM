#pragma once
// 动态库导出宏
// NOVA_SDK_EXPORTS 在编译 nova_sdk 时定义
// NOVA_SDK_SHARED 在链接 nova_sdk 时定义

#ifdef _WIN32
    #ifdef NOVA_SDK_EXPORTS
        #define NOVA_SDK_API __declspec(dllexport)
    #elif defined(NOVA_SDK_SHARED)
        #define NOVA_SDK_API __declspec(dllimport)
    #else
        #define NOVA_SDK_API
    #endif
#else
    #define NOVA_SDK_API __attribute__((visibility("default")))
#endif
