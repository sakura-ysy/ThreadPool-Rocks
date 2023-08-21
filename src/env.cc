#include "env.h"
#include "env_posix.h"

Env::Env(){
}

Env::~Env(){
}

//
// Default Posix Env
//
Env* Env::Default() {
  // The following function call initializes the singletons of ThreadLocalPtr
  // right before the static default_env.  This guarantees default_env will
  // always being destructed before the ThreadLocalPtr singletons get
  // destructed as C++ guarantees that the destructions of static variables
  // is in the reverse order of their constructions.
  //
  // Since static members are destructed in the reverse order
  // of their construction, having this call here guarantees that
  // the destructor of static ThreadPoolEnv will go first, then the
  // the singletons of ThreadLocalPtr.
  static PosixEnv default_env;
  return &default_env;
}
