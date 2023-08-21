#include "threadpool_impl.h"
#include "iostream"
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

  Env::Priority priority_;             // 线程池优先级
  Env* env_;                           // 全局Env
  std::atomic_uint queue_len_;         // 任务队列queue_的长度
    
  int total_threads_limit_;            // 线程池容纳的最大线程数
  
  int num_waiting_threads_;            // 正在等待的线程数
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
    : priority_(Env::LOW),
      env_(nullptr),
      queue_len_(0),
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


// 线程函数
void ThreadPoolImpl::Impl::BGThread(size_t thread_id){
  // One io priority is said to be higher than another one 
  // if it belongs to a higher priority class.
  int io_priority_class = priority_;

  // The cpu prio argument is a value in the range -20 to 19
  // with -20 being the highest priority and 19 being the lowest priority
  int cpu_priority = 19 - 5*priority_; 
  cpu_priority = std::max(cpu_priority, -20);

  // Set the io and cpu priority
#ifdef OS_LINUX
#define IOPRIO_CLASS_SHIFT (13)
#define IOPRIO_PRIO_VALUE(class, data) (((class) << IOPRIO_CLASS_SHIFT) | data)
  // io: https://www.man7.org/linux/man-pages/man2/ioprio_set.2.html
  syscall(SYS_ioprio_set, 1,  // IOPRIO_WHO_PROCESS
          0,                  // current thread
          IOPRIO_PRIO_VALUE(io_priority_class, 0));

  // cpu: https://www.man7.org/linux/man-pages/man2/setpriority.2.html
  setpriority(
    PRIO_PROCESS,
    // Current thread.
    0,
    // nice value
    cpu_priority);

#else
    (void)io_priority_class;  // avoid 'unused variable' error
    (void)cpu_priority;
#endif


  while(true){
    // Wait until there is an item that is ready to run
    std::unique_lock<std::mutex> lock(mu_);

    // Stop waiting if the thread needs to do work or needs to terminate.
    // Increase num_waiting_threads_ once this task has started waiting.
    num_waiting_threads_ ++;

    // When not exist_all_threads and the current thread id is not the last
    // excessive thread, it may be blocked due to 3 reasons: 1) queue is empty
    // 2) it is the excessive thread (not the last one)
    // 3) the number of waiting threads is not greater than reserved threads
    // (i.e, no available threads due to full reservation")
    while(!exit_all_threads_ && !IsLastExcessiveThread(thread_id) &&
          (queue_.empty() || IsExcessiveThread(thread_id))) {
      bgsignal_.wait(lock);        
    }

    // Decrease num_waiting_threads_ once the thread is not waiting
    num_waiting_threads_--;

    if(exit_all_threads_){
      if(!wait_for_jobs_to_complete_ || queue_.empty()){
        break;
      }
    } else if(IsLastExcessiveThread(thread_id)) {
      // Current thread is the last generated one and is excessive.
      // We always terminate excessive thread in the reverse order of
      // generation time. But not when `exit_all_threads_ == true`,
      // otherwise `JoinThreads()` could try to `join()` a `detach()`ed
      // thread.
      auto& terminating_thread = bgthreads_.back();
      terminating_thread.detach();
      bgthreads_.pop_back();
      if(HasExcessiveThread()){
        // There is still at least more excessive thread to terminate.
        WakeUpAllThreads();
      }
      break;
    }

    auto func = std::move(queue_.front().function);
    queue_.pop_front();
    queue_len_.store(static_cast<unsigned int>(queue_.size()), std::memory_order_relaxed);

    lock.unlock();

    // 执行任务
    func();
  }
}

// Helper struct for passing arguments when creating threads
struct BGThreadMetadata {
  ThreadPoolImpl::Impl* thread_pool_;
  size_t thread_id_;  // Thread count in the thread.
  BGThreadMetadata(ThreadPoolImpl::Impl* thread_pool, size_t thread_id)
      : thread_pool_(thread_pool), thread_id_(thread_id){}
};

// BGThread()的封装
void ThreadPoolImpl::Impl::BGThreadWrapper(void* arg){
  BGThreadMetadata* meta = reinterpret_cast<BGThreadMetadata*>(arg);
  size_t thread_id = meta->thread_id_;
  ThreadPoolImpl::Impl* tp = meta->thread_pool_;

  delete meta;
  tp->BGThread(thread_id);
}

void ThreadPoolImpl::Impl::SetBackgroundThreadsInternal(int num,
                                                        bool allow_reduce) {
  std::lock_guard<std::mutex> lock(mu_);
  if (exit_all_threads_) {
    return;
  }
  if (num > total_threads_limit_ ||
      (num < total_threads_limit_ && allow_reduce)) {
    total_threads_limit_ = std::max(0, num);
    WakeUpAllThreads();
    StartBGThreads();
  }
}

