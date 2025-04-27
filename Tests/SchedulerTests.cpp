//
// Created by Iskandar Safarov on 9/3/2025.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <scheduler/scheduler.hpp>

using namespace testing;

struct MockCPU {
    MockCPU() { MockCPU::impl = this; }
    MOCK_METHOD(void, enterCriticalSection, ());
    MOCK_METHOD(void, leaveCriticalSection, ());
    MOCK_METHOD(void, sleep, ());
    MOCK_METHOD(uint32_t, getSystemTick, ());
    static inline MockCPU * impl;
};

struct MockCompletion {
    MOCK_METHOD(void, onComplete, (), (const));

    // Trampoline function that bridges the C callback to the C++ lambda.
    static void completion_callback(void* context) {
        // Cast the context back to a pointer to a std::function.
        const auto mock = static_cast<MockCompletion*>(context);
        mock->onComplete();
    }
};

struct MockStaticCPU {
    static void enterCriticalSection() { MockCPU::impl->enterCriticalSection(); }
    static void leaveCriticalSection() { MockCPU::impl->leaveCriticalSection(); }
    static void sleep() { MockCPU::impl->sleep(); }
    static uint32_t getSystemTick() { return MockCPU::impl->getSystemTick(); }
};

struct SchedulerHelper {
    using TestScheduler = Scheduler< StrictMock<MockStaticCPU>, 10 >;
    StrictMock<MockCPU> cpu;

    constexpr static uint32_t maxTaskCount = TestScheduler::maxTaskCount;
    // consteval static uint32_t maxTaskCount() {
    //     return TestScheduler::maxTaskCount;
    // }

    void update() {
        scheduler.update();
    }

    int schedule(uint32_t delay, uint32_t id = kSchedulerDefaultId) {
        completion.push_back(std::make_unique<StrictMock<MockCompletion>>());
        return scheduler.scheduleTask(MockCompletion::completion_callback, completion.back().get(), delay, id);
    }

    std::vector<std::unique_ptr<StrictMock<MockCompletion>>> completion;

private:
    TestScheduler scheduler;
    size_t scheduled = 0;
};

/// One tasks is scheduled
/// Completion is happened with no delay
TEST(Scheduler, ScheduleForImmediateCall) {
    InSequence s;
    SchedulerHelper scheduler;
    MockCPU cpu;

    EXPECT_CALL(cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(cpu, enterCriticalSection());
    EXPECT_CALL(cpu, leaveCriticalSection());
    scheduler.schedule(0);

    EXPECT_CALL(cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(cpu, enterCriticalSection());
    EXPECT_CALL(cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[0], onComplete());
    scheduler.update();
}

/// One tasks is scheduled
/// Completion is happened with delay
TEST(Scheduler, ScheduleForDelayedCall) {
    InSequence s;
    SchedulerHelper scheduler;

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(10);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(5));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(10));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[0], onComplete());
    scheduler.update();
}

/// One tasks is scheduled and re-scheduled to a later time
/// Completion is happened with delay
TEST(Scheduler, ScheduleReuseID) {
    InSequence s;
    SchedulerHelper scheduler;

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(10);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(5));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(10));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(10, 1);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(15));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(20));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[1], onComplete());
    scheduler.update();
}

/// One tasks is scheduled
/// Completion is happened with delay
/// Timer is overflown
TEST(Scheduler, ScheduleForDelayedCompletionOverflowCounter) {
    InSequence s;
    SchedulerHelper scheduler;

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(-10));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(20);

    // No completion shall happen on timer=-1
    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(-1));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    // No completion shall happen on timer=5
    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(5));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    // Completion shall happen on timer=10
    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(10));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[0], onComplete());
    scheduler.update();
}

/// Multiple tasks is scheduled
/// Completion is happened simultaneously with delay
TEST(Scheduler, ScheduleMultiForDelayedCallSimultaneousCompletion) {
    InSequence s;
    SchedulerHelper scheduler;

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(10);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(20);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(5));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(20));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[0], onComplete());
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[1], onComplete());
    scheduler.update();
}

/// Multiple tasks is scheduled
/// Completion is happened serially with delay
TEST(Scheduler, ScheduleMultiForDelayedCallSerialCompletion) {
    InSequence s;
    SchedulerHelper scheduler;

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(10);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(20);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(5));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(10));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[0], onComplete());
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(20));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[1], onComplete());
    scheduler.update();
}

/// Multiple tasks is scheduled out of order
/// Completion is happened serially in order with delay
TEST(Scheduler, ScheduleForDelayedCompletionOutOfOrder) {
    InSequence s;
    SchedulerHelper scheduler;

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(20);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.schedule(10);

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(5));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(10));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[1], onComplete());
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(20));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    EXPECT_CALL(*scheduler.completion[0], onComplete());
    scheduler.update();
}

/// Too many tasks is scheduled
/// Completion is happened only to those fitting into task queue
TEST(Scheduler, ScheduleForDelayedCompletionOverflowTasks) {
    InSequence s;
    SchedulerHelper scheduler;

    for (size_t i = 0; i < SchedulerHelper::maxTaskCount + 10; i++) {
        EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(0));
        EXPECT_CALL(scheduler.cpu, enterCriticalSection());
        EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
        scheduler.schedule(20);
    }

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(20));
    for (size_t i = 0; i < SchedulerHelper::maxTaskCount; i++) {
        EXPECT_CALL(scheduler.cpu, enterCriticalSection());
        EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
        EXPECT_CALL(*scheduler.completion[i], onComplete());
    }
    scheduler.update();

    EXPECT_CALL(scheduler.cpu, getSystemTick()).WillOnce(Return(30));
    EXPECT_CALL(scheduler.cpu, enterCriticalSection());
    EXPECT_CALL(scheduler.cpu, leaveCriticalSection());
    scheduler.update();
}
