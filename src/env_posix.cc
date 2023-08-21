#include "env_posix.h"

PosixEnv::PosixEnv() 
      : thread_pools_(Priority::TOTAL) {
  pthread_mutex_init(&mu_, nullptr);
  for (int i = 0; i < (int)thread_pools_.size(); i++)
  {
    thread_pools_[i].SetHostEnv(this);
    thread_pools_[i].SetThreadPriority(static_cast<Env::Priority>(i));
  } 
}

PosixEnv::~PosixEnv(){
}

void PosixEnv::Schedule(void (*function)(void* arg1), void* arg1, Priority pri,
                void* tag,
                void (*unschedFunction)(void* arg2), void* arg2) {
  assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
  thread_pools_[pri].Schedule(function, arg1, tag, unschedFunction, arg2);
}

int PosixEnv::UnSchedule(void* tag, Priority pri) {
  assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
  return thread_pools_[pri].UnSchedule(tag);
}

void PosixEnv::JoinAllThreads(Priority pri) {
  thread_pools_[pri].JoinAllThreads();
}

void PosixEnv::WaitForJobsAndJoinAllThreads(Priority pri) {
  thread_pools_[pri].WaitForJobsAndJoinAllThreads();
}

unsigned int PosixEnv::GetThreadPoolQueueLen(Priority pri) const {
  assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
  return thread_pools_[pri].GetQueueLen();
}

void PosixEnv::SetBackgroundThreads(int num, Priority pri)  {
  assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
  thread_pools_[pri].SetBackgroundThreads(num);
}

int PosixEnv::GetBackgroundThreads(Priority pri) {
  assert(pri >= Priority::BOTTOM && pri <= Priority::HIGH);
  return thread_pools_[pri].GetBackgroundThreads();
}
