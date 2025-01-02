#include "../src/event.hpp" // Include the Event header
#include <gtest/gtest.h>

// Test default constructor
TEST(EventTest, DefaultConstructor) {
    Event e;

    EXPECT_EQ(e.getEventId(), 0);
    EXPECT_EQ(e.getThreadId(), 0);
    EXPECT_EQ(e.getTargetId(), 0);
    EXPECT_EQ(e.getTargetValue(), 0);
    EXPECT_EQ(e.getEventType(), Event::Read); // Default raw_event_ is 0, so EventType is Read
    EXPECT_TRUE(Event::isNullEvent(e));
}

// Test parameterized constructor
TEST(EventTest, ParameterizedConstructor) {
    uint64_t rawEvent = (static_cast<uint64_t>(Event::Write) << 60) |
                        (static_cast<uint64_t>(42) << 52) |
                        (static_cast<uint64_t>(1234) << 32) |
                        5678; // Event with Write type, ThreadId 42, TargetId 1234, TargetValue 5678
    uint32_t eventId = 99;

    Event e(rawEvent, eventId);

    EXPECT_EQ(e.getEventType(), Event::Write);
    EXPECT_EQ(e.getThreadId(), 42);
    EXPECT_EQ(e.getTargetId(), 1234);
    EXPECT_EQ(e.getTargetValue(), 5678);
    EXPECT_EQ(e.getEventId(), 99);
    EXPECT_FALSE(Event::isNullEvent(e));
}

// Test operator==
TEST(EventTest, EqualityOperator) {
    uint64_t rawEvent1 = (static_cast<uint64_t>(Event::Acquire) << 60) |
                         (static_cast<uint64_t>(10) << 52) |
                         (static_cast<uint64_t>(1000) << 32) |
                         5000;
    uint32_t eventId1 = 1;

    uint64_t rawEvent2 = (static_cast<uint64_t>(Event::Acquire) << 60) |
                         (static_cast<uint64_t>(10) << 52) |
                         (static_cast<uint64_t>(1000) << 32) |
                         5000;
    uint32_t eventId2 = 1;

    Event e1(rawEvent1, eventId1);
    Event e2(rawEvent2, eventId2);

    EXPECT_TRUE(e1 == e2);

    Event e3(rawEvent1, 2); // Different eventId
    EXPECT_FALSE(e1 == e3);
}

// Test hash function
TEST(EventTest, HashFunction) {
    uint64_t rawEvent = (static_cast<uint64_t>(Event::Release) << 60) |
                        (static_cast<uint64_t>(20) << 52);
    uint32_t eventId = 10;

    Event e(rawEvent, eventId);
    EventHash hash;

    EXPECT_EQ(hash(e), std::hash<uint64_t>{}(e.getEventId()));

    Event e2(rawEvent, 11); // Different eventId
    EXPECT_NE(hash(e), hash(e2));
}

// Test static isNullEvent
TEST(EventTest, IsNullEvent) {
    Event nullEvent; // Default constructor
    EXPECT_TRUE(Event::isNullEvent(nullEvent));

    uint64_t rawEvent = (static_cast<uint64_t>(Event::Join) << 60) |
                        (static_cast<uint64_t>(5) << 52);
    Event nonNullEvent(rawEvent, 1);
    EXPECT_FALSE(Event::isNullEvent(nonNullEvent));
}
