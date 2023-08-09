#include "threadpool_impl.h"

#ifndef OS_WIN
#include <unistd.h>
#endif

#ifdef OS_LINUX
#include <sys/resource.h>
#include <sys/syscall.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

struct ThreadPoolImpl::Impl { 
public:
  Impl();
  ~Impl();

  void JoinThreads(bool wait_for_jobs_to_complete);

  void SetBackgroundThreadsInternal(int num, bool allow_reduce);
  int GetBackgroundThreads();

  unsigned int GetQueueLen() const {
    return queue_len_.load(std::memory_order_relaxed);
  }

  void LowerIOPriority();

  void LowerCPUPriority(Env::CpuPriority pri);

  void WakeUpAllThreads() { bgsignal_.notify_all(); }

  void BGThread(size_t thread_id);

  void StartBGThreads();

  void Submit(std::function<void()>&& schedule,
              std::function<void()>&& unschedule, void* tag);

  int UnSchedule(void* arg);

  void SetHostEnv(Env* env) { env_ = env; }

  Env* GetHostEnv() const { return env_; }

  bool HasExcessiveThread() const {
    return static_cast<int>(bgthreads_.size()) > total_threads_limit_;
  }

  // Return true iff the current thread is the excessive thread to terminate.
  // Always terminate the running thread that is added last, even if there are
  // more than one thread to terminate.
  bool IsLastExcessiveThread(size_t thread_id) const {
    return HasExcessiveThread() && thread_id == bgthreads_.size() - 1;
  }

  bool IsExcessiveThread(size_t thread_id) const {
    return static_cast<int>(thread_id) >= total_threads_limit_;
  }

  // Return the thread priority.
  // This would allow its member-thread to know its priority.
  Env::Priority GetThreadPriority() const { return priority_; }

  // Set the thread priority.
  void SetThreadPriority(Env::Priority priority) { priority_ = priority; }

private:
  static void BGThreadWrapper(void* arg);

  bool low_io_priority_;               // IO优先级是否为low
  Env::CpuPriority cpu_priority_;      // CPU优先级
  Env::Priority priority_;             // 线程池优先级
  Env* env_;                           // 全局Env
  int queue_len_;                      // 任务队列queue_的长度
    
  int total_threads_limit_;            // 线程池容纳的最大线程数
  
  bool exit_all_threads_;              // 所有线程是否退出
  bool wait_for_jobs_to_complete_;     // 是否等待所有线程执行完毕

  // Entry per Schedule()/Submit() call
  // 任务
  struct BGItem {
    void* tag = nullptr;
    std::function<void()> function;
    std::function<void()> unschedFunction;
  };

  using BGQueue = std::deque<BGItem>;
  BGQueue queue_;                       // 任务队列

  std::mutex mu_;                       // 锁
  std::condition_variable bgsignal_;    // 信号
  std::vector<std::thread> bgthreads_;  // 已有线程
};

inline ThreadPoolImpl::Impl::Impl()
    : low_io_priority_(false),
      cpu_priority_(Env::CpuPriority::kNormal),
      priority_(Env::LOW),
      env_(nullptr),
      total_threads_limit_(0),
      exit_all_threads_(false),
      wait_for_jobs_to_complete_(false),
      queue_(),
      mu_(),
      bgsignal_(),
      bgthreads_() {}

inline ThreadPoolImpl::Impl::~Impl(){
  assert(bgthreads_.size() == 0);
}

// 清除所有线程
void ThreadPoolImpl::Impl::JoinThreads(bool wait_for_jobs_to_complete){
  std::unique_lock<std::mutex> lock(mu_);
  assert(!exit_all_threads_);

  wait_for_jobs_to_complete_ = wait_for_jobs_to_complete;
  exit_all_threads_ = true;
  // prevent threads from being recreated right after they're joined, in case
  // the user is concurrently submitting jobs.
  total_threads_limit_ = 0;

  lock.unlock();

  bgsignal_.notify_all();

  for(auto& th : bgthreads_){
    th.join();
  }

  bgthreads_.clear();
  exit_all_threads_ = false;
}

// IO优先级降为low
inline void ThreadPoolImpl::Impl::LowerIOPriority(){
  std::lock_guard<std::mutex> lock(mu_);
  low_io_priority_ = true;
}






