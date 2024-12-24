#include "../src/lock_region.h" // Include the LockRegion header
#include <gtest/gtest.h>

// Test LockRegion constructor
TEST(LockRegionTest, Constructor) {
    Event acq_event((static_cast<uint64_t>(Event::Acquire) << 60) |
                    (static_cast<uint64_t>(1) << 52),
                    1);
    Event rel_event((static_cast<uint64_t>(Event::Release) << 60) |
                    (static_cast<uint64_t>(1) << 52),
                    5);

    LockRegion lock_region(acq_event, rel_event);

    EXPECT_EQ(&lock_region.getAcqEvent(), &acq_event);
    EXPECT_EQ(&lock_region.getRelEvent(), &rel_event);
}

// Test getter methods
TEST(LockRegionTest, Getters) {
    Event acq_event((static_cast<uint64_t>(Event::Acquire) << 60) |
                    (static_cast<uint64_t>(2) << 52),
                    2);
    Event rel_event((static_cast<uint64_t>(Event::Release) << 60) |
                    (static_cast<uint64_t>(2) << 52),
                    10);

    LockRegion lock_region(acq_event, rel_event);

    const Event& acquired = lock_region.getAcqEvent();
    const Event& released = lock_region.getRelEvent();

    EXPECT_EQ(acquired.getEventType(), Event::Acquire);
    EXPECT_EQ(acquired.getEventId(), 2);
    EXPECT_EQ(released.getEventType(), Event::Release);
    EXPECT_EQ(released.getEventId(), 10);
}

// Test containsEvent when the event is within the region
TEST(LockRegionTest, ContainsEvent_WithinRegion) {
    Event acq_event((static_cast<uint64_t>(Event::Acquire) << 60) |
                    (static_cast<uint64_t>(1) << 52),
                    1);
    Event rel_event((static_cast<uint64_t>(Event::Release) << 60) |
                    (static_cast<uint64_t>(1) << 52),
                    10);

    LockRegion lock_region(acq_event, rel_event);

    Event test_event1((static_cast<uint64_t>(Event::Read) << 60) |
                      (static_cast<uint64_t>(1) << 52),
                      5);

    Event test_event2((static_cast<uint64_t>(Event::Write) << 60) |
                      (static_cast<uint64_t>(1) << 52),
                      1); // Same as acq_event

    Event test_event3((static_cast<uint64_t>(Event::Write) << 60) |
                      (static_cast<uint64_t>(1) << 52),
                      10); // Same as rel_event

    EXPECT_TRUE(lock_region.containsEvent(test_event1));
    EXPECT_TRUE(lock_region.containsEvent(test_event2));
    EXPECT_TRUE(lock_region.containsEvent(test_event3));
}

// Test containsEvent when the event is outside the region
TEST(LockRegionTest, ContainsEvent_OutsideRegion) {
    Event acq_event((static_cast<uint64_t>(Event::Acquire) << 60) |
                    (static_cast<uint64_t>(1) << 52),
                    1);
    Event rel_event((static_cast<uint64_t>(Event::Release) << 60) |
                    (static_cast<uint64_t>(1) << 52),
                    10);

    LockRegion lock_region(acq_event, rel_event);

    Event test_event1((static_cast<uint64_t>(Event::Read) << 60) |
                      (static_cast<uint64_t>(1) << 52),
                      0); // Event ID before acq_event

    Event test_event2((static_cast<uint64_t>(Event::Write) << 60) |
                      (static_cast<uint64_t>(1) << 52),
                      11); // Event ID after rel_event

    Event test_event3((static_cast<uint64_t>(Event::Write) << 60) |
                      (static_cast<uint64_t>(2) << 52),
                      5); // Different thread ID

    EXPECT_FALSE(lock_region.containsEvent(test_event1));
    EXPECT_FALSE(lock_region.containsEvent(test_event2));
    EXPECT_FALSE(lock_region.containsEvent(test_event3));
}