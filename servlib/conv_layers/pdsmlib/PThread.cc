
// Implementation of the PThread posix threads encapsulation class

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <time.h>

#include "PThread.h"

// static variables
PMutex PThread::m_libcLock;


// these methods provide static wrappers for various time functions
// which need to be protected from thread cancellation
struct tm* PThread::gmtime ( const time_t *timep, struct tm* result )
{
  m_libcLock.lock();
  struct tm* tmptr = gmtime_r ( timep, result );
  m_libcLock.unlock();
  return tmptr;
}


struct tm* PThread::localtime ( const time_t *timep, struct tm* result )
{
  m_libcLock.lock();
  struct tm* tmptr = localtime_r ( timep, result );
  m_libcLock.unlock();
  return tmptr;
}


// test the validity of a thread id
//
// this method returns "true" if the specified
// thread id is valid, false if it is not
bool PThread::validate_threadid ( pthread_t tid )
{
  if ( (pthread_t)0 == tid ) return false;
  return ( 0 == pthread_kill ( tid, 0 ) );
}


// a friend function responsible for handling thread cleanup
void PThread_cleanup ( void* arg )
{
  PThread* pobj = (PThread *)arg;

  errno = 0;

  if ( NULL != pobj )
  {
    // release the thread run lock
    pobj->m_pthreadRunLock.unlock();
    pobj->m_pthreadCancelLock.unlock();

    // reset thread id
    pobj->m_thread = 0;

    // invoke user cleanup - do this last just in case the objects
    // cleanup method actually deletes the object, which would
    // make our "pobj" object pointer invalid afterwards.
    pobj->cleanup();
  }

}


// internal function which is actually the target of the posix
// pthread_create call, that will execute the appropriate user
// defined function or overridden run method
void* PThread_start ( void* arg )
{
  errno = 0;

  // enable cancellation requests
  pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  PThread::PThread_arg* parg = (PThread::PThread_arg *)arg;
  PThread* pobj = parg->theThis;

  void* status = NULL;

  // push thread cleanup handler
  pthread_cleanup_push (PThread_cleanup, pobj);

  // it is now safe to release the cancel lock,
  // this thread is now fully operational and
  // prepared to handle a cancel gracefully
  pobj->m_pthreadCancelLock.unlock();

  if (pobj->m_userfunc)
  {
    // invoke user defined C like function
    status = pobj->m_userfunc (parg->theArg);
  }
  else
  {
    // invoke overridden (hopefully) run method of user derived class
    status = pobj->run (parg->theArg);
  }

  // pop and execute thread cleanup handler
  pthread_cleanup_pop(1);

  return status;

}


// initialize all data members
void PThread::init()
{
  errno = 0;

  m_pthreadRunLock.setOwnerOverride();
  m_pthreadCancelLock.setOwnerOverride();

  m_userfunc = NULL;
  m_arg = NULL;

  memset ( (pthread_t *)&m_thread, 0, sizeof(m_thread) );

  pthread_attr_init ((pthread_attr_t *)&m_attr);
  pthread_attr_setdetachstate (
    (pthread_attr_t *)&m_attr, PTHREAD_CREATE_DETACHED );

  m_stop = false;

}


// constructor will cause overriden run method to be invoked with
// the specified argument
PThread::PThread ( void* arg )
{
  init();
  setArg (arg);
}


// constructor for C like user function
PThread::PThread ( void* (*userfunc)(void *), void* arg )
{
  init();
  setFunction (userfunc);
  setArg (arg);
}


// destructor
PThread::~PThread()
{
  // always attempt to stop any running thread
  // (unless of course it is us ... huh?)
  if ( validate_threadid()  &&  (pthread_t)m_thread != pthread_self() )
  {
    this->stop();

    // wait for thread execution to complete.
    // once we can successfully obtain the
    // run lock we'll know this thread has
    // actually been canceled
    m_pthreadRunLock.lock();
    m_pthreadRunLock.unlock();
    m_pthreadCancelLock.unlock();

    // destroy the thread attribute structure
    pthread_attr_destroy ((pthread_attr_t *)&m_attr);
  }

}


// set user function (C function to be called on "start")
void PThread::setFunction ( void* (*userfunc)(void *) )
{
  m_userfunc = userfunc;
}


// set arg (to be passed to C function on "start")
void PThread::setArg ( void* arg )
{
  m_arg = arg;
}


// set system scope for this thread object.  this method must
// be called before the "start" method is invoked to be effective.
// this method should be used with great restraint and a very
// thorough understanding of its possible ramifications.  it is
// only effective for super user processes.  see the man page
// "pthread_attr_setscope" for somewhat of an explanation as to
// the effects this can have.
void PThread::setsystemscope()
{
  errno = 0;
  pthread_attr_setscope (
    (pthread_attr_t *)&m_attr, PTHREAD_SCOPE_SYSTEM );
  put_syserrno();
}

// the opposite of the above method (and also the default) this
// method can be used to reset the thread scope and must be called
// before the "start" method to be effective.
void PThread::setprocessscope()
{
  errno = 0;
  pthread_attr_setscope (
    (pthread_attr_t *)&m_attr, PTHREAD_SCOPE_PROCESS );
  put_syserrno();
}


// set stack size
void PThread::setstacksize ( size_t size )
{
  errno = 0;
  pthread_attr_setstacksize ((pthread_attr_t *)&m_attr, size);
  put_syserrno();
}


// get stack size
size_t PThread::getstacksize()
{
  size_t size;
  pthread_attr_getstacksize ((pthread_attr_t *)&m_attr, &size);
  return size;
}


// get thread scheduling parameters
int PThread::getschedparam ( int* policy, int* priority )
{
  int status = 0;
  int mypolicy = SCHED_OTHER;

  sched_param mypriority;
  memset ( &mypriority, 0, sizeof(mypriority) );

  errno = 0;

  // we must make sure this object represents a valid running thread
  if ( validate_threadid() )
  {
    // we must set the cancel lock to prevent having the rug pulled out
    // from under us if this thread were to be cancelled during our
    // pthread_getschedparam call (ouch!!!)
    if ( m_pthreadCancelLock.lock (false) )
    {
      // recheck to make sure the thread is still valid after obtaining the lock
      if ( validate_threadid() )
      {
        // finally reasonably safe to grab scheduling parameters for this thread
        status = pthread_getschedparam ( m_thread, &mypolicy, &mypriority );
      }
      else
      {
        // this thread object no longer represents a valid running thread
        status = ESRCH;
      }

      // release the cancel lock
      m_pthreadCancelLock.unlock();
    }
    else
    {
      // could not get the lock
      status = m_pthreadCancelLock.get_syserrno();
    }
  }
  else
  {
    // this thread object does not currently represent a valid running thread
    status = ESRCH;
  }

  // save system status
  put_syserrno ( status );

  // return requested values to caller
  if ( policy ) *policy = mypolicy;
  if ( priority ) *priority = mypriority.sched_priority;

  // return final status determination
  return status;

}


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
int PThread::setschedparam ( int policy, int priority )
{
  int status = 0, min = 0, max = 0;

  errno = 0;
  reset_syserrno();

  // make sure priority is in range of specified policy
  switch ( policy )
  {
  case SCHED_RR:
  case SCHED_FIFO:
    min = sched_get_priority_min ( policy );
    max = sched_get_priority_max ( policy );

    if ( priority > max )
      priority = max;
    else if ( priority < min )
      priority = min;

    break;


  default:
    // default is zero for all other scheduling policies
    priority = 0;
    break;
  }

  sched_param mypriority;
  memset ( &mypriority, 0, sizeof(mypriority) );

  // we must make sure this object represents a valid running thread
  if ( validate_threadid() )
  {
    // we must set the cancel lock to prevent having the rug pulled out
    // from under us if this thread were to be cancelled during our
    // pthread_setschedparam call (ouch!!!)
    if ( m_pthreadCancelLock.lock (false) )
    {
      // recheck to make sure the thread is still valid after obtaining the lock
      if ( validate_threadid() )
      {
        // finally, it's reasonably safe to set scheduling parameters for this thread
        mypriority.sched_priority = priority;

        // set the new policy and priority
        status = pthread_setschedparam ( m_thread, policy, &mypriority );
      }
      else
      {
        // this thread object no longer represents a valid running thread
        status = ESRCH;
      }

      // release the cancel lock
      m_pthreadCancelLock.unlock();
    }
    else
    {
      // could not get the lock
      status = m_pthreadCancelLock.get_syserrno();
    }
  }
  else
  {
    // this thread object does not currently represent a valid running thread
    status = ESRCH;

    // just for good measure, make sure both locks are freed up as well
    m_pthreadRunLock.unlock();
    m_pthreadCancelLock.unlock();
  }

  // save system status
  put_syserrno ( status );

  // return final status determination
  return status;

}


// get runon cpu for this thread
int PThread::get_runon_cpu()
{
  int cpu = 0;

  if ( m_pthreadCancelLock.lock (false) )
  {
    if ( validate_threadid() )
    {
#if defined(__sgi)
      pthread_getrunon_np ( &cpu );
#endif
    }

    m_pthreadCancelLock.unlock();
  }
  else
  {
    put_syserrno ( m_pthreadCancelLock.get_syserrno() );
  }

  return cpu;

}


// set runon cpu for this thread
int PThread::set_runon_cpu ( int processor )
{
  (void) processor;
  int status = 0;

  if ( m_pthreadCancelLock.lock (false) )
  {
    if ( validate_threadid() )
    {
#if defined(__sgi)
      status = pthread_setrunon_np ( processor );
#endif
    }

    m_pthreadCancelLock.unlock();
  }
  else
  {
    put_syserrno ( m_pthreadCancelLock.get_syserrno() );
    status = get_syserrno();
  }

  return status;

}


// start thread if it is not already running
bool PThread::start()
{
  errno = 0;
  reset_syserrno();

  // set the cancel lock, i.e. it is not safe to
  // try and cancel this thread at this time
  if (! m_pthreadCancelLock.lock (false))
  {
    put_syserrno (m_pthreadCancelLock.get_syserrno());
    return false;
  }

  // obtain the run lock if possible.  if not,
  // then the thread is already active and
  // should not be started again.
  if (! m_pthreadRunLock.lock (false))
  {
    put_syserrno (m_pthreadRunLock.get_syserrno());
    m_pthreadCancelLock.unlock();
    return false;
  }

  // if this object still somehow represents a
  // valid thread id, then don't destroy it
  if ( validate_threadid() )
  {
    put_syserrno ( EEXIST );
    m_pthreadCancelLock.unlock();
    return false;
  }

  // reset the stop flag
  m_stop = false;

  m_parg.theThis = this;
  m_parg.theArg = (void *)m_arg;
  setattributes();

  errno = 0;
  reset_syserrno();

  int status = 0;

  status = pthread_create ( (pthread_t *)&m_thread, (pthread_attr_t *)&m_attr, PThread_start, &m_parg );

  if (status != SUCCESS)
  {
    put_syserrno(status);
    m_thread = 0;
    m_pthreadRunLock.unlock();
    m_pthreadCancelLock.unlock();
    return false;
  }

  // wait for the new thread to release the cancel
  // lock so we can be sure it is running
  m_pthreadCancelLock.lock();
  m_pthreadCancelLock.unlock();

  return true;

}


// start thread with "arg"
bool PThread::start ( void* arg )
{
  errno = 0;
  reset_syserrno();

  // set the cancel lock, i.e. it is not safe to
  // try and cancel this thread at this time
  if (! m_pthreadCancelLock.lock (false))
  {
    put_syserrno (m_pthreadCancelLock.get_syserrno());
    return false;
  }

  // obtain the run lock if possible.  if not,
  // then the thread is already active and
  // should not be started again.
  if (! m_pthreadRunLock.lock (false))
  {
    put_syserrno (m_pthreadRunLock.get_syserrno());
    m_pthreadCancelLock.unlock();
    return false;
  }

  // if this object still somehow represents a
  // valid thread id, then don't destroy it
  if ( validate_threadid() )
  {
    put_syserrno ( EEXIST );
    m_pthreadCancelLock.unlock();
    return false;
  }

  // reset the stop flag
  m_stop = false;

  m_parg.theThis = this;
  m_parg.theArg = arg;
  setattributes();

  errno = 0;
  reset_syserrno();

  int status = 0;

  status = pthread_create ( (pthread_t *)&m_thread, (pthread_attr_t *)&m_attr, PThread_start, &m_parg );

  if (status != SUCCESS)
  {
    put_syserrno(status);
    m_thread = 0;
    m_pthreadRunLock.unlock();
    m_pthreadCancelLock.unlock();
    return false;
  }

  // wait for the new thread to release the cancel
  // lock so we can be sure it is running
  m_pthreadCancelLock.lock();
  m_pthreadCancelLock.unlock();

  return true;

}


// reset thread parameters
//
// use this method if you have a thread that is "hung"
// and you don't want to use the stop method because
// your calling thread could get hung as well in the
// pthread_cancel call
void PThread::reset()
{
  stop ( false );
}


// stop thread if it is running
void* PThread::stop()
{
  return stop ( true );
}