int ThreadPoolImpl::Impl::GetBackgroundThreads() {
  std::unique_lock<std::mutex> lock(mu_);
  return total_threads_limit_;
}

// 新增一个BGThread
void ThreadPoolImpl::Impl::StartBGThreads() {
  // Start background thread if necessary
  while ((int)bgthreads_.size() < total_threads_limit_) {
    std::thread p_t(&BGThreadWrapper,
                     new BGThreadMetadata(this, bgthreads_.size()));
    bgthreads_.push_back(std::move(p_t));
  }
}

// 新增一项任务
void ThreadPoolImpl::Impl::Submit(std::function<void()>&& schedule,
                                  std::function<void()>&& unschedule,
                                  void* tag) {
  std::lock_guard<std::mutex> lock(mu_);

  if (exit_all_threads_) {
    return;
  }

  StartBGThreads();

  // Add to priority queue
  queue_.push_back(BGItem());

  auto& item = queue_.back();
  item.tag = tag;
  item.function = std::move(schedule);
  item.unschedFunction = std::move(unschedule);

  queue_len_.store(static_cast<unsigned int>(queue_.size()),
                   std::memory_order_relaxed);

  if (!HasExcessiveThread()) {
    // Wake up at least one waiting thread.
    bgsignal_.notify_one();
  } else {
    // Need to wake up all threads to make sure the one woken
    // up is not the one to terminate.
    WakeUpAllThreads();
  }
}

// 撤回一项任务
int ThreadPoolImpl::Impl::UnSchedule(void* tag){
  int count = 0;

  std::vector<std::function<void()>> candidates;
  {
    std::lock_guard<std::mutex> lock(mu_);

    // Remove from queue
    BGQueue::iterator it = queue_.begin();
    while(it != queue_.end()) {
      if(it->tag == tag) {
        if(it->unschedFunction){
          candidates.push_back(it->unschedFunction);
        }
        it = queue_.erase(it);
        count ++;
      } else {
        it ++;
      }
    }

    queue_len_.store(static_cast<unsigned int>(queue_.size()),
                     std::memory_order_relaxed);
  }

  // Run unschedule functions outside the mutex
  for (auto f : candidates) {
    f();
  }

  return count;
}

ThreadPoolImpl::ThreadPoolImpl() : impl_(new Impl()) {}

ThreadPoolImpl::~ThreadPoolImpl() {}

void ThreadPoolImpl::JoinAllThreads() { impl_->JoinThreads(false); }

void ThreadPoolImpl::SetBackgroundThreads(int num) {
  impl_->SetBackgroundThreadsInternal(num, true);
}

int ThreadPoolImpl::GetBackgroundThreads() {
  return impl_->GetBackgroundThreads();
}

unsigned int ThreadPoolImpl::GetQueueLen() const {
  return impl_->GetQueueLen();
}

void ThreadPoolImpl::WaitForJobsAndJoinAllThreads() {
  impl_->JoinThreads(true);
}

void ThreadPoolImpl::IncBackgroundThreadsIfNeeded(int num) {
  impl_->SetBackgroundThreadsInternal(num, false);
}

void ThreadPoolImpl::SubmitJob(const std::function<void()>& job) {
  auto copy(job);
  impl_->Submit(std::move(copy), std::function<void()>(), nullptr);
}

void ThreadPoolImpl::SubmitJob(std::function<void()>&& job) {
  impl_->Submit(std::move(job), std::function<void()>(), nullptr);
}

void ThreadPoolImpl::Schedule(void (*function)(void* arg1), void* arg1, 
                              void* tag,
                              void (*unschedFunction)(void* arg2), void* arg2){
  if (unschedFunction == nullptr) {
    impl_->Submit(std::bind(function, arg1), std::function<void()>(), tag);
  } else {
    impl_->Submit(std::bind(function, arg1), std::bind(unschedFunction, arg2), tag);
  }
}

int ThreadPoolImpl::UnSchedule(void* tag) { return impl_->UnSchedule(tag); }

void ThreadPoolImpl::SetHostEnv(Env* env) { impl_->SetHostEnv(env); }

Env* ThreadPoolImpl::GetHostEnv() const { return impl_->GetHostEnv(); }

// Return the thread priority.
// This would allow its member-thread to know its priority.
Env::Priority ThreadPoolImpl::GetThreadPriority() const {
  return impl_->GetThreadPriority();
}

// Set the thread priority.
void ThreadPoolImpl::SetThreadPriority(Env::Priority priority) {
  impl_->SetThreadPriority(priority);
}










