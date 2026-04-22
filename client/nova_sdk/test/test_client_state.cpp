// test_client_state.cpp — ClientState 枚举和字符串转换测试

#include <model/client_state.h>

#include <gtest/gtest.h>
#include <cstring>

using nova::client::ClientState;
using nova::client::ClientStateStr;

TEST(ClientStateTest, DisconnectedString) {
    EXPECT_STREQ(ClientStateStr(ClientState::kDisconnected), "Disconnected");
}

TEST(ClientStateTest, ConnectingString) {
    EXPECT_STREQ(ClientStateStr(ClientState::kConnecting), "Connecting");
}

TEST(ClientStateTest, ConnectedString) {
    EXPECT_STREQ(ClientStateStr(ClientState::kConnected), "Connected");
}

TEST(ClientStateTest, AuthenticatedString) {
    EXPECT_STREQ(ClientStateStr(ClientState::kAuthenticated), "Authenticated");
}

TEST(ClientStateTest, ReconnectingString) {
    EXPECT_STREQ(ClientStateStr(ClientState::kReconnecting), "Reconnecting");
}

TEST(ClientStateTest, UnknownEnumReturnsUnknown) {
    auto bogus = static_cast<ClientState>(99);
    EXPECT_STREQ(ClientStateStr(bogus), "Unknown");
}

TEST(ClientStateTest, EnumValuesAreStable) {
    // 协议层依赖这些数值，禁止变更
    EXPECT_EQ(static_cast<uint8_t>(ClientState::kDisconnected),  0);
    EXPECT_EQ(static_cast<uint8_t>(ClientState::kConnecting),    1);
    EXPECT_EQ(static_cast<uint8_t>(ClientState::kConnected),     2);
    EXPECT_EQ(static_cast<uint8_t>(ClientState::kAuthenticated), 3);
    EXPECT_EQ(static_cast<uint8_t>(ClientState::kReconnecting),  4);
}
