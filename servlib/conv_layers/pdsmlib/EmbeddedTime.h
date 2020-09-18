
#ifndef __EMBEDDEDTIME_H__
#define __EMBEDDEDTIME_H__

#include <sys/time.h>

#include "TimeUtil.h"


// This class provides some time manipulation functionality
// for time values in the CCSDS secondary packet header
// "coarse" time format.
//
// NOTE: this class is NOT all inclusive.  there may be some
// additional functionality required in the future to support
// other operations.  this class was originally designed for
// use in the DSM software, and only the functionality required
// to support its needs was originally implemented.  it should
// hopefully just be a matter of adding additional methods to
// this class, or deriving another class from this one, to
// support any additional operations.

class EmbeddedTime : virtual public TimeUtil
{
// *** member variables ***
public:
  // size of a CCSDS secondary packet header "coarse" time value
  typedef enum EMBEDDED_TIME_SIZE
  {
    Embedded_Time_Size = 5
  } EMBEDDED_TIME_SIZE_t;


protected:
  // difference between GPS and UTC time in seconds
  static long m_utcdiff;

  // number of seconds in the "coarse" time value
  int m_coarse;

  // 256ths of a second in the "fine" time value
  unsigned char m_fine;

  // actual copy of the time in the embedded time format
  unsigned char m_time[Embedded_Time_Size];

  // time value formatted as a printable string.  the format used
  // is the standard unix time format "Fri Sep 13 00:00:00.0 1986"
  char m_stringtime[40];

  // time value formatted as a printable string.  the format used
  // is the julian data "ddd:hh:mm:ss.t".
  char m_juliantime[40];

private:


// *** methods ***
public:
  // default constructor
  EmbeddedTime();

  // copy constructor
  EmbeddedTime ( const EmbeddedTime& other );

  // overloaded constructors
  EmbeddedTime ( const unsigned char* time );
  EmbeddedTime ( int coarse_time, int fine_time );
  EmbeddedTime ( struct timeval time );

  // destructor
  virtual ~EmbeddedTime();

  // overloaded assignment operator
  virtual EmbeddedTime& operator= ( const EmbeddedTime& other );

  // set a new time
  virtual void settime ( const unsigned char* time );

  // set a new time
  virtual void settime ( int coarse_time, int fine_time );

  // set a new time
  virtual void settime ( struct timeval time );

  // get a simple byte pointer to the embedded time type in this object
  virtual const unsigned char* getEmbeddedTimePtr() const;

  // get the difference between GPS and UTC epoch times
  virtual long getUTCdiff() const;

  // convert to UTC (seconds since midnight 1/1/1970)
  virtual long getUTC() const;

  // get coarse time
  virtual long getCoarseTime() const;

  // get fine time converted to desired granularity.
  // the default (=0) is the exact value as contained
  // in the packet, i.e. 256ths of a second.  otherwise,
  // the caller can specify values such as 10 to get
  // 10ths of seconds, 100 for 100ths of seconds, and
  // so forth.  but be wary, if you input an non-sensical
  // value for the granularity, expect an non-sensical
  // result to be returned to you.
  //
  // this is the fractional part of an embedded time.
  virtual long getFineTime ( int granularity = 0 ) const;

  // convert to string.  format is "Fri Sep 13 00:00:00.00 1986".
  virtual const char* toString();

  // convert to string.  format is julian time "ddd:hh:mm:ss.t".
  //
  // note: the argument is the accuracy at which you wish to
  // format the fractional seconds.  the default is tenths
  // of seconds, but you may also specify centiseconds or
  // milliseconds by specifying 100 or 1000 as the argument.
  // any other values will use the default tenths of seconds.
  virtual const char* toString ( int arg );

  // equality comparison operators
  virtual bool operator== ( const unsigned char* time );
  virtual bool operator== ( const EmbeddedTime& time );

  // inequality comparison operators
  virtual bool operator!= ( const unsigned char* time );
  virtual bool operator!= ( const EmbeddedTime& time );

  // greater than comparison operators
  virtual bool operator> ( const unsigned char* time );
  virtual bool operator> ( const EmbeddedTime& time );

  // greater than or equal to comparison operators
  virtual bool operator>= ( const unsigned char* time );
  virtual bool operator>= ( const EmbeddedTime& time );

  // less than comparison operators
  virtual bool operator< ( const unsigned char* time );
  virtual bool operator< ( const EmbeddedTime& time );

  // less than or equal to comparison operators
  virtual bool operator<= ( const unsigned char* time );
  virtual bool operator<= ( const EmbeddedTime& time );

  // check to see if the time value is all zero
  virtual bool isZero();

protected:
  // method to perform actual time comparison checks.  similar to
  // the strcmp "C" function, this method returns 0 if the time
  // values are equal, -1 if the objects time is less than the
  // comparison time, or 1 if the objects time is greater than
  // the comparison time
  virtual int compare ( const EmbeddedTime& time );

  // compute static constant gps-utc time difference
  virtual void compute_utcdiff();

  // convert coarse + fine time and store in m_time
  virtual void convert_time();

private:

};

#endif  // __EMBEDDEDTIME_H__

