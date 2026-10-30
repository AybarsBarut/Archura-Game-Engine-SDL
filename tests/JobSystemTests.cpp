#include "core/threading/JobSystem.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;
bool failInitialization = false;

void Check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
struct CopyControlledJob {
  std::atomic<int>* remainingCopies;
  std::atomic<int>* completed;
  CopyControlledJob(std::atomic<int>& copies, std::atomic<int>& calls)
      : remainingCopies(&copies), completed(&calls) {}
  CopyControlledJob(CopyControlledJob&&) noexcept = default;
  CopyControlledJob(const CopyControlledJob& other)
      : remainingCopies(other.remainingCopies), completed(other.completed) {
    if (remainingCopies->fetch_sub(1) <= 0)
      throw std::runtime_error("injected callable copy failure");
  }
  void operator()() const { ++*completed; }
};

void TestEnqueueFailure() {
  std::atomic<int> copies{0}, completed{0};
  Archura::JobSystem::Job job = CopyControlledJob(copies, completed);
  Archura::JobSystem::Init();
  bool threw = false;
  try { Archura::JobSystem::Execute(job); }
  catch (const std::runtime_error&) { threw = true; }
  Check(threw, "enqueue copy failure propagates to submitting caller");
  const bool busy = Archura::JobSystem::IsBusy();
  Check(!busy, "failed enqueue must not leave an active job");
  Check(completed == 0, "failed enqueue must not invoke callback");
  // Avoid hanging the pre-fix implementation on its corrupted counter.
  if (!busy) {
    Archura::JobSystem::Execute([&] { ++completed; });
    Archura::JobSystem::Wait();
    Check(completed == 1, "queue remains usable after failed enqueue");
  }
  Archura::JobSystem::Shutdown();
}

void TestWorkerDoesNotCopy() {
  std::atomic<int> copies{1}, completed{0};
  Archura::JobSystem::Job job = CopyControlledJob(copies, completed);
  Archura::JobSystem::Init();
  Archura::JobSystem::Execute(job);
  Archura::JobSystem::Wait();
  Check(completed == 1, "accepted callback executes without another callable copy");
  Check(copies == 0, "only the submitting copy should consume the copy budget");
  Archura::JobSystem::Shutdown();
}

struct CleanupProbe {
  std::promise<void>* entered;
  std::shared_future<void> release;
  ~CleanupProbe() { entered->set_value(); release.wait(); }
};

struct BlockingCopyJob {
  std::promise<void>* entered;
  std::shared_future<void> release;
  std::atomic<int>* completed;
  BlockingCopyJob(std::promise<void>& copied, std::shared_future<void> gate, std::atomic<int>& calls)
      : entered(&copied), release(std::move(gate)), completed(&calls) {}
  BlockingCopyJob(BlockingCopyJob&&) noexcept = default;
  BlockingCopyJob(const BlockingCopyJob& other)
      : entered(other.entered), release(other.release), completed(other.completed) {
    entered->set_value();
    release.wait();
  }
  void operator()() const { ++*completed; }
};

void TestSubmissionAcrossShutdown() {
  std::promise<void> entered, release;
  auto copied = entered.get_future();
  std::atomic<int> completed{0};
  Archura::JobSystem::Job job = BlockingCopyJob(entered, release.get_future().share(), completed);
  Archura::JobSystem::Init();
  auto submitting = std::async(std::launch::async, [&] { Archura::JobSystem::Execute(job); });
  copied.wait();
  Archura::JobSystem::Shutdown();
  Archura::JobSystem::Init();
  release.set_value();
  submitting.get();
  Check(completed == 0, "submission copied across shutdown is rejected before queue admission");
  Check(!Archura::JobSystem::IsBusy(), "late rejected submission leaves no pending work");
  Archura::JobSystem::Wait();
  Check(completed == 0, "late rejected submission cannot revive at restart");
  Archura::JobSystem::Execute([&] { ++completed; });
  Archura::JobSystem::Wait();
  Check(completed == 1, "restarted session accepts its own new work");
  Archura::JobSystem::Shutdown();
}

void TestCleanupBeforeIdle() {
  std::promise<void> run, cleanupEntered, releaseCleanup;
  auto runGate = run.get_future().share();
  auto entered = cleanupEntered.get_future();
  auto probe = std::make_shared<CleanupProbe>();
  probe->entered = &cleanupEntered;
  probe->release = releaseCleanup.get_future().share();
  std::weak_ptr<CleanupProbe> lifetime = probe;
  Archura::JobSystem::Job job = [probe, runGate] { runGate.wait(); };
  Archura::JobSystem::Execute(job);
  job = {};
  probe.reset();
  run.set_value();
  entered.wait();
  Check(Archura::JobSystem::IsBusy(), "callback cleanup is part of active work");
  releaseCleanup.set_value();
  Archura::JobSystem::Wait();
  Check(lifetime.expired(), "Wait includes destruction of owned callback captures");
}

