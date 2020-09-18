
#ifndef __ThreadSafe_SystemFunctions_h__
#define __ThreadSafe_SystemFunctions_h__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>

#include "namespaces.h"


// This class implements a standard interface to the thread
// safe reentrant system functions such as gethostbyname_r,
// gethostbyaddr_r, getpwnam_r, getpwuid_r, getspnam_r, and
// so on.  All of these functions have the same basic calling
// sequence and structure, and require similar memory management
// characteristics.
//
// You will no doubt have to include your classes specific
// system include file before including this ".h" file so
// your getxxx_r method override won't have a problem
// during compilation.
//
//
// EXAMPLE:
//
// #include <pwd.h>
// #include <sys/types.h>
//
// #include "ThreadSafe_SystemFunctions.h"
//
//
// int main ( int argc, char* argv[] )
// {
//   // create a thread safe getpwnam_r class
//   class threadsafe_getpwnam : public ThreadSafe_SystemFunctions < struct passwd >
//   {
//   public:
//     virtual int getxxx_r ( struct passwd* buf, char* additional_space,
//       int additional_space_size, struct passwd** result, va_list args )
//     {
//       // extract parameters necessary for this particular
//       // system function call from the variable length
//       // parameter list
//       char* username = va_arg (args, char *);
// 
//       // perform actual system function call and
//       // return result to caller
//       return getpwnam_r ( username,
//         buf, additional_space, additional_space_size, result );
//     }
//   };
// 
//   int status = 0;
//   threadsafe_getpwnam pwnam;
//   struct passwd* pswd = pwnam.invoke_function ( &status, "dunkleml" );
// 
//   if (pswd)
//   {
//     printf ("struct passwd pw_name:   %s\n", pswd->pw_name);
//     printf ("struct passwd pw_passwd: %s\n", pswd->pw_passwd);
//     printf ("struct passwd pw_uid:    %d\n", pswd->pw_uid);
//     printf ("struct passwd pw_gid:    %d\n", pswd->pw_gid);
//     printf ("struct passwd pw_gecos:  %s\n", pswd->pw_gecos);
//     printf ("struct passwd pw_dir:    %s\n", pswd->pw_dir);
//     printf ("struct passwd pw_shell:  %s\n", pswd->pw_shell);
//   }
//   else
//   {
//     printf ("pswd returned is null, status=%d\n", status);
//   }
// 
//   return 0;
// 
// }


template < class StructType >
class ThreadSafe_SystemFunctions
{

// *** member variables ***
public:


protected:
  typedef struct threadsafe_struct
  {
    StructType result;
    StructType buf;
    char* additional_space;
    int additional_space_size;
  } threadsafe_struct_t;

  threadsafe_struct_t m_struct;


private:


// *** methods ***
public:
  // constructor
  ThreadSafe_SystemFunctions()
  {
    memset ( (void *)&m_struct, 0, sizeof(m_struct) );
  }


  // destructor
  virtual ~ThreadSafe_SystemFunctions()
  {
    if ( m_struct.additional_space != NULL )
    {
      delete[] m_struct.additional_space;
    }
  }


  // perform thread safe system function call
  //
  // this is the function call you should actually
  // make, with the parameters specific to your
  // particular system function, when you want to
  // use your derived class.  the first parameter
  // will always be the address of an integer
  // status return variable, the rest of the
  // argument list depends on your particular
  // system function call.  those parameters
  // will be used inside your getxxx_r override
  // method (see below).
  virtual StructType* invoke_function ( int* status, ... )
  {
    // perform system call in thread safe manner
    StructType *result;
    *status = 0;

    // allocate an initial buffer, we'll start with
    // a modest 64 bytes of space and grow from there
    // as necessary
    if ( m_struct.additional_space == NULL )
    {
      m_struct.additional_space_size = 64;
      m_struct.additional_space = new char[m_struct.additional_space_size];

    }

    // set up variable length argument list
    va_list args;
    va_start ( args, status );

    // while the additional space buffer is still too small ...
    while ( ERANGE ==
      ( *status = getxxx_r ( &m_struct.buf, m_struct.additional_space,
                             m_struct.additional_space_size, &result, args ) ) )
    {
      // should be impossible for this to be null at this point but ...
      if ( m_struct.additional_space != NULL )
      {
        delete[] m_struct.additional_space;
        m_struct.additional_space = NULL;
      }

      // if one meg isn't enough something is probably seriously wrong ...
      if ( m_struct.additional_space_size >= 1024000 )
      {
        m_struct.additional_space_size = 0;
        break;
      }

      m_struct.additional_space_size *= 2;
      m_struct.additional_space = new char[m_struct.additional_space_size];


      // reset the argument list
      va_end (args);
      va_start ( args, status );
    }

    va_end (args);

    // check for other errors
    if ( *status != 0  ||  result == NULL )
    {
      if ( m_struct.additional_space != NULL )
      {
        delete[] m_struct.additional_space;
      }
      m_struct.additional_space = NULL;
      m_struct.additional_space_size = 0;
      return NULL;
    }

    // because technically the result pointer does not necessarily
    // have to be the same as the input structure pointer, even
    // though in reality it probably is anyway.
    else
    {
      memcpy ( (void *)&m_struct.result, result, sizeof(StructType) );

      // return thread safe copy of system function structure
      //
      // note:  this only works because the system structure
      // is FIRST in the threadsafe_struct structure, but it
      // sure keeps things a lot simpler that way
      return (StructType *)&m_struct;
    }

  }


  // override this method to perform the call
  // to the actual system function in question
  virtual int getxxx_r ( StructType* buf, char* additional_space,
    int additional_space_size, StructType** result, va_list args ) = 0;


protected:


private:

};

#endif  // __ThreadSafe_SystemFunctions_h__

