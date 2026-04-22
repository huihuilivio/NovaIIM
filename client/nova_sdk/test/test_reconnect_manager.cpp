// test_reconnect_manager.cpp — ReconnectManager 单元测试
//
// 覆盖: 启停状态、Reset、OnStateChanged 触发的重连调度

#include <core/client_config.h>
#include <core/reconnect_manager.h>
#include <model/client_state.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using nova::client::ClientConfig;
using nova::client::ClientState;
using nova::client::ReconnectManager;

namespace {

ClientConfig MakeFastConfig(uint32_t initial = 10, uint32_t max_ms = 100, double mult = 2.0) {
    ClientConfig cfg;
    cfg.reconnect_initial_ms = initial;
    cfg.reconnect_max_ms     = max_ms;
    cfg.reconnect_multiplier = mult;
    return cfg;
}

}  // namespace

TEST(ReconnectManagerTest, DefaultEnabledNotStopped) {
    auto cfg = MakeFastConfig();
    ReconnectManager mgr(cfg);
    EXPECT_TRUE(mgr.IsEnabled());
    EXPECT_FALSE(mgr.IsStopped());
}

TEST(ReconnectManagerTest, SetEnabledTogglesFlag) {
    auto cfg = MakeFastConfig();
    ReconnectManager mgr(cfg);
    mgr.SetEnabled(false);
    EXPECT_FALSE(mgr.IsEnabled());
    mgr.SetEnabled(true);
    EXPECT_TRUE(mgr.IsEnabled());
}

TEST(ReconnectManagerTest, StopMarksStoppedAndDisabled) {
    auto cfg = MakeFastConfig();
    ReconnectManager mgr(cfg);
    mgr.Stop();
    EXPECT_TRUE(mgr.IsStopped());
    EXPECT_FALSE(mgr.IsEnabled());
}

TEST(ReconnectManagerTest, DisconnectTriggersReconnectCallback) {
    auto cfg = MakeFastConfig(/*initial=*/10);
    ReconnectManager mgr(cfg);

    std::atomic<int> hits{0};
    mgr.OnReconnect([&hits]() { hits.fetch_add(1); });

    mgr.OnStateChanged(ClientState::kDisconnected);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_GE(hits.load(), 1);
}

TEST(ReconnectManagerTest, StoppedSuppressesScheduling) {
    auto cfg = MakeFastConfig(/*initial=*/10);
    ReconnectManager mgr(cfg);

    std::atomic<int> hits{0};
    mgr.OnReconnect([&hits]() { hits.fetch_add(1); });

    mgr.Stop();
    mgr.OnStateChanged(ClientState::kDisconnected);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(hits.load(), 0);
}

TEST(ReconnectManagerTest, AuthenticatedStateDoesNotTriggerReconnect) {
    auto cfg = MakeFastConfig(/*initial=*/10);
    ReconnectManager mgr(cfg);

    std::atomic<int> hits{0};
    mgr.OnReconnect([&hits]() { hits.fetch_add(1); });

    mgr.OnStateChanged(ClientState::kAuthenticated);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(hits.load(), 0);
}

TEST(ReconnectManagerTest, ResetIsSafe) {
    auto cfg = MakeFastConfig();
    ReconnectManager mgr(cfg);
    EXPECT_NO_THROW(mgr.Reset());
}

TEST(ReconnectManagerTest, DestructorCancelsPendingTimer) {
    // 验证析构时不会触发已调度的重连回调
    std::atomic<int> hits{0};
    {
        auto cfg = MakeFastConfig(/*initial=*/100);  // 调度但不要触发
        ReconnectManager mgr(cfg);
        mgr.OnReconnect([&hits]() { hits.fetch_add(1); });
        mgr.OnStateChanged(ClientState::kDisconnected);
        // mgr 析构（隐式 Stop）
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(hits.load(), 0);
}
