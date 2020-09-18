
#include <pthread.h>

#include "CUtils.h"


#if defined(__cplusplus)
extern "C" {
#endif

/****************************************************************
 * this file contains C library function source code of functions
 * not available on the target platform which must be simulated,
 * or which are very generic and "C"-like in nature.
 ****************************************************************/


/****************************************************************
 * generic functions
 ****************************************************************/


/* byte swapping functions
 *
 * perform 8-byte integer byte swapping, on LITTLE_ENDIAN machines,
 * otherwise this function does nothing
 */
void byte_swap8 ( unsigned char* buf, int size_in_bytes )
{
#if BYTE_ORDER == LITTLE_ENDIAN
  int nwords = size_in_bytes / 8;

  unsigned char *p1, *p2, tmp;
  unsigned char *ptr = buf;

  /* swap 8-byte words */
  for (int word=0; word < nwords; ++word)
  {
    p1 = ptr + 0;
    p2 = ptr + 7;
    tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;

    p1 = ptr + 1;
    p2 = ptr + 6;
    tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;

    p1 = ptr + 2;
    p2 = ptr + 5;
    tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;

    p1 = ptr + 3;
    p2 = ptr + 4;
    tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;

    ptr += 8;
  }

#endif
}


/* perform 4-byte integer byte swapping, on LITTLE_ENDIAN machines,
 * otherwise this function does nothing
 */
void byte_swap4 ( unsigned char* buf, int size_in_bytes )
{
#if BYTE_ORDER == LITTLE_ENDIAN
  int nwords = size_in_bytes / 4;

  unsigned char *p1, *p2, tmp;
  unsigned char *ptr = buf;

  /* swap 4-byte words */
  for (int word=0; word < nwords; ++word)
  {
    p1 = ptr + 0;
    p2 = ptr + 3;
    tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;

    p1 = ptr + 1;
    p2 = ptr + 2;
    tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;

    ptr += 4;
  }

#endif
}


/* perform 2-byte integer byte swapping, on LITTLE_ENDIAN machines,
 * otherwise this function does nothing
 */
void byte_swap2 ( unsigned char* buf, int size_in_bytes )
{
#if BYTE_ORDER == LITTLE_ENDIAN
  int nwords = size_in_bytes / 2;

  unsigned char tmp;
  unsigned char *ptr = buf;

  /* swap 2-byte words */
  for (int words=0; words < nwords; ++words)
  {
    tmp = *ptr;
    *ptr = *(ptr+1);
    *(ptr+1) = tmp;

    ptr += 2;
  }

#endif
}


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
  const char* stderr_path, int backup_flag )
{
  int status = 0;
  int return_status = 0;
  const char* devnull = "/dev/null";

  /* put ourself into the "background" if necessary */
  if (getppid() != 1)
  {
    pid_t pid = fork();
    if (pid == -1) return_status |= 0x01;

    /* the parent process exits, therefore we (the child process)
     * are now owned by the system "init" task, which effectively
     * means we are "running in the background"
     */
    if (pid > 0) exit(0);
  }

  /* are the standard out and standard error the same? */
  int out_eq_err = 0;
  if ( stdout_path  &&  stderr_path  &&
    strcmp (stdout_path, stderr_path) == 0 ) out_eq_err = 1;

  char path[MAXPATHLEN];
  char backup[MAXPATHLEN];

  /* if the user specified a standard out file then create its
   * path, and rename any existing file with a ".bak" extension.
   * otherwise, just open the null device, i.e. the proverbial
   * "bit bucket"
   */
  int open_flags = O_RDWR | O_CREAT | O_TRUNC;
  int outfd = -1;

  if (stdout_path)
  {
    /* create the standard out directory path */
    strcpy (path, stdout_path);
    char* lastslash = strrchr (path, '/');

    if (lastslash  &&  lastslash != path)
    {
      *lastslash = '\0';
      mkdirp (path, 0777);
      chmod (path, 0777);
    }

    /* rename any existing standard out file */
    if (backup_flag)
    {
      strcpy (backup, stdout_path);
      strcat (backup, ".bak");
      rename (stdout_path, backup);
    }

    /* create the new standard out file */
    do
    {
      pthread_testcancel();

      errno = 0;
      outfd = open (stdout_path, open_flags, 0666);
    } while ( outfd == -1  &&  errno == EINTR );

    /* ouch, we're not gonna be able to capture any
     * output, but we can still detach from the tty
     */
    if (outfd == -1)
    {
      return_status |= 0x02;

      do
      {
        pthread_testcancel();

        errno = 0;
        outfd = open (devnull, O_RDWR);
      } while ( outfd == -1  &&  errno == EINTR );
    }
  }
  else
  {
    do
    {
      pthread_testcancel();

      errno = 0;
      outfd = open (devnull, O_RDWR);
    } while ( outfd == -1  &&  errno == EINTR );
  }

  /* if the user specified a standard error file, and it is not
   * the same file as was specified for stdout,  then do the same
   * processing for this file.
   */
  int errfd = -1;

  if ( stderr_path  &&  ! out_eq_err )
  {
    /* create the standard error directory path */
    strcpy (path, stderr_path);
    char* lastslash = strrchr (path, '/');

    if (lastslash  &&  lastslash != path)
    {
      *lastslash = '\0';
      mkdirp (path, 0777);
      chmod (path, 0777);
    }

    /* rename any existing standard error file */
    if (backup_flag)
    {
      strcpy (backup, stderr_path);
      strcat (backup, ".bak");
      rename (stderr_path, backup);
    }

    /* create the new standard error file */
    do
    {
      pthread_testcancel();

      errno = 0;
      errfd = open (stderr_path, open_flags, 0666);
    } while ( errfd == -1  &&  errno == EINTR );

    /* ouch, we're not gonna be able to capture any
     * output, but we can still detach from the tty
     */
    if (errfd == -1)
    {
      return_status |= 0x04;

      do
      {
        pthread_testcancel();

        errno = 0;
        errfd = open (devnull, O_RDWR);
      } while ( errfd == -1  &&  errno == EINTR );
    }
  }
  else if ( stderr_path  &&  out_eq_err )
  {
    errfd = outfd;
  }
  else
  {
    do
    {
      pthread_testcancel();

      errno = 0;
      errfd = open (devnull, O_RDWR);
    } while ( errfd == -1  &&  errno == EINTR );
  }

  /* create the new standard in */
  int infd = -1;

  if (stdin_path)
  {
    do
    {
      pthread_testcancel();

      errno = 0;
      infd = open (stdin_path, O_RDONLY);
    } while ( infd == -1  &&  errno == EINTR );

    if (infd == -1)
    {
      return_status |= 0x08;

      do
      {
        pthread_testcancel();

        errno = 0;
        infd = open (devnull, O_RDONLY);
      } while ( infd == -1  &&  errno == EINTR );
    }
  }
  else
  {
    do
    {
      pthread_testcancel();

      errno = 0;
      infd = open (devnull, O_RDONLY);
    } while ( infd == -1  &&  errno == EINTR );
  }

  /* close the old connections to the controlling tty */
  fclose (stdin);
  fclose (stdout);
  fclose (stderr);

  /* dupe the in/out file descriptors with the
   * correct standard in/out/err file descriptors
   */
  int stdin_fd = -1;
  int stdout_fd = -1;
  int stderr_fd = -1;

  do
  {
    pthread_testcancel();

    errno = 0;
    stdin_fd = dup2 (infd, STDIN_FILENO);
  } while ( stdin_fd == -1  &&  errno == EINTR );

  do
  {
    pthread_testcancel();

    errno = 0;
    stdout_fd = dup2 (outfd, STDOUT_FILENO);
  } while ( stdout_fd == -1  &&  errno == EINTR );

  do
  {
    pthread_testcancel();

    errno = 0;
    stderr_fd = dup2 (errfd, STDERR_FILENO);
  } while ( stderr_fd == -1  &&  errno == EINTR );

  /* make sure the "close-on-exec" flag is off */
  long fcntl_arg = 0;  /* FD_CLOEXEC is off */

  do
  {
    pthread_testcancel();

    errno = 0;
    status = fcntl (stdin_fd, F_SETFD, fcntl_arg);
  } while ( status == -1  &&  errno == EINTR );

  do
  {
    pthread_testcancel();

    errno = 0;
    status = fcntl (stdout_fd, F_SETFD, fcntl_arg);
  } while ( status == -1  &&  errno == EINTR );

  do
  {
    pthread_testcancel();

    errno = 0;
    status = fcntl (stderr_fd, F_SETFD, fcntl_arg);
  } while ( status == -1  &&  errno == EINTR );

  /* reassign stdin, stdout, and stderr */
  FILE* thestdin = NULL;
  FILE* thestdout = NULL;
  FILE* thestderr = NULL;

  do
  {
    pthread_testcancel();

    errno = 0;
    thestdin = fdopen (stdin_fd, "r");
  } while ( thestdin == NULL  &&  errno == EINTR );

  do
  {
    pthread_testcancel();

    errno = 0;
    thestdout = fdopen (stdout_fd, "a");
  } while ( thestdout == NULL  &&  errno == EINTR );

  do
  {
    pthread_testcancel();

    errno = 0;
    thestderr = fdopen (stderr_fd, "a");
  } while ( thestderr == NULL  &&  errno == EINTR );

#if defined(__linux)
  stdin = thestdin;
  stdout = thestdout;
  stderr = thestderr;
#elif defined(sun)  ||  defined(__sgi)
  memcpy ( stdin, thestdin, sizeof(FILE) );
  memcpy ( stdout, thestdout, sizeof(FILE) );
  memcpy ( stderr, thestderr, sizeof(FILE) );
#endif

  return return_status;

}


