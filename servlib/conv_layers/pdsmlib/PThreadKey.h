
#ifndef __PTHREADKEY_H__
#define __PTHREADKEY_H__

#include <pthread.h>

#include "SystemBase.h"


// This class encapsulates the basic posix thread "key" concepts.
// A posix thread "key" allows you to store a value that must be
// unique for each thread and not shared like normal stack/heap data.

class PThreadKey : public SystemBase
{

// *** member variables ***
public:
  typedef void* PThreadKeyDataType;

protected:
  // posix thread key data structure
  pthread_key_t m_key;

private:


// *** methods ***
public:
  // constructor
  PThreadKey ( void (*cleanup_function) (void *) = NULL );

  // destructor
  virtual ~PThreadKey();

  // set key value
  virtual bool set ( const PThreadKey::PThreadKeyDataType );

  // get key value, returns the key value (which is a pointer)
  // if successful, or NULL if not
  virtual PThreadKeyDataType get();

protected:
  // disallow copy constructor and assignment operator
  PThreadKey ( PThreadKey& ) : SystemBase() { }
  virtual PThreadKey& operator = ( PThreadKey& ) { return *this; }

private:
  // object initialization method
  void init ( void (*cleanup_function) (void *) );

};

#endif  // __PTHREADKEY_H__

