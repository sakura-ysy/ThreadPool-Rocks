#include <functional>
#include <memory>
#include <env.h>
#include <threadpool.h>



class ThreadPoolImpl : public ThreadPool {
public:
  ThreadPoolImpl();
  ~ThreadPoolImpl();
  // Implement ThreadPool interfaces

  // Wait for all threads to finish.
  // Discards all the jobs that did not
  // start executing and waits for those running
  // to complete
  void JoinAllThreads() override;

  // Set the number of background threads that will be executing the
  // scheduled jobs.
  void SetBackgroundThreads(int num) override;
  int GetBackgroundThreads() override;

  // Get the number of jobs scheduled in the ThreadPool queue.
  unsigned int GetQueueLen() const override;

  // Waits for all jobs to complete those
  // that already started running and those that did not
  // start yet
  void WaitForJobsAndJoinAllThreads() override;

  // Make threads to run at a lower kernel IO priority
  // Currently only has effect on Linux
  void LowerIOPriority();

  // Make threads to run at a lower kernel CPU priority
  // Currently only has effect on Linux
  void LowerCPUPriority(Env::CpuPriority pri);

  // Ensure there is at aleast num threads in the pool
  // but do not kill threads if there are more
  void IncBackgroundThreadsIfNeeded(int num);

  // Submit a fire and forget job
  // These jobs can not be unscheduled

  // This allows to submit the same job multiple times
  void SubmitJob(const std::function<void()>&) override;
  // This moves the function in for efficiency
  void SubmitJob(std::function<void()>&&) override;

  // Schedule a job with an unschedule tag and unschedule function
  // Can be used to filter and unschedule jobs by a tag
  // that are still in the queue and did not start running
  void Schedule(void (*function)(void* arg1), void* arg, void* tag,
                void (*unschedFunction)(void* arg));

  // Filter jobs that are still in a queue and match
  // the given tag. Remove them from a queue if any
  // and for each such job execute an unschedule function
  // if such was given at scheduling time.
  int UnSchedule(void* tag);

  void SetHostEnv(Env* env);

  Env* GetHostEnv() const;

  // Return the thread priority.
  // This would allow its member-thread to know its priority.
  Env::Priority GetThreadPriority() const;

  // Set the thread priority.
  void SetThreadPriority(Env::Priority priority);

  static void PthreadCall(const char* label, int result);

  struct Impl;

private:
  std::unique_ptr<Impl> impl_;
};
