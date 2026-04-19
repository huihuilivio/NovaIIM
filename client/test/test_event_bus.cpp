#include <gtest/gtest.h>
#include <core/event_bus.h>

#include <string>
#include <atomic>

using namespace nova::client;

struct TestEvent {
    int value;
    std::string name;
};

struct OtherEvent {
    double data;
};

TEST(EventBusTest, SubscribeAndPublish) {
    EventBus::Get().Clear();

    int received = 0;
    EventBus::Get().Subscribe<TestEvent>([&](const TestEvent& e) {
        received = e.value;
    });

    EventBus::Get().Publish(TestEvent{42, "test"});
    EXPECT_EQ(received, 42);
}

TEST(EventBusTest, MultipleSubscribers) {
    EventBus::Get().Clear();

    int count = 0;
    EventBus::Get().Subscribe<TestEvent>([&](const TestEvent&) { ++count; });
    EventBus::Get().Subscribe<TestEvent>([&](const TestEvent&) { ++count; });

    EventBus::Get().Publish(TestEvent{1, ""});
    EXPECT_EQ(count, 2);
}

TEST(EventBusTest, DifferentEventTypes) {
    EventBus::Get().Clear();

    int test_count = 0;
    int other_count = 0;

    EventBus::Get().Subscribe<TestEvent>([&](const TestEvent&) { ++test_count; });
    EventBus::Get().Subscribe<OtherEvent>([&](const OtherEvent&) { ++other_count; });

    EventBus::Get().Publish(TestEvent{1, ""});
    EXPECT_EQ(test_count, 1);
    EXPECT_EQ(other_count, 0);

    EventBus::Get().Publish(OtherEvent{3.14});
    EXPECT_EQ(test_count, 1);
    EXPECT_EQ(other_count, 1);
}

TEST(EventBusTest, Unsubscribe) {
    EventBus::Get().Clear();

    int count = 0;
    EventBus::Get().Subscribe<TestEvent>([&](const TestEvent&) { ++count; });
    EventBus::Get().Publish(TestEvent{1, ""});
    EXPECT_EQ(count, 1);

    EventBus::Get().Unsubscribe<TestEvent>();
    EventBus::Get().Publish(TestEvent{2, ""});
    EXPECT_EQ(count, 1);  // 不再收到
}

TEST(EventBusTest, ClearAll) {
    EventBus::Get().Clear();

    int a = 0, b = 0;
    EventBus::Get().Subscribe<TestEvent>([&](const TestEvent&) { ++a; });
    EventBus::Get().Subscribe<OtherEvent>([&](const OtherEvent&) { ++b; });

    EventBus::Get().Clear();

    EventBus::Get().Publish(TestEvent{1, ""});
    EventBus::Get().Publish(OtherEvent{1.0});
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 0);
}

TEST(EventBusTest, NoSubscribersNoCrash) {
    EventBus::Get().Clear();
    // 无订阅者时发布不应崩溃
    EXPECT_NO_THROW(EventBus::Get().Publish(TestEvent{1, ""}));
}
