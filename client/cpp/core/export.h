#pragma once
// 动态库导出宏
// NOVA_CLIENT_EXPORTS 在编译 nova_client 时定义
// NOVA_CLIENT_SHARED 在链接 nova_client 时定义

#ifdef _WIN32
    #ifdef NOVA_CLIENT_EXPORTS
        #define NOVA_CLIENT_API __declspec(dllexport)
    #elif defined(NOVA_CLIENT_SHARED)
        #define NOVA_CLIENT_API __declspec(dllimport)
    #else
        #define NOVA_CLIENT_API
    #endif
#else
    #define NOVA_CLIENT_API __attribute__((visibility("default")))
#endif