void TestLifecycle() {
  std::atomic<int> completed{0};
  std::atomic<int> badGroup{0};
  Archura::JobSystem::Shutdown();
  Archura::JobSystem::Wait();
  Archura::JobSystem::Execute([&] { ++completed; });
  Check(!Archura::JobSystem::IsBusy(), "stopped queue rejects work");
  Archura::JobSystem::Init();
  Archura::JobSystem::Init();
  Archura::JobSystem::Execute({});
  Archura::JobSystem::Dispatch(8, 2, [&](Archura::JobSystem::JobDispatchArgs args) {
    if (args.groupIndex != args.jobIndex / 2)
      ++badGroup;
    ++completed;
  });
  Archura::JobSystem::Wait();
  Check(completed == 8, "all dispatched jobs should complete");
  Check(badGroup == 0, "dispatch group indices should respect groupSize");
  Archura::JobSystem::Execute([] { throw std::runtime_error("expected test exception"); });
  Archura::JobSystem::Execute([] { throw 7; });
  Archura::JobSystem::Execute([&] { ++completed; });
  Archura::JobSystem::Wait();
  Check(completed == 9, "throwing callbacks do not prevent later work");
  TestCleanupBeforeIdle();

  std::promise<void> finish;
  auto gate = finish.get_future().share();
  Archura::JobSystem::Execute([gate] { gate.wait(); });
  auto first = std::async(std::launch::async, [] { Archura::JobSystem::Wait(); });
  auto second = std::async(std::launch::async, [] { Archura::JobSystem::Wait(); });
  finish.set_value();
  first.get();
  second.get();
  Check(!Archura::JobSystem::IsBusy(), "all external waiters observe idle completion");
  Archura::JobSystem::Shutdown();
  TestSubmissionAcrossShutdown();

  for (int cycle = 0; cycle < 3; ++cycle) {
    Archura::JobSystem::Init();
    completed = 0;
    for (int i = 0; i < 128; ++i)
      Archura::JobSystem::Execute([&] { ++completed; });
    std::promise<void> start;
    auto startGate = start.get_future().share();
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
      producers.emplace_back([&, startGate] {
        startGate.wait();
        for (int i = 0; i < 128; ++i)
          Archura::JobSystem::Execute([&] { ++completed; });
      });
    }
    start.set_value();
    Archura::JobSystem::Shutdown();
    for (auto& producer : producers) producer.join();
    Check(completed >= 128, "shutdown drains work accepted before producer race");
    Check(!Archura::JobSystem::IsBusy(), "shutdown leaves no orphaned accepted work");
    Archura::JobSystem::Wait();
    const int drained = completed;
    Archura::JobSystem::Execute([&] { ++completed; });
    Check(completed == drained, "submissions after shutdown cannot execute");
    Archura::JobSystem::Init();
    Archura::JobSystem::Wait();
    Check(completed == drained, "restart cannot revive rejected work");
    Archura::JobSystem::Shutdown();
  }
  Archura::JobSystem::Shutdown();
}

void TestInitializationFailure() {
  failInitialization = true;
  bool threw = false;
  try { Archura::JobSystem::Init(); }
  catch (const std::runtime_error&) { threw = true; }
  failInitialization = false;
  Check(threw, "partial initialization propagates failure");
  Check(!Archura::JobSystem::IsBusy(), "failed initialization leaves no pending work");
  Archura::JobSystem::Wait();
  Archura::JobSystem::Shutdown();
  Archura::JobSystem::Init();
  std::atomic<int> completed{0};
  Archura::JobSystem::Execute([&] { ++completed; });
  Archura::JobSystem::Wait();
  Check(completed == 1, "initialization can restart after partial failure");
  Archura::JobSystem::Shutdown();
}
} // namespace

namespace Archura::JobSystemTest {
void AfterWorkerCreated(uint32_t) {
  if (failInitialization) throw std::runtime_error("injected partial initialization failure");
}
} // namespace Archura::JobSystemTest

int main(int argc, char** argv) {
  std::set_terminate([] {
    std::cerr << "FAIL: exception escaped worker boundary\n";
    std::_Exit(86);
  });
  const std::string mode = argc > 1 ? argv[1] : "lifecycle";
  if (mode == "enqueue-failure") TestEnqueueFailure();
  else if (mode == "worker-copy") TestWorkerDoesNotCopy();
  else if (mode == "init-failure") TestInitializationFailure();
  else TestLifecycle();
  return failures == 0 ? 0 : 1;
}
