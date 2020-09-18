
// This class encapsulates the basic posix thread "key" concepts.
// A posix "key" allows you to store a value that must be unique
// for each thread and not shared like normal stack/heap values.
// This enables threads to create and use "private" data values.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PThreadKey.h"


// initialize member variables
void PThreadKey::init ( void (*cleanup_function) (void *) )
{
  memset (&m_key, 0, sizeof(m_key));
  int status = pthread_key_create (&m_key, cleanup_function);
  if (status != SUCCESS) put_syserrno(status);
}


// constructor
PThreadKey::PThreadKey ( void (*cleanup_function) (void *) )
{
  init ( cleanup_function );
}


// destructor
PThreadKey::~PThreadKey()
{
  pthread_key_delete (m_key);
}


// set key value
bool PThreadKey::set ( const PThreadKey::PThreadKeyDataType newvalue )
{
  int status = pthread_setspecific (m_key, newvalue);
  if (status != SUCCESS) put_syserrno(status);
  return (status == SUCCESS);
}

// get key value
PThreadKey::PThreadKeyDataType PThreadKey::get()
{
  return (PThreadKey::PThreadKeyDataType)pthread_getspecific (m_key);
}

