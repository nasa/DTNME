
// Implementation of the posix threads mutex encapsulation class
//
// This class encapsulates some of the basic posix thread
// mutex functionality.  A mutex should be used when some
// shared resource requires that one and only one thread
// manipulate that resource in any way at any given time,
// i.e. a mutex lock is always an exclusive lock.

#include <stdio.h>
#include <stdlib.h>

#include "PMutex.h"

// static variable initialization
static const pthread_t NoOwner = (pthread_t)-1;


// initialize all data members
void PMutex::init()
{
  m_lockowner = NoOwner;
  m_override = false;
  m_useraddress = false;
  m_destroyOnExit = false;

  pthread_mutexattr_init ((pthread_mutexattr_t *)&m_attr);

}


// default constructor (actual mutex is private local memory)
PMutex::PMutex()
{
  init();
  m_mutex = &m_privateMutex;

#if !defined(__linux)
  pthread_mutexattr_setpshared (
    (pthread_mutexattr_t *)&m_attr, PTHREAD_PROCESS_PRIVATE );
#endif

  setattributes();
  pthread_mutex_init (
    (pthread_mutex_t *)m_mutex, (pthread_mutexattr_t *)&m_attr );

}


// constructor to allow user to specify mutex address.  this
// constructor should be used if you wish to share the mutex
// between processes, in which case the mutex can be allocated
// in shared memory and its address can be passed into here.
// the "create" parameter should only be set to true by the
// first process to attach to the mutex, other processes
// should specify false for this parameter.
PMutex::PMutex ( void* mutex, bool create )
{
  init();
  m_mutex = (pthread_mutex_t *)mutex;

#if !defined(__linux)
  pthread_mutexattr_setpshared (
    (pthread_mutexattr_t *)&m_attr, PTHREAD_PROCESS_SHARED );
#endif

  m_useraddress = true;
  if (create)
  {
    setattributes();
    pthread_mutex_init (
      (pthread_mutex_t *)m_mutex, (pthread_mutexattr_t *)&m_attr );
    m_destroyOnExit = true;
  }
}


// destructor
PMutex::~PMutex()
{
  if (! m_useraddress  ||  (m_useraddress  &&  m_destroyOnExit) )
  {
    pthread_mutex_destroy ((pthread_mutex_t *)m_mutex);
  }
}


// set "destroy on exit" flag for shared mutexs
void PMutex::setDestroyOnExit ( bool destroy_on_exit )
{
  m_destroyOnExit = destroy_on_exit;
}


// obtain lock, method returns true if successful, false if not.
bool PMutex::lock ( bool waitforlock )
{
  int status = 0;
  pthread_t my_threadid = pthread_self();

  errno = 0;
  reset_syserrno();

  // if we are not overriding the ownership checks, and the calling
  // thread does indeed already own this lock, then a dead lock
  // would result, so return that status to the calling thread
  if (! m_override  &&  m_lockowner == my_threadid)
  {
    status = EDEADLK;
  }
  // either we are overriding the ownership checks, or the calling
  // thread does not already own the mutex, so obtain the mutex
  // lock or block trying, unless this is a non-blocking call,
  // then just try to obtain the lock without blocking
  else
  {
    if (waitforlock)
      status = pthread_mutex_lock ((pthread_mutex_t *)m_mutex);
    else
      status = pthread_mutex_trylock ((pthread_mutex_t *)m_mutex);
  }

  if (status == SUCCESS)
  {
    m_lockowner = my_threadid;
    return true;
  }
  else
  {
    put_syserrno(status);
    return false;
  }

}


// release lock, method returns true if successful, false if not.
bool PMutex::unlock()
{
  int status = 0;
  pthread_t my_threadid = pthread_self();

  errno = 0;
  reset_syserrno();

  // if we are checking for lock ownership by the calling thread,
  // and that thread does indeed own the lock, then reset the lock
  // owner and perform the mutex unlock
  if (! m_override  &&  m_lockowner == my_threadid)
  {
    m_lockowner = NoOwner;
    status = pthread_mutex_unlock ((pthread_mutex_t *)m_mutex);
  }
  // if we are overriding the lock ownership check, then simply
  // reset the lock owner and perform the unlock no matter what
  else if (m_override)
  {
    m_lockowner = NoOwner;
    status = pthread_mutex_unlock ((pthread_mutex_t *)m_mutex);
  }
  // otherwise, the current thread does not own the mutex, or the
  // mutex is not locked, and we are not overriding those checks.
  // so tell the calling thread that it does not have permission
  // to perform this unlock.
  else
  {
    status = EPERM;
  }

  if (status == SUCCESS)
  {
    return true;
  }
  else
  {
    put_syserrno(status);
    return false;
  }

}


// set/reset the mutex lock ownership override.
void PMutex::setOwnerOverride ( bool override )
{
  m_override = override;
}


// get the mutex lock ownership override setting
bool PMutex::getOwnerOverride()
{
  return m_override;
}

