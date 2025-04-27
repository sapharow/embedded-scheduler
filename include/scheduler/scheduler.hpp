//
// Created by Iskandar Safarov on 20/3/2025.
//

#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#ifndef _Nonnull
#define _Nonnull
#endif

#ifndef _Nullable
#define _Nullable
#endif

using CompletionHandler = void (*)(void* _Nullable);
using SchedulerTaskID = uint32_t;

/// Pass this to ID parameter of `scheduleTask` in order to avoid ID reusing and return new ID
static constexpr SchedulerTaskID kSchedulerDefaultId = 0;

template <typename T>
concept SchedulerConcept = requires(T t, CompletionHandler handler, void* user, uint32_t delay, SchedulerTaskID id) {
    /// Start critical section
    { t.update() } -> std::same_as<void>;

    /// @brief Schedule task for execution
    /// @param [in] func Completion handler scheduled for execution
    /// @param [in] user Function parameter passed
    /// @param [in] delay Execution delay, CPU ticks
    /// @return scheduling status
    { t.scheduleTask(handler, user, delay, id) } -> std::same_as<SchedulerTaskID>;
};

/// General CPU-related routines
template <typename T>
concept CPUConcept = requires(T t) {
    /// Start a critical section
    { T::enterCriticalSection() } -> std::same_as<void>;

    /// End critical section
    { T::leaveCriticalSection() } -> std::same_as<void>;

    /// Sleep CPU
    { T::sleep() } -> std::same_as<void>;

    /// Return system tick
    { T::getSystemTick() } -> std::same_as<uint32_t>;
};

template <CPUConcept CPU, uint32_t MaxTaskCount>
class Scheduler final {
public:

    constexpr static uint32_t maxTaskCount = MaxTaskCount;

    /// @note Needs to be run from the main loop
    void update() {
        const uint32_t now = CPU::getSystemTick();

        // Check if the first (earliest) task is due.
        while (true) {
            CPU::enterCriticalSection();
            if (_taskCount == 0) {
                CPU::leaveCriticalSection();
                break;
            }

            // Using signed arithmetic handles tick overflow correctly.
            if (static_cast<int32_t>(now - _taskList[0].executeTime) < 0) {
                CPU::leaveCriticalSection();
                break; // No task is ready yet
            }

            // Copy the task to a local variable.
            const auto task = _taskList[0];

            // Remove the executed task from the list (shift left).
            for (int i = 1; i < _taskCount; i++) {
                _taskList[i - 1] = _taskList[i];
            }
            _taskCount = _taskCount - 1;
            const int task_count = _taskCount;
            CPU::leaveCriticalSection();

            // Call the scheduled function with interrupts enabled.
            task.func(task.param);

            if (task_count == 0) {
                return;
            }
        }
    }

    /// @brief Unschedule task
    /// @param [in] taskId Identifier of the task to unschedule
    void unscheduleTask(SchedulerTaskID taskId) {
        CPU::enterCriticalSection();
        for (int i = 0; i < _taskCount; i++) {
            if (_taskList[i].id == taskId) {
                for (int k = i + 1; k < _taskCount; k++) {
                    _taskList[k - 1] = _taskList[k];
                }
                _taskCount = _taskCount - 1;
                break;
            }
        }
        CPU::leaveCriticalSection();
    }

    /// @brief Schedule task for execution
    /// @param [in] func Completion handler scheduled for execution
    /// @param [in] param Function parameter passed
    /// @param [in] delay Execution delay, CPU ticks
    /// @param [in] reuseId Attempt to reuse task ID effectively replacing old task by this one
    /// @return scheduling status
    SchedulerTaskID scheduleTask(CompletionHandler _Nonnull func, void* _Nullable param, uint32_t delay,
                                 SchedulerTaskID reuseId = kSchedulerDefaultId) {
        // Compute the absolute time when the task should run.
        const uint32_t target_time = CPU::getSystemTick() + delay;

        CPU::enterCriticalSection();

        if (reuseId) {
            for (int i = 0; i < _taskCount; i++) {
                if (_taskList[i].id == reuseId) {
                    // Remove task
                    for (int k = i + 1; k < _taskCount; k++) {
                        _taskList[k - 1] = _taskList[k];
                    }
                    _taskCount = _taskCount - 1;
                    break;
                }
            }
        }

        if (_taskCount >= static_cast<int>(MaxTaskCount)) {
            CPU::leaveCriticalSection();
            return 0; // No free slot available
        }

        // Insert the new task in sorted order (ascending execute_time)
        int i = _taskCount - 1;
        // Shift tasks that are scheduled after target_time one slot back.
        while (i >= 0 && static_cast<int32_t>(target_time - _taskList[i].executeTime) < 0) {
            _taskList[i + 1] = _taskList[i];
            i--;
        }
        _taskList[i + 1].executeTime = target_time;
        _taskList[i + 1].func = func;
        _taskList[i + 1].param = param;

        SchedulerTaskID id;
        if (reuseId) {
            id = reuseId;
        } else {
            id = _id;
            _id = _id + 1;
        }
        _taskList[i + 1].id = id;
        _taskCount = _taskCount + 1;

        CPU::leaveCriticalSection();

        return id;
    }

private:
    // Task structure for a scheduled function call.
    struct Task {
        uint32_t executeTime = 0; // Absolute system tick when the task should run
        CompletionHandler _Nullable func = nullptr; // Function pointer to call
        void* _Nullable param = nullptr; // Parameter to pass to the function
        SchedulerTaskID id = 0;
    };

    Task _taskList[MaxTaskCount]{};
    volatile int _taskCount = 0;
    volatile SchedulerTaskID _id = 1;
};

#endif //SCHEDULER_HPP
