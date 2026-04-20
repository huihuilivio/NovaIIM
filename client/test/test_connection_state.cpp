#include <gtest/gtest.h>
#include <infra/connection_state.h>

using namespace nova::client;

TEST(ConnectionStateTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(ConnectionState::kDisconnected), 0);
    EXPECT_EQ(static_cast<uint8_t>(ConnectionState::kConnecting), 1);
    EXPECT_EQ(static_cast<uint8_t>(ConnectionState::kConnected), 2);
}

TEST(ConnectionStateTest, StateToString) {
    EXPECT_STREQ(ConnectionStateStr(ConnectionState::kDisconnected), "Disconnected");
    EXPECT_STREQ(ConnectionStateStr(ConnectionState::kConnecting), "Connecting");
    EXPECT_STREQ(ConnectionStateStr(ConnectionState::kConnected), "Connected");
}
