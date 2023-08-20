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

class PosixEnv : public Env {
public:
  PosixEnv(/* args */);
  ~PosixEnv();

private:
  friend Env* Env::Default();

  std::vector<ThreadPoolImpl>& thread_pools_;
  pthread_mutex_t& mu_;
  std::vector<pthread_t>& threads_to_join_;
};
