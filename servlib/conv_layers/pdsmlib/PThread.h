
#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "SystemBase.h"
#include "PMutex.h"


// This class encapsulates some of the basic posix thread
// functionality.  A PThread object is first created, then
// when the user is ready to actually start the thread,
// the start method can be invoked.  When the user wishes
// to stop the thread, the stop method can be invoked.  The
// user may supply a standalone "C" like function to be executed
// by the thread.  Alternatively, a class can be derived from
// this class, and the "run" method overridden, which will be
// called when the thread is started.  This derived class
// method of operation is intentionally very similar to the
// manner in which threading is implemented in the Java
// programming language.  The author felt the Java implementation
// is particularly elegant, as well as being simple and easy to
// use, so it is mimicked in this implementation.

class PThread : public SystemBase
{

// *** member variables ***
public:

protected:
  // libc system call thread cancellation protection lock
  //
  // this mutex lock is used to protect certain system calls
  // such as the "gmtime/gmtime_r" functions, which use a
  // mutex lock in the "C" library.  if a thread is cancelled
  // without releasing that lock, subsequent threads which
  // try to access whatever that lock is protecting will be
  // "hung" waiting for the mutex lock in the library, which
  // will never be released.
  //
  // at least that's my working theory, and i'm sticking to
  // it, until i can prove conclusively otherwise  :)
  static PMutex m_libcLock;

  // user "C" thread function address
  void* (*m_userfunc) ( void * );

  // user thread startup argument pointer
  volatile void* m_arg;

  // posix thread data structure pointer
  volatile pthread_t m_thread;

  // posix thread attribute structure
  volatile pthread_attr_t m_attr;

  // thread execution run lock
  PMutex m_pthreadRunLock;

  // thread execution cancel lock
  PMutex m_pthreadCancelLock;

  // structure defining PThread_start friend function argument
  typedef struct PTHREAD_ARG
  {
    PThread* theThis;
    void* theArg;
  } PThread_arg;

  PThread_arg m_parg;

  // stop flag
  volatile bool m_stop;


private:


// *** methods ***
public:
  // these methods provide static wrappers for various time functions
  // which need to be protected from thread cancellation
  static struct tm* gmtime ( const time_t *timep, struct tm* result );
  static struct tm* localtime ( const time_t *timep, struct tm* result );

  // test the validity of a thread id
  //
  // this method returns "true" if the specified
  // thread id is valid, false if it is not
  static bool validate_threadid ( pthread_t tid );

  // constructor to create a PThread object for use with a derived
  // class in which the "run" method has been overridden.  the
  // overridden "run" method will be passed the specified "arg".
  //
  // NOTE: your derived class might look something like the following:
  //
  // class MyPThread : public PThread
  // {
  // public:
  //   MyPThread ( myarg1, myarg2, ..., void* arg, ... ) : PThread(arg)
  //   {
  //     // your constructor code
  //   }
  // 
  //   void* run ( void* arg )
  //   {
  //     // your thread execution code
  //     :
  //     :
  //     return (void *)status;  // return some kind of status
  //                             // or at least a NULL
  //   }
  // };
  PThread ( void* arg = NULL );

  // constructor to create a PThread object for use with a
  // C-like function ("userfunc").  the user C-like function
  // will be passed "arg".
  PThread (
    void* (*userfunc)(void *),  // user function to be called on "start"
    void* arg = NULL            // arg passed to user function
  );

  // destructor
  virtual ~PThread();

  // set user function (C function to be called on "start")
  virtual void setFunction ( void* (*userfunc)(void *) );

  // set arg (to be passed to thread function on "start")
  virtual void setArg ( void* arg );

  // set user attributes.  this method is designed to be overridden
  // in a derived class to allow the user to override the default
  // attributes before the thread is actually created internally.
  //
  // this method allows the user to control additional attributes
  // in a manner similar to the set system/process scope methods.
  virtual void setattributes() { }

  // set system scope for this thread object.  this method must
  // be called before the "start" method is invoked to be effective.
  // this method should be used with great restraint and a very
  // thorough understanding of its possible ramifications.  it is
  // only effective for super user processes.  see the man page
  // "pthread_attr_setscope" for somewhat of an explanation as to
  // the effects this can have.
  virtual void setsystemscope();

  // the opposite of the setsystemscope method (and also the default)
  // this method can be used to reset the thread scope and must be
  // called before the "start" method to be effective.
  virtual void setprocessscope();

  // set stack size
  //
  // this method, like the setsystemscope method above, must
  // be invoked before the "start" method in order to be
  // effective.  the default on the Origin 2000 at the time
  // of this writing is 131072 (128K) bytes per thread.
  void setstacksize ( size_t size );

  // get stack size
  size_t getstacksize();

  // get thread scheduling parameters
  virtual int getschedparam ( int* policy, int* priority );

  // set thread scheduling parameters
  //
  // the policy must be one of the following
  // [ SCHED_OTHER, SCHED_RR, SCHED_FIFO ]
  //
  // the priority value must be in the inclusive range of
  //
  // [ sched_get_priority_min - sched_get_priority_max ]
  //
  // for the specified policy value
  virtual int setschedparam ( int policy, int priority );

  // get runon cpu for this thread
  virtual int get_runon_cpu();

  // set runon cpu for this thread
  virtual int set_runon_cpu ( int processor );

  // start thread execution
  virtual bool start();

  // start thread execution with "arg".  NOTE: existing
  // "arg" within this object is not overwritten.
  virtual bool start ( void* arg );

  // stop thread execution
  virtual void* stop();

  // reset thread parameters
  //
  // use this method if you have a thread that is "hung"
  // and you don't want to use the stop method because
  // your calling thread could get hung as well in the
  // pthread_cancel call
  virtual void reset();

  // thread cleanup method - override this method to implement
  // any class specific thread cleanup logic.  this method is
  // invoked when a thread is terminated.
  virtual void cleanup();

  // override this run method in a derived class.  it will
  // automatically be called by the start method if no other
  // function has been specified by using the "C" type of
  // constructor, or the setFunction method.
  virtual void* run ( void* arg = NULL );

  // access to the posix thread identifier
  virtual pthread_t getPThreadId();

  // test the validity of the thread represented by this object
  //
  // this method returns "true" if this objects
  // thread id is valid, false if it is not
  virtual bool validate_threadid();

  // get/set stop flag
  virtual bool getstop();
  virtual void setstop ( bool flag = true );

protected:
  // a friend function which is actually the target of the posix
  // pthread_create call, that will execute the appropriate user
  // defined function or overridden run method
  friend void* PThread_start ( void* arg );

  // a friend function responsible for handling thread cleanup
  friend void PThread_cleanup ( void* arg );

  // protected stop method
  virtual void* stop ( bool cancel_thread );

  // disallow copy constructor and assignment operator
  PThread ( PThread& ) : SystemBase() { }
  virtual PThread& operator = ( PThread& ) { return *this; }

private:
  // object initialization method
  void init();

};

#endif  // __PTHREAD_H__

