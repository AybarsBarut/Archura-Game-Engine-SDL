#include "core/threading/JobSystem.h"

#include <atomic>
#include <iostream>

namespace {
int failures = 0;

void Check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
} // namespace

int main() {
  std::atomic<int> completed{0};
  std::atomic<int> badGroup{0};

  Archura::JobSystem::Init();
  Archura::JobSystem::Dispatch(8, 2, [&](Archura::JobSystem::JobDispatchArgs args) {
    if (args.groupIndex != args.jobIndex / 2)
      ++badGroup;
    ++completed;
  });
  Archura::JobSystem::Wait();
  Check(completed == 8, "all dispatched jobs should complete");
  Check(badGroup == 0, "dispatch group indices should respect groupSize");
  Archura::JobSystem::Shutdown();

  return failures == 0 ? 0 : 1;
}
