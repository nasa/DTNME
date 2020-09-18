
#ifndef __EHSTIME_H__
#define __EHSTIME_H__

#include <sys/time.h>

#include "Protocols.h"
#include "TimeUtil.h"


// This class provides some time manipulation functionality
// for time values in the EHS_Time_Type_t format.
//
// NOTE: this class is NOT all inclusive.  there may be some
// additional functionality required in the future to support
// other operations.  this class was originally designed for
// use in the DSM software, and only the functionality required
// to support its needs was originally implemented.  it should
// hopefully just be a matter of adding additional methods to
// this class, or deriving another class from this one, to
// support any additional operations.

class EHSTime : virtual public TimeUtil
{
// *** member variables ***
public:

protected:
  // time value in the EHS_Time_Type_t format
  EHS_Time_Type_t m_time;

  // time value formatted as a printable string.  the format used
  // is the standard unix time format "Fri Sep 13 00:00:00.0 1986"
  char m_stringtime[40];

  // time value formatted as a printable string.  the format used
  // is the julian data "ddd:hh:mm:ss.t".
  char m_juliantime[20];

  // flag if zero time is considered invalid by isInvalid method.
  // the default for this flag is "true"
  bool m_zeroIsInvalid;

private:


// *** methods ***
public:
  // default constructor
  EHSTime();

  // copy constructor
  EHSTime ( const EHSTime& other );

  // constructor that accepts a simple byte pointer which
  // points to a time value in the EHS_Time_Type_t format
  EHSTime ( const unsigned char* time );

  // constructor that accepts an EHS_Time_Type_t structure pointer
  EHSTime ( const EHS_Time_Type_t* time );

  // constructor that accepts an EHS_Time_Type_t structure reference
  EHSTime ( const EHS_Time_Type_t& time );

  // constructor that accepts the individual structure values
  // for an EHS_Time_Type_t structure reference
  EHSTime ( int year, int day, int hour, int minute, int second, int tenths );

  // constructor for gettimeofday format
  EHSTime ( struct timeval time );

  // destructor
  virtual ~EHSTime();

  // overloaded assignment operator
  virtual EHSTime& operator= ( const EHSTime& other );

  // set a new time
  virtual void settime ( const unsigned char* time );
  virtual void settime ( const EHS_Time_Type_t* time );
  virtual void settime ( const EHS_Time_Type_t& time );

  virtual void settime ( int year, int day, int hour, int minute, int second, int tenths );

  virtual void settime ( struct timeval time );

  // get an EHS_Time_Type_t pointer to the time in this object
  virtual const EHS_Time_Type_t* getEHSTimeType() const;

  // get a simple byte pointer to the EHS_Time_Type_t time in this object
  virtual const unsigned char* getEHSTimeTypePtr() const;

  // convert to UTC (seconds since midnight 1/1/1970)
  virtual long getUTC();

  // convert to string.  format is "Fri Sep 13 00:00:00.00 1986".
  virtual const char* toString();

  // convert to string.  format is julian time "ddd:hh:mm:ss.t".
  //
  // note: the argument is not significant, it is just a simple
  // mechanism for overloading the toString method to provide
  // output in another format.  an int argument was chosen because
  // it seems somewhat natural to think of the julian time as an
  // "integer" type of format.
  virtual const char* toString ( int arg );

  // get discrete time values
  //
  // years since 1900
  // julian day of year
  // hour, minute, second, tenths of seconds
  virtual void gettime ( int* year, int* day, int* hour, int* minute, int* seconds, int* tenths );

  // get coarse time and fine time, i.e. convert
  // from EHS time format to embedded time format
  virtual int getcoarsetime();
  virtual int getfinetime();

  // equality comparison operators
  virtual bool operator== ( const unsigned char* time );
  virtual bool operator== ( const EHSTime& time );

  // inequality comparison operators
  virtual bool operator!= ( const unsigned char* time );
  virtual bool operator!= ( const EHSTime& time );

  // greater than comparison operators
  virtual bool operator> ( const unsigned char* time );
  virtual bool operator> ( const EHSTime& time );

  // greater than or equal to comparison operators
  virtual bool operator>= ( const unsigned char* time );
  virtual bool operator>= ( const EHSTime& time );

  // less than comparison operators
  virtual bool operator< ( const unsigned char* time );
  virtual bool operator< ( const EHSTime& time );

  // less than or equal to comparison operators
  virtual bool operator<= ( const unsigned char* time );
  virtual bool operator<= ( const EHSTime& time );

  // check to see if the time value is all zero
  virtual bool isZero();

  // check to see if the time value is invalid
  virtual bool isInvalid();

  // get/set zero is invalid flag
  virtual bool get_zeroIsInvalid() const;
  virtual void set_zeroIsInvalid ( bool flag );


protected:
  // method to perform actual time comparison checks.  similar to
  // the strcmp "C" function, this method returns 0 if the time
  // values are equal, -1 if the objects time is less than the
  // comparison time, or 1 if the objects time is greater than
  // the comparison time
  virtual int compare ( const unsigned char* timeptr );

private:

};

#endif  // __EHSTIME_H__

