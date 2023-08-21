#pragma once
#include <stdint.h>
#include <cstdarg>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

// Env抽象类
class Env {
public:
  Env();
  virtual ~Env() = 0;

  static Env* Default();

  // Priority for scheduling job in thread pool
  enum Priority { BOTTOM, LOW, HIGH, USER, TOTAL };

  // Arrange to run "(*function)(arg)" once in a background thread, in
  // the thread pool specified by pri. By default, jobs go to the 'LOW'
  // priority thread pool.

  // "function" may run in an unspecified thread.  Multiple functions
  // added to the same Env may run concurrently in different threads.
  // I.e., the caller may not assume that background work items are
  // serialized.
  // When the UnSchedule function is called, the unschedFunction
  // registered at the time of Schedule is invoked with arg as a parameter.
  virtual void Schedule(void (*function)(void* arg1), void* arg1,
                        Priority pri = LOW, void* tag = nullptr,
                        void (*unschedFunction)(void* arg) = nullptr, void* arg2 = nullptr) = 0;

  // Arrange to remove jobs for given arg from the queue_ if they are not
  // already scheduled. Caller is expected to have exclusive lock on arg.
  virtual int UnSchedule(void* /*arg*/, Priority /*pri*/) { return 0; }

  // wake up all threads and join them so that they can end
  virtual void JoinAllThreads(Priority pri){}

  // JoinAllThreads but wait the uncompleted jobs
  virtual void WaitForJobsAndJoinAllThreads(Priority pri){}

  // Get thread pool queue length for specific thread pool.
  virtual unsigned int GetThreadPoolQueueLen(Priority /*pri*/ = LOW) const {
    return 0;
  }

  // The number of background worker threads of a specific thread pool
  // for this environment. 'LOW' is the default pool.
  // default number: 1
  virtual void SetBackgroundThreads(int num, Priority pri) {}

  virtual int GetBackgroundThreads(Priority pri) {
    return 0;
  }

  virtual void LowerThreadPoolIOPriority(Priority pri) {}

  virtual void LowerThreadPoolCPUPriority(Priority pri) {}

};