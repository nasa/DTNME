
#ifndef __CUTILS_H__
#define __CUTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <shadow.h>

#if defined(__linux)
#  include <endian.h>
#endif


#if defined(__cplusplus)
extern "C" {
#endif

/****************************************************************
 * this file contains C library function prototypes that are 
 * not available on the target platform and must be simulated,
 * or which are very generic and "C"-like in nature.
 ****************************************************************/


/****************************************************************
 * generic functions
 ****************************************************************/


/* perform 8-byte integer byte swapping, on LITTLE_ENDIAN machines,
 * otherwise this function does nothing
 */
void byte_swap8 ( unsigned char* buf, int size_in_bytes );


/* perform 4-byte integer byte swapping, on LITTLE_ENDIAN machines,
 * otherwise this function does nothing
 */
void byte_swap4 ( unsigned char* buf, int size_in_bytes );


/* perform 2-byte integer byte swapping, on LITTLE_ENDIAN machines,
 * otherwise this function does nothing
 */
void byte_swap2 ( unsigned char* buf, int size_in_bytes );


/* "daemonize" a process
 *
 * this means the task will be forced into the "background",
 * i.e. it's parent process will be the system "init" task.
 *
 * also, the controlling tty association will be broken and
 * re-established using the supplied paths.
 *
 *
 * parameter description:
 *
 * stdxxx_path: replace the standard in/out/err with a stream
 * assigned to the user specified device or file.  if the user
 * supplied pointer is NULL, the system null device "/dev/null"
 * will be used instead.  if the stdout and stderr paths are
 * the same, i/o will be coordinated for the two.  this function
 * will attempt to create the entire path and file for both the
 * standard out and standard error paths.
 *
 * backup_flag: if true (not zero), any existing stdout or stderr
 * file will be renamed with a ".bak" extension before the new
 * file is created.
 *
 * a bitwise error code will be returned as follows:
 *
 * 0x00 - no errors detected
 * 0x01 - fork error
 * 0x02 - unable to create stdout file ("/dev/null" used instead)
 * 0x04 - unable to create stderr file ("/dev/null" used instead)
 * 0x08 - unable to open stdin file ("/dev/null" used instead)
 */

int daemonize ( const char* stdin_path, const char* stdout_path,
  const char* stderr_path, int backup_flag );


/****************************************************************
 * platform specific functions
 ****************************************************************/

/* create directory path recursively and chown every new directory */
int mkdirpandchown ( const char* pathname, mode_t mode, const char* newowner );


#if defined(__linux)
/* create directory path recursively */
int mkdirp ( const char* pathname, mode_t mode );

#elif defined(__sgi)

/* these prototypes are defined in stdio.h and stdlib.h, but there
 * is no way to get at them on the sgi due to the bizarre array of
 * compiler flags you have to wade through to get to all the rest
 * of the things we need.  these require the _SGIAPI compiler flag
 * to be defined, but we can't turn that one on with all the rest
 * of them we need.
 */
extern int snprintf(char *, size_t, const char *, ...);
extern int vsnprintf(char *, size_t, const char *, /*va_list*/ char *);
extern long double strtold(const char *nptr, char **endptr);
extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);

/* return string describing signal */
extern char* strsignal ( int signo );

#elif defined(sun)

#endif


#if defined(__cplusplus)
}
#endif

/****************************************************************
 * the following are actually C++ utility functions and therefore
 * fall outside the extern "C" { } declaration
 ****************************************************************/

/* these wrapper classes provide a thread safe version
 * of their system counterparts.  any additional system
 * functions that use this same type of calling sequence
 * can easily be added to this list as needed.
 */

#include "ThreadSafe_SystemFunctions.h"


class threadsafe_gethostbyname : public ThreadSafe_SystemFunctions < struct hostent >
{
  virtual int getxxx_r ( struct hostent* buf, char* additional_space,
    int additional_space_size, struct hostent** result, va_list args )
  {
    // extract parameters necessary for this particular system
    // function call from the variable length parameter list
    char* hostname = va_arg (args, char *);

    // perform actual system function call and return result to caller
    int h_err=0;
    errno = 0;

#if defined(__linux)
    return gethostbyname_r ( hostname, buf, additional_space, additional_space_size, result, &h_err );
#elif defined(sun)
    *result = gethostbyname_r ( hostname, buf, additional_space, additional_space_size, &h_err );
    return ( NULL != *result  ?  0 : errno );
#endif
  }
};


class threadsafe_getpwnam : public ThreadSafe_SystemFunctions < struct passwd >
{
  virtual int getxxx_r ( struct passwd* buf, char* additional_space,
    int additional_space_size, struct passwd** result, va_list args )
  {
    // extract parameters necessary for this particular system
    // function call from the variable length parameter list
    char* username = va_arg (args, char *);
    errno = 0;

    // perform actual system function call and return result to caller
    return getpwnam_r ( username, buf, additional_space, additional_space_size, result );
  }
};


class threadsafe_getspnam : public ThreadSafe_SystemFunctions < struct spwd >
{
  virtual int getxxx_r ( struct spwd* buf, char* additional_space,
    int additional_space_size, struct spwd** result, va_list args )
  {
    // extract parameters necessary for this particular system
    // function call from the variable length parameter list
    char* username = va_arg (args, char *);
    errno = 0;

    // perform actual system function call and return result to caller
#if defined(__linux)
    return getspnam_r ( username, buf, additional_space, additional_space_size, result );
#elif defined(sun)
    *result = getspnam_r ( username, buf, additional_space, additional_space_size );
    return ( NULL != *result  ?  0 : errno );
#endif
  }
};


// this function is a replacement for the standard strerror
// function which is not thread safe.  it utilizes the
// reentrant strerror_r version from the system library,
// but it manages the buffer allocation for the caller
// on a per thread basis.
const char* threadsafe_strerror ( int errnum );


// thread safe inet_ntoa function.
//
// IMPORTANT NOTE:  this function uses a mutex lock to
// provide the thread safe functionality, and is therefore
// ONLY effective if EVERYBODY uses it.  if someone uses
// inet_ntoa directly then all bets are off.
//
// note:  on linux systems you should use the inet_ntop
// function anyway, which is already thread safe, rather
// than this version of inet_ntoa.
void inet_ntoa_r ( struct in_addr in, char* buf, int bufsize );

#endif  /* __CUTILS_H__ */

