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

  enum CpuPriority {
    kIdle = 0,
    kLow = 1,
    kNormal = 2,
    kHigh = 3,
  };

  // Arrange to run "(*function)(arg)" once in a background thread, in
  // the thread pool specified by pri. By default, jobs go to the 'LOW'
  // priority thread pool.

  // "function" may run in an unspecified thread.  Multiple functions
  // added to the same Env may run concurrently in different threads.
  // I.e., the caller may not assume that background work items are
  // serialized.
  // When the UnSchedule function is called, the unschedFunction
  // registered at the time of Schedule is invoked with arg as a parameter.
  virtual void Schedule(void (*function)(void* arg), void* arg,
                        Priority pri = LOW, void* tag = nullptr,
                        void (*unschedFunction)(void* arg) = nullptr) = 0;

  // Arrange to remove jobs for given arg from the queue_ if they are not
  // already scheduled. Caller is expected to have exclusive lock on arg.
  virtual int UnSchedule(void* /*arg*/, Priority /*pri*/) { return 0; }

  // Start a new thread, invoking "function(arg)" within the new thread.
  // When "function(arg)" returns, the thread will be destroyed.
  virtual void StartThread(void (*function)(void* arg), void* arg) = 0;

    // Wait for all threads started by StartThread to terminate.
  virtual void WaitForJoin() {}

  // Reserve available background threads in the specified thread pool.
  virtual int ReserveThreads(int /*threads_to_be_reserved*/, Priority /*pri*/) {
    return 0;
  }

  // Release a specific number of reserved threads from the specified thread
  // pool
  virtual int ReleaseThreads(int /*threads_to_be_released*/, Priority /*pri*/) {
    return 0;
  }

  // Get thread pool queue length for specific thread pool.
  virtual unsigned int GetThreadPoolQueueLen(Priority /*pri*/ = LOW) const {
    return 0;
  }

};

// Env封装类
class EnvWrapper : public Env{
public:
  explicit EnvWrapper(Env* target) : target_(target){};
  virtual ~EnvWrapper() = 0;

  void Schedule(void (*f)(void* arg), void* a, Priority pri,
                void* tag = nullptr, void (*u)(void* arg) = nullptr) override {
    return target_->Schedule(f, a, pri, tag, u);
  }

  int UnSchedule(void* tag, Priority pri) override {
    return target_->UnSchedule(tag, pri);
  }

  void StartThread(void (*f)(void*), void* a) override {
    return target_->StartThread(f, a);
  }

  void WaitForJoin() override {
    return target_->WaitForJoin(); 
  }

  int ReserveThreads(int threads_to_be_reserved, Priority pri) override {
    return target_->ReserveThreads(threads_to_be_reserved, pri);
  } 

  int ReleaseThreads(int threads_to_be_released, Priority pri) override {
    return target_->ReleaseThreads(threads_to_be_released, pri);
  }

  unsigned int GetThreadPoolQueueLen(Priority pri = LOW) const override {
    return target_->GetThreadPoolQueueLen(pri);
  }

private:
  Env* target_;
};