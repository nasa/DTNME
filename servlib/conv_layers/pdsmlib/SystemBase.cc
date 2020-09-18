
// Implementation of the Unix flavor of the System Base class

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>

// note:  this is the ONE AND ONLY place that __GETTID__ should be defined
#define __GETTID__
#include "SystemBase.h"
#undef __GETTID__

#include "CUtils.h"
#include "namespaces.h"


// constructor
SystemBase::SystemBase()
{
  m_errno = 0;
}


// destructor
SystemBase::~SystemBase()
{
}


// get last system error number
int SystemBase::get_syserrno()
{
  return m_errno;
}


// get last system error descriptive text message
const char* SystemBase::get_syserrstr()
{
  const char* errstr = threadsafe_strerror(m_errno);
  return (errstr ?  errstr : "Invalid errno");
}


// get last system error descriptive text message
const char* SystemBase::get_syserrstr ( int usererrno )
{
  const char* errstr = threadsafe_strerror(usererrno);
  return (errstr ?  errstr : "Invalid errno");
}


// reset system error number
void SystemBase::reset_syserrno()
{
  m_errno = 0;
}


// capture the system error number
void SystemBase::put_syserrno()
{
  m_errno = errno;
}


// capture the system error number
void SystemBase::put_syserrno ( int usererrno )
{
  m_errno = usererrno;
}

