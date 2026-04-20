#pragma once
// UIDispatcher — UI 线程回调投递接口
//
// 各平台实现:
//   Qt:      QMetaObject::invokeMethod
//   iOS:     dispatch_async(dispatch_get_main_queue(), ...)
//   Android: Handler(Looper.getMainLooper()).post(...)
//
// 用法:
//   UIDispatcher::Set([](auto fn) { QMetaObject::invokeMethod(qApp, fn); });
//   UIDispatcher::Post([&] { emit loginSuccess(); });

#include <export.h>

#include <functional>
#include <mutex>

namespace nova::client {

class NOVA_SDK_API UIDispatcher {
public:
    using DispatchFunc = std::function<void(std::function<void()>)>;

    /// 设置 UI 线程投递函数（初始化时调用一次）
    static void Set(DispatchFunc dispatcher);

    /// 投递回调到 UI 线程
    static void Post(std::function<void()> callback);

    /// 是否已设置
    static bool IsSet();

private:
    static DispatchFunc dispatcher_;
    static std::mutex mutex_;
};

}  // namespace nova::client