/* create directory path recursively and chown each new directory */
int mkdirpandchown ( const char* pathname, mode_t mode, const char *new_owner )
{
  int status = 0;
  int chownstatus = 0;
  char *ptr, *strtok_r_savptr;
  char path[MAXPATHLEN+1], work[MAXPATHLEN];
  const char *delimiter = "/";

  // change the owner of this directory to be the user
  // so they can delete operator notes as needed
  //
  // first, we need to obtain the user and group id's
  // of this user for the chown call
  threadsafe_getpwnam tso;
  struct passwd* pswd = tso.invoke_function ( &status, new_owner ); // one new owner, group and user id are both from same owner

  /* make working copy of path name for string tokenizer */
  strncpy (path, pathname, MAXPATHLEN);
  path[MAXPATHLEN] = '\0';

  /* initialize working path name */
  *work = '\0';
  if (*pathname == '/') strcat (work, "/");

  /* get first token in path name */
  ptr = strtok_r (path, delimiter, &strtok_r_savptr);

  /* break the path apart and create each portion with the
   * specified mode if it does not already exist
   */
  while (ptr)
  {
    /* append next piece of the path name to working path name */
    strcat (work, ptr);

    /* if this portion of the path does not exist, create it */
    if ( -1 == access (work, F_OK) )
    {
      do
      {
        pthread_testcancel();
        errno = 0;
        status = mkdir (work, mode);
      } while ( -1 == status  &&  EINTR == errno );

      if ( -1 != status )
      {
        do
        {
          pthread_testcancel();
          errno = 0;
          chownstatus = chown ( work, pswd->pw_uid, pswd->pw_gid );
        } while ( -1 == chownstatus  &&  EINTR == errno );
      }

      if ( -1 == status  ||  -1 == chownstatus )
      {
        return -1;
      }
    }

    /* concatenate another slash to the end of our working path name */
    strcat (work, "/");

    /* get next token in path name */
    ptr = strtok_r (NULL, delimiter, &strtok_r_savptr);
  }

  return 0;
}


/****************************************************************
 * platform specific functions
 ****************************************************************/


#if defined(__linux)


/* create directory path recursively */
int mkdirp ( const char* pathname, mode_t mode )
{
  int status = 0;
  char *ptr, *strtok_r_savptr;
  char path[MAXPATHLEN+1], work[MAXPATHLEN];
  const char *delimiter = "/";

  /* make working copy of path name for string tokenizer */
  strncpy (path, pathname, MAXPATHLEN);
  path[MAXPATHLEN] = '\0';

  /* initialize working path name */
  *work = '\0';
  if (*pathname == '/') strcat (work, "/");

  /* get first token in path name */
  ptr = strtok_r (path, delimiter, &strtok_r_savptr);

  /* break the path apart and create each portion with the
   * specified mode if it does not already exist
   */
  while (ptr)
  {
    /* append next piece of the path name to working path name */
    strcat (work, ptr);

    /* if this portion of the path does not exist, create it */
    if (access (work, F_OK) == -1)
    {
      do
      {
        pthread_testcancel();

        errno = 0;
        status = mkdir (work, mode);
      } while ( status == -1  &&  errno == EINTR );

      if (status == -1)
      {
        return -1;
      }
    }

    /* concatenate another slash to the end of our working path name */
    strcat (work, "/");

    /* get next token in path name */
    ptr = strtok_r (NULL, delimiter, &strtok_r_savptr);

  }

  return 0;

}


#elif defined(__sgi)


