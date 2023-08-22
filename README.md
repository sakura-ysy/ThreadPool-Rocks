## ThreadPool-Rocks

仿造RocksDB实现的多优先级线程池

- 不同优先级的多个 ThreadPool，从低到高依次为 BOTTOM, LOW, HIGH, USER，默认 LOW；
- 系统层面设置 IO 优先级与 CPU 优先级；
- 动态增减 ThreadPool 中的线程个数；
- 抽象 Env 类，通过 Env 直接操作 ThreadPool，移植只需重新实现 Env；

单个 ThreadPool 中，创建任务时一并创建线程，线程数超过阈值后不再新建线程，多余的任务存在任务队列中，当有线程空闲时则从队列中抓取任务执行。

## Compile

- lib & static lib: 

```shell
make all
```

- example: 

```shell
// 在此之前，确保 libthreadpool.a / libthreadpool.so 成功生成
cd example
make all
```

## API

新建实例：

```cpp
Env::Default()
```

设置单个 ThreadPool 中的最大线程数：

```cpp
Env::SetBackgroundThreads(int num, Priority pri)
```

新增任务：

```cpp
Env::Schedule(void (*function)(void* arg1), void* arg1, Priority pri,
                void* tag,
                void (*unschedFunction)(void* arg2), void* arg2)
```

撤销任务：

```cpp
Env::UnSchedule(void* tag, Priority pri)
```

结束所有线程：

```cpp
// wake up all threads and join them so that they can end
Env::JoinAllThreads(Priority pri)
// JoinAllThreads but wait the uncompleted jobs
Env::WaitForJobsAndJoinAllThreads(Priority pri)
```

线程调度的函数栈：

```cpp
Env::Schedule()
  PosixEnv::Schedule()
    ThreadPoolImpl::Schedule()
      ThreadPoolImpl::Impl::Submit()
        ThreadPoolImpl::Impl::StartBGThreads()
          ThreadPoolImpl::Impl::BGThreadWrapper()
            ThreadPoolImpl::Impl::BGThread()
              func(); // job func
```

## Priority

优先级由系统层面完成，而非软件层面。

- IO priority: [Linux man-page: ioprio_set](https://www.man7.org/linux/man-pages/man2/ioprio_set.2.html)

- CPU priority: [Linux man-page: setpriority](https://www.man7.org/linux/man-pages/man2/setpriority.2.html)

```cpp
// One io priority is said to be higher than another one 
// if it belongs to a higher priority class.
int io_priority_class = priority_;

// The cpu prio argument is a value in the range -20 to 19
// with -20 being the highest priority and 19 being the lowest priority
int cpu_priority = 19 - 5*priority_; 
cpu_priority = std::max(cpu_priority, -20);

// io
syscall(SYS_ioprio_set, 1,  // IOPRIO_WHO_PROCESS
        0,                  // current thread
        IOPRIO_PRIO_VALUE(io_priority_class, 0));

// cpu
setpriority(
  PRIO_PROCESS,
  // Current thread.
  0,
  // nice value
  cpu_priority);
```
