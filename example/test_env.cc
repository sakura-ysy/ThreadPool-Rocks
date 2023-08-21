#include <iostream>
#include <unistd.h>
#include <atomic>

#include "env.h"
#include "threadpool.h"
#include "threadpool_impl.h"

#define JOB_NUM 5

using namespace std;

struct ThreadArgs {
  int id_;

  ThreadArgs(int id)
    : id_(id){};
};

void JobFunc(void* arg){
  int count = 10;
  ThreadArgs* targs = static_cast<ThreadArgs*>(arg); 
  while(count --){
    cout << targs->id_ << " is running" << endl;
  }
  cout << targs->id_ << " finish" << endl;
}



int main(){
  Env* env = Env::Default();
  env->SetBackgroundThreads(2, Env::LOW);
  env->SetBackgroundThreads(2, Env::HIGH);
  int job_count = 0;
  while(job_count < JOB_NUM){
    ThreadArgs* targs1 = new ThreadArgs(job_count);
    ThreadArgs* targs2 = new ThreadArgs(JOB_NUM + job_count);
    env->Schedule(&JobFunc, targs1, Env::LOW, nullptr, nullptr, nullptr);
    env->Schedule(&JobFunc, targs2, Env::HIGH, nullptr, nullptr, nullptr);
    job_count ++;
  }
  sleep(100);
  return 0;
}