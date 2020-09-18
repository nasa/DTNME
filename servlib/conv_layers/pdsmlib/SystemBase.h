
// interface to linux system level gettid function
#if defined(__linux)
#ifdef NOTHING
# if defined(__GETTID__)

#   include <linux/unistd.h>
#   include <sys/syscall.h>

    extern "C"
    {
      // this actually creates and defines the gettid function,
      // which obviously should be done in one and only one place
      // _syscall0(pid_t,gettid)
      pid_t gettid() { return syscall(SYS_gettid); }
    };

# endif // if defined(__GETTID__)

#endif // if defined(__linux)
#endif

#ifndef __SYSTEMBASE_H__
#define __SYSTEMBASE_H__

//#if !defined(_REENTRANT)
//#  define _REENTRANT
//#endif 

#include <errno.h>

// prototype for the linux system level gettid function
#if defined(__linux)
  extern "C"
  {
    // this is just the "C" prototype for the gettid function
    pid_t gettid(void);
  };
#endif


//#if defined(_REENTRANT)
//#  undef _REENTRANT
//#endif 

// global PDSM utility library last update label
#define PDSMUtilLastUpdate "10/12/2006"


// This class provides some base class functionality for
// operating system dependent classes.  It provides a
// common method for storing and accessing operating
// system specific error information for classes which
// invoke OS specific functions.

class SystemBase
{

// *** member variables ***
public:
  enum SystemBase_Constants
  {
    // generic error status values
    SUCCESS = 0,
    FAILURE = -1,

    // for derived classes which need to provide additional error 
    // codes, this value indicates the last SystemBase error that
    // is in use.
    //
    // DerivedClass_ErrorCode1 = LAST_SYSTEMBASE_ERROR_CODE - 1
    // DerivedClass_ErrorCode2 = LAST_SYSTEMBASE_ERROR_CODE - 2
    // etc.
    LAST_SYSTEMBASE_ERROR_CODE = -1
  };

  // get PDSMUtil library last update label
  static const char* getlastupdate()
  {
    return PDSMUtilLastUpdate;
  }

protected:
  // last system error number encountered
  volatile int m_errno;

private:


// *** methods ***
public:
  // constructor
  SystemBase();

  // destructor
  virtual ~SystemBase();

  // get last system error number
  virtual int get_syserrno();

  // get last system error descriptive text string
  virtual const char* get_syserrstr();

  // get specified system error descriptive text string
  virtual const char* get_syserrstr ( int usererrno );

  // reset system error number
  virtual void reset_syserrno();

protected:
  // store system error number, for use by derived classes.
  virtual void put_syserrno();
  virtual void put_syserrno ( int usererrno );

private:

};

#endif  // __SYSTEMBASE_H__


