#pragma once
#if defined(OS_LINUX)
#include <linux/fs.h>
#endif
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(OS_LINUX) || defined(OS_SOLARIS) || defined(OS_ANDROID)
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#endif
#include <sys/time.h>
#include <time.h>
#include <algorithm>
// Get nano time includes
#if defined(OS_LINUX) || defined(OS_FREEBSD)
#elif defined(__MACH__)
#include <mach/clock.h>
#include <mach/mach.h>
#else
#include <chrono>
#endif
#include <deque>
#include <vector>

#include "threadpool_impl.h"
#include "env.h"

// Env实现类
class PosixEnv : public Env {
public:
  PosixEnv(/* args */);
  ~PosixEnv();

  void Schedule(void (*function)(void* arg1), void* arg1, Priority pri = LOW,
                void* tag = nullptr,
                void (*unschedFunction)(void* arg2) = nullptr, void* arg2 = nullptr) override;

  int UnSchedule(void* arg, Priority pri) override;

  void JoinAllThreads(Priority pri) override;

  void WaitForJobsAndJoinAllThreads(Priority pri) override;

  unsigned int GetThreadPoolQueueLen(Priority pri = LOW) const override;

  // Allow increasing the number of worker threads.
  void SetBackgroundThreads(int num, Priority pri);

  int GetBackgroundThreads(Priority pri) override;

private:
  friend Env* Env::Default();

  std::vector<ThreadPoolImpl> thread_pools_;
  pthread_mutex_t mu_;
};

