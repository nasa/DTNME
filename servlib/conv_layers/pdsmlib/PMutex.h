
#ifndef __PMUTEX_H__
#define __PMUTEX_H__

#include <pthread.h>

#include "SystemBase.h"


// This class encapsulates some of the basic posix thread
// mutex functionality.  A mutex should be used when some
// shared resource requires that one and only one thread
// manipulate that resource in any way at any given time,
// i.e. a mutex lock is always an exclusive access lock.

class PMutex : public SystemBase
{

// *** member variables ***
public:
  // size of memory area required to create an
  // interprocess mutex lock (in shared memory)
  enum PMutex_Constants
  {
    PMUTEX_SIZE = sizeof(pthread_mutex_t)
  };


protected:
  // posix mutex lock data structure pointer
  volatile pthread_mutex_t* m_mutex;

  // local posix mutex lock data structure
  volatile pthread_mutex_t m_privateMutex;

  // posix mutex lock attribute structure
  volatile pthread_mutexattr_t m_attr;

  // thread id of lock owner
  volatile pthread_t m_lockowner;

  // flag indicating whether PTHREAD_MUTEX_ERRORCHECK type checks
  // are performed (the default) or whether they are being overridden
  // by the user.
  bool m_override;

  // flag indicating the user has specified an interprocess
  // mutex lock address (in shared memory)
  bool m_useraddress;

  // flag indicating an interprocess mutex lock should be
  // destroyed when this object is destroyed
  bool m_destroyOnExit;


private:


// *** methods ***
public:
  // constructor
  PMutex();

  // constructor to allow user to specify mutex address.  this
  // constructor should be used if you wish to share the mutex
  // between processes, in which case the mutex can be allocated
  // in shared memory and its address can be passed into here.
  // the mutex address should point to an area in memory at least
  // PMUTEX_SIZE bytes in length.  the create flag indicates
  // that this process is the one that will create the mutex.
  // one and only one process should create an inter-process
  // mutex, the rest should only use it, hence the default value
  // of "false" for this parameter.
  PMutex ( void* mutex, bool create = false );

  // destructor
  virtual ~PMutex();

  // set user attributes.  this method is designed to be overridden
  // in a derived class to allow the user to override the default
  // attributes before the mutex is actually created internally.
  virtual void setattributes() { }

  // set flag to indicate that the mutex should be destroyed
  // when the destructor is called.  this flag is only applicable
  // to mutexs shared between processes.  mutexs just shared
  // between the threads of a single process are automatically
  // destroyed when the destructor is called.  the "create" flag
  // in the shared constructor serves as the initial value for
  // this flag so that the creator is automatically flagged as
  // the destroyer for the shared mutex.  this behavior can
  // be overridden by the use of this method.
  virtual void setDestroyOnExit ( bool destroy_on_exit );

  // obtain lock, method returns true if successful, false if not.
  virtual bool lock ( bool waitforlock = true );

  // release lock, method returns true if successful, false if not.
  virtual bool unlock();

  // set/reset the mutex lock ownership override.
  //
  // the default behavior of the PMutex class is similar to the
  // behavior associated with the PTHREAD_MUTEX_ERRORCHECK type
  // supported on some systems.  if the owning thread attempts
  // to relock the mutex, it will get the error EDEADLK, and
  // not block on itself.  also, if another thread attempts to
  // perform an unlock on a lock it does not own, it will get
  // the error EPERM.
  //
  // if "owner override" is set to true, it provides the ability
  // for a thread to perform an unlock on a mutex lock for which
  // it is not the owner.  it also provides the ability for a
  // thread that is already the owner of a mutex lock to block
  // on a second lock call for that mutex, presumably until
  // such time as a non-owner thread unlocks the mutex.
  virtual void setOwnerOverride ( bool override = true );

  // get the mutex lock ownership override setting
  virtual bool getOwnerOverride();


protected:
  // disallow copy constructor and assignment operator
  PMutex ( PMutex& ) : SystemBase() { }
  virtual PMutex& operator = ( PMutex& ) { return *this; }


private:
  // object initialization method
  void init();

};

#endif  // __PMUTEX_H__

