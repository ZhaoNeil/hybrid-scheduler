#include <stdio.h>

#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include <iostream>

#include "lib/base.h"
#include "lib/ghost.h"

// A series of simple tests for ghOSt schedulers.

namespace ghost {
namespace {

std::mutex mtx;

struct ScopedTime {
  ScopedTime() { start = absl::Now(); }
  ~ScopedTime() {
    printf(" took %0.2f ms\n", absl::ToDoubleMilliseconds(absl::Now() - start));
  }
  absl::Time start;
};

void threadFunction(int threadId) {
    int n = 0;
    for (int i = 0; i < 1; ++i) {
        std::cout << "Thread " << threadId << " is running iteration " << i << std::endl;
        // std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (int j = 1; j < 10000; ++j) {
            for (int k = 1; k < 100000; ++k) {
                n += j*k;
            }
        }
        printf("N is %d\n", n);
    } 
    // std::lock_guard<std::mutex> lock(mtx);
    std::cout << "Thread " << threadId << " completed." << std::endl;
}

void test (int num_threads) {
  std::vector<std::unique_ptr<GhostThread>> threads;

  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(
      new GhostThread(GhostThread::KernelScheduler::kGhost, [i] {
          absl::SleepFor(absl::Milliseconds(10));
          std::thread t([i] {CHECK_EQ(sched_getscheduler(/*pid=*/0), SCHED_GHOST);
                                      threadFunction(i); });
          absl::Time thread_created = absl::Now();
          printf("Thread %d created\n", i);
          t.join();
          absl::Time thread_joined = absl::Now();
          absl::SleepFor(absl::Milliseconds(10));
        }));
  }

  for (auto& t : threads) t->Join();
}

}  // namespace
}  // namespace ghost

int main() {
  {
    printf("Test\n");
    // ghost::ScopedTime time;
    ghost::test(10);
  }
  return 0;
}