// protected stop method
void* PThread::stop ( bool cancel_thread )
{
  errno = 0;
  reset_syserrno();

  // set the stop flag
  m_stop = true;

  // if we can obtain the run lock, then the
  // thread is not running, so do not attempt
  // to cancel it, as this action sometimes
  // generates strange problems when the thread
  // is not active.  and besides, if the thread
  // is not running, it is not necessary anyway.
  if (m_pthreadRunLock.lock (false))
  {
    m_pthreadRunLock.unlock();
    m_pthreadCancelLock.unlock();
    return (void *)SUCCESS;
  }

  // force the thread to stop execution
  int status = SUCCESS;

  if ( validate_threadid() )
  {
    // set the cancel lock, i.e. it is not safe for
    // anyone else to try and cancel this thread
    // at this time
    m_pthreadCancelLock.lock();

    // we must check the thread id again now that
    // we have the cancel lock just in case someone
    // else already performed this cancel
    if ( validate_threadid() )
    {
      // okey, it is finally safe to cancel
      // this thread and reset the thread id
      if ( cancel_thread  &&  (pthread_t)m_thread != pthread_self() )
      {
        // we must also make sure this thread is not trying
        // to access any system time functions as well.  we
        // don't want a thread cancelled in the middle of a
        // "gmtime" function call for example.  this can
        // result in a "C" library mutex lock being left
        // in a "lock"ed state, and then every thread that
        // trys to call any functions using that same lock
        // will get hung out to dry ...
        m_libcLock.lock();

        // note:  the m_pthreadRunLock will be released
        // by the running thread when it finishes
        if ( validate_threadid() )
        {
          status = pthread_cancel (m_thread);
        }

        m_libcLock.unlock();
      }
      else
      {
        // since we cannot really cancel the thread without
        // risking hanging this thread, the cancelling thread,
        // we must reset the run lock here
        m_pthreadRunLock.unlock();
      }

      m_thread = 0;
    }

    // release the cancel lock
    m_pthreadCancelLock.unlock();
  }
  else
  {
    // the thread ID is invalid, so just reset the run lock since
    // we know it is already locked, and we don't want to leave
    // it that way.
    //
    // now just exactly how did this happen when the thread ID is
    // already zero anyways (you might ask)???  well, the only way
    // i can conceive of this happening goes something like this:
    //
    // 1) the thread has been "stop"ped, i.e. we're in this "stop" logic
    // 2) PThread_cleanup is invoked by PThread_start via pthread_cleanup_pop
    //    due to the pthread_cancel call above.
    // 3) PThread_cleanup first resets the thread ID to zero (just in case the
    //    thread objects cleanup method might invalidate the pobj pointer by
    //    invoking "delete this;", so cleanup is the last thing done with pobj)
    // 4) PThread_cleanup then executes the thread objects cleanup method
    // 5) the thread objects cleanup method logic DOES invoke "delete this;"
    // 6) the PThread base class destructor calls this->stop() because
    //    m_thread != pthread_self() since m_thread is now zero (see (3)).
    //    but the destructor wasn't also checking to see if m_thread was
    //    not equal pthread_self on account of it was already set to zero
    //    before calling this stop method again (now it does of course).
    //
    // and there you have it, m_pthreadRunLock was not released here and
    // you wound up hung out to dry.  unlikely? perhaps, impossible?, no...
    //
    // and since we already found one way for that to happen, we're gonna
    // release the lock here just to make sure it doesn't happen again,
    // due to some other bizarre and unforeseen circumstances...
    m_pthreadRunLock.unlock();
    m_pthreadCancelLock.unlock();
  }

  // check for errors
  if (status != SUCCESS)
  {
    put_syserrno(status);
  }

  return (void *)( (long)status );

}


// thread cleanup method - override this method to implement
// any class specific thread cleanup logic.  this method is
// invoked when a thread is terminated.
//
// Note: this base class method actually does nothing, it is
// simply a placeholder to allow the user to add thread cleanup
// logic when needed.
void PThread::cleanup()
{
}


// override this run method in a derived class.  it will
// automatically be called by the start method if no other
// function has been specified by using the "C" type of
// constructor, or the setFunction method.
void* PThread::run ( void* arg )
{
  (void) arg;
  fprintf ( stderr, "PThread.run(%ld) : base class method invoked - override intended?\n", (long)pthread_self() );
  fflush (stderr);
  return 0;
}


// access to the posix thread identifier
pthread_t PThread::getPThreadId()
{
  return (pthread_t)m_thread;
}


// test the validity of the thread represented by this object
//
// this method returns "true" if this objects
// thread id is valid, false if it is not
bool PThread::validate_threadid()
{
  return PThread::validate_threadid ( (pthread_t)m_thread );
}


// get/set stop flag
bool PThread::getstop()
{
  return m_stop;
}

void PThread::setstop ( bool flag )
{
  m_stop = flag;
}