/* return string describing signal */
char* strsignal ( int signo )
{

  /* list of descriptive signal strings */
  static char* signal_strings[] =
  {
    "hangup",
    "interrupt (rubout)",
    "quit (ASCII FS)",
    "illegal instruction (not reset when caught)",
    "trace trap (not reset when caught)",
    "IOT instruction -or- used by abort, replace SIGIOT in the  future",
    "EMT instruction",
    "floating point exception",
    "kill (cannot be caught or ignored)",
    "bus error",
    "segmentation violation",
    "bad argument to system call",
    "write on a pipe with no one to read it",
    "alarm clock",
    "software termination signal from kill",
    "user defined signal 1",
    "user defined signal 2",
    "death of a child",
    "power-fail restart",
    "window size changes",
    "urgent condition on IO channel",
    "pollable event occurred -or- input/output possible signal",
    "sendable stop signal not from tty",
    "stop signal from tty",
    "continue a stopped process",
    "to readers pgrp upon background tty read",
    "like TTIN for output if (tp->t_local&LTOSTOP)",
    "virtual time alarm",
    "profiling alarm",
    "Cpu time limit exceeded",
    "Filesize limit exceeded",
    "Reserved for kernel usage",
    "Checkpoint warning",
    "Restart warning ",
    "Undefined Signal 35",
    "Undefined Signal 36",
    "Undefined Signal 37",
    "Undefined Signal 38",
    "Undefined Signal 39",
    "Undefined Signal 40",
    "Undefined Signal 41",
    "Undefined Signal 42",
    "Undefined Signal 43",
    "Undefined Signal 44",
    "Undefined Signal 45",
    "Undefined Signal 46",
    "Signal 47 - SIGPTINTR defined for Posix 1003.1c.",
    "Signal 48 - SIGPTRESCHED defined for Posix 1003.1c.",
    "Real-time signal 49",
    "Real-time signal 50",
    "Real-time signal 51",
    "Real-time signal 52",
    "Real-time signal 53",
    "Real-time signal 54",
    "Real-time signal 55",
    "Real-time signal 56",
    "Real-time signal 57",
    "Real-time signal 58",
    "Real-time signal 59",
    "Real-time signal 60",
    "Real-time signal 61",
    "Real-time signal 62",
    "Real-time signal 63",
    "Real-time signal 64"

  };

  static int nent = sizeof(signal_strings) / sizeof(signal_strings[0]);

  /* in case of an invalid signal number use this */
  static char unknown_signal[40];

  /* calculate index */
  int ix = signo - 1;

  /* if the user's signal number is out of range */
  if ( ix < 0  ||  ix >= nent )
  {
    snprintf (unknown_signal, sizeof(unknown_signal), "Unknown signal %d", signo);
    return unknown_signal;
  }

  return signal_strings[ix];

}


#elif defined(sun)


#endif


#if defined(__cplusplus)
}
#endif

/**********************************************************
 * note:  the following are actually C++ functions so they
 * fall outside the range of the 'extern "C" { }' braces
 *********************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PThreadKey.h"
#include "PMutex.h"


// this function is a "helper" function to the thread
// safe version of the system strerror function below.
// it will delete any strerror message space allocated
// for a particular thread when the thread exits or
// is cancelled.
static void threadsafe_strerror_destructor ( void* key )
{
  // delete the strerror buffer space.  "key" should
  // never be null if we're in here, but it can't
  // hurt to check it either
  if ( key )
  {
    char* ptr = (char *)key;
    delete[] ptr;
  }
}


// this function is a replacement for the standard strerror
// function which is not thread safe.  it utilizes the
// reentrant strerror_r version from the system library,
// but it manages the buffer allocation for the caller
// on a per thread basis.
const char* threadsafe_strerror ( int errnum )
{
  static size_t buffer_size = 1024;
  static PThreadKey buffer ( threadsafe_strerror_destructor );

  // fetch the strerror buffer pointer for this thread
  char* ptr = (char *)buffer.get();

  // oops, don't have one yet, so create it
  if ( ptr == NULL )
  {
    ptr = new char[buffer_size];

    if ( ! ptr )
    {
      return "Unable to allocate heap space for threadsafe_strerror";
    }


    buffer.set ( (PThreadKey::PThreadKeyDataType)ptr );
  }

  // translate the error number into text.  note that it is
  // NOT required that ptr == errstr, and in fact it does not.
  const char* errstr = NULL;

#if defined(__linux)
  errstr = strerror_r ( errnum, ptr, buffer_size );
#elif defined(sun)
  if ( 0 == strerror_r ( errnum, ptr, buffer_size ) ) errstr = ptr;
#endif

  if ( NULL == errstr ) errstr = "unknown error";

  // return thread safe error string to the caller
  return errstr;

}


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
void inet_ntoa_r ( struct in_addr in, char* buf, int bufsize )
{
  static PMutex mutex_lock;

  // initialize users buffer
  *buf = '\0';

  // protect actual inet_ntoa call with mutex lock
  mutex_lock.lock();

  // make inet_ntoa call
  char* ptr = inet_ntoa(in);

  if ( ptr )
  {
    // copy inet_ntoa static buffer to users buffer
    strncpy ( buf, ptr, bufsize );
    buf[bufsize-1] = '\0';
  }

  // release the inet_ntoa mutex lock
  mutex_lock.unlock();

}

