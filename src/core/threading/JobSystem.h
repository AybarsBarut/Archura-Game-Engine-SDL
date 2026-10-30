#pragma once

#include <functional>
#include <cstdint>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>

namespace Archura {

class JobSystem {
public:
    using Job = std::function<void()>;

    // Utility struct for parallel loops
    struct JobDispatchArgs {
        uint32_t jobIndex;
        uint32_t groupIndex;
    };

    // Init/Shutdown must be externally serialized and called outside worker jobs.
    // Shutdown drains accepted work; concurrent Execute calls may be rejected.
    static void Init();
    static void Shutdown();

    // Copies the callback into the queue, or ignores empty/stopped submissions.
    // A failed copy/allocation propagates without accepting the job.
    // A copy spanning a shutdown/restart cannot enter the new session.
    static void Execute(const Job& job);
    static void Dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& job);
    
    // Idle includes destruction of callback captures. Wait must not run in a job.
    // Callers must separately synchronize producers to wait for a closed batch.
    static bool IsBusy();
    static void Wait();

private:
    static void WorkerThread();

    // shared state
    static std::vector<std::thread> s_WorkerThreads;
    static std::deque<Job> s_JobQueue;
    static std::mutex s_QueueMutex;
    static std::condition_variable s_Condition;
    static std::condition_variable s_IdleCondition;
    static std::atomic<bool> s_Running;
    static uint64_t s_Session; // Protected by s_QueueMutex.
    static std::atomic<uint32_t> s_ActiveJobs; // Tracks jobs currently running + in queue
};

} // namespace Archura
