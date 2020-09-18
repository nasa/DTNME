
// This class provides some time manipulation functionality
// for time values in the EHS_Time_Type_t format.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EHSTime.h"
#include "EmbeddedTime.h"
#include "CUtils.h"

// this object is used to access the utc/gps time difference
static EmbeddedTime EHSTime_EmbeddedTime_Object;


// set a new time
void EHSTime::settime ( const unsigned char* time )
{
  if (time)
    memcpy ( &m_time, time, sizeof(m_time) );
  else
    memset ( &m_time, 0, sizeof(m_time) );

  memset ( &m_stringtime, 0, sizeof(m_stringtime) );
  memset ( &m_juliantime, 0, sizeof(m_juliantime) );

  // mask off the flags
  m_time.Tenths_Flags &= 0xf0;

}


void EHSTime::settime ( const EHS_Time_Type_t* time )
{
  settime ((const unsigned char *)time);
}


void EHSTime::settime ( const EHS_Time_Type_t& time )
{
  settime ((const unsigned char *)&time);
}


void EHSTime::settime (
  int year, int day, int hour, int minute, int second, int tenths )
{
  // build the time structure
  m_time.Year = ( year >= 1900  ?  year - 1900 : year );
  m_time.Day_High = (day & 0xff00) >> 8;
  m_time.Day_Low = day & 0xff;
  m_time.Hour = hour;
  m_time.Minute = minute;
  m_time.Second = second;
  m_time.Tenths_Flags = tenths << 4;

  // reset the string times
  memset ( &m_stringtime, 0, sizeof(m_stringtime) );
  memset ( &m_juliantime, 0, sizeof(m_juliantime) );

}


void EHSTime::settime ( struct timeval time )
{
  int year, day, hour, minute, second;
  fromUTC ( time.tv_sec, &year, &day, &hour, &minute, &second );

  // build the time structure
  m_time.Year = ( year >= 1900  ?  year - 1900 : year );
  m_time.Day_High = (day & 0xff00) >> 8;
  m_time.Day_Low = day & 0xff;
  m_time.Hour = hour;
  m_time.Minute = minute;
  m_time.Second = second;

  int tenths = time.tv_usec / 100000;
  m_time.Tenths_Flags = tenths << 4;

  // reset the string times
  memset ( &m_stringtime, 0, sizeof(m_stringtime) );
  memset ( &m_juliantime, 0, sizeof(m_juliantime) );

}


// get an EHS_Time_Type_t pointer to the time in this object
const EHS_Time_Type_t* EHSTime::getEHSTimeType() const
{
  return &m_time;
}


// get a simple byte pointer to the EHS_Time_Type_t time in this object
const unsigned char* EHSTime::getEHSTimeTypePtr() const
{
  return (unsigned char *)&m_time;
}


// default constructor
EHSTime::EHSTime()
{
  m_zeroIsInvalid = true;
  settime ((const unsigned char *)NULL);
}


// copy constructor
EHSTime::EHSTime ( const EHSTime& other )
    : TimeUtil()
{
  *this = other;
}


// overloaded constructors
EHSTime::EHSTime ( const unsigned char* time )
{
  m_zeroIsInvalid = true;
  settime (time);
}


EHSTime::EHSTime ( const EHS_Time_Type_t* time )
{
  m_zeroIsInvalid = true;
  settime ((const unsigned char *)time);
}


EHSTime::EHSTime ( const EHS_Time_Type_t& time )
{
  m_zeroIsInvalid = true;
  settime ((const unsigned char *)&time);
}


EHSTime::EHSTime (
  int year, int day, int hour, int minute, int second, int tenths )
{
  m_zeroIsInvalid = true;
  settime ( year, day, hour, minute, second, tenths );
}


EHSTime::EHSTime ( struct timeval time )
{
  m_zeroIsInvalid = true;
  settime ( time );
}


// destructor
EHSTime::~EHSTime()
{
}


// overloaded assignment operator
EHSTime& EHSTime::operator= ( const EHSTime& other )
{
  m_zeroIsInvalid = other.m_zeroIsInvalid;
  settime (other.m_time);
  return *this;
}


// convert to UTC (seconds since midnight 1/1/1970)
long EHSTime::getUTC()
{
  int year = m_time.Year + 1900;
  int julianday = ((int)m_time.Day_High << 8) | (int)m_time.Day_Low;
  int hour = m_time.Hour;
  int minute = m_time.Minute;
  int second = m_time.Second;

  return toUTC (year, julianday, hour, minute, second);

}


// convert to string.  format is "Fri Sep 13 00:00:00.0 1986".
const char* EHSTime::toString()
{
  if (*m_stringtime != 0) return m_stringtime;

  int year = m_time.Year + 1900;
  int julianday = ((int)m_time.Day_High << 8) | (int)m_time.Day_Low;
  int hour = m_time.Hour;
  int minute = m_time.Minute;
  int second = m_time.Second;
  int tenths = (m_time.Tenths_Flags >> 4) & 0x0f;

  int day, month;
  gregorian (year, julianday, &month, &day);

  snprintf ( m_stringtime, sizeof(m_stringtime),
    "%s %s %02d %02d:%02d:%02d.%01d %4d",
    strdow (year, month, day), strmonth (month),
    day, hour, minute, second, tenths, year );

  return m_stringtime;

}


// convert to string.  format is julian time "ddd:hh:mm:ss.hh".
//
// note: the argument is not significant, it is just a simple
// mechanism for overloading the toString method to provide
// output in another format.  an int argument was chosen because
// it seems somewhat natural to think of the julian time as an
// "integer" type of format.
const char* EHSTime::toString ( int arg )
{
  (void) arg;
  if (*m_juliantime != 0) return m_juliantime;

  int year = m_time.Year + 1900;
  int julianday = ((int)m_time.Day_High << 8) | (int)m_time.Day_Low;
  int hour = m_time.Hour;
  int minute = m_time.Minute;
  int second = m_time.Second;
  int tenths = (m_time.Tenths_Flags >> 4) & 0x0f;

  snprintf ( m_juliantime, sizeof(m_juliantime),
    "%04d/%03d:%02d:%02d:%02d.%01d",
    year, julianday, hour, minute, second, tenths );

  return m_juliantime;

}


// get discrete time values
//
// years since 1900
// julian day of year
// hour, minute, second, tenths of seconds
void EHSTime::gettime ( int* year, int* day, int* hour, int* minute, int* seconds, int* tenths )
{
  int yr = m_time.Year;
  int jday = ((int)m_time.Day_High << 8) | (int)m_time.Day_Low;
  int hr = m_time.Hour;
  int min = m_time.Minute;
  int secs = m_time.Second;
  int ts = (m_time.Tenths_Flags >> 4) & 0x0f;

  if ( year ) *year = yr;
  if ( day ) *day = jday;
  if ( hour ) *hour = hr;
  if ( minute ) *minute = min;
  if ( seconds ) *seconds = secs;
  if ( tenths ) *tenths = ts;

}


// get coarse time and fine time, i.e. convert
// from EHS time format to embedded time format
int EHSTime::getcoarsetime()
{
  int coarse = getUTC() - EHSTime_EmbeddedTime_Object.getUTCdiff();
  return coarse;
}


int EHSTime::getfinetime()
{
  int tenths = (m_time.Tenths_Flags >> 4) & 0x0f;
  int i256ths = ( tenths * 256 ) / 10;
  return i256ths;
}


// comparison operators
bool EHSTime::operator== ( const unsigned char* time )
{
  if (compare(time) == 0)
    return true;
  else
    return false;
}


bool EHSTime::operator== ( const EHSTime& time )
{
  return (*this == (const unsigned char *)&time.m_time);
}


bool EHSTime::operator!= ( const unsigned char* time )
{
  if (compare(time) != 0)
    return true;
  else
    return false;
}


bool EHSTime::operator!= ( const EHSTime& time )
{
  return (*this != (const unsigned char *)&time.m_time);
}


bool EHSTime::operator> ( const unsigned char* time )
{
  if (compare(time) > 0)
    return true;
  else
    return false;
}


bool EHSTime::operator> ( const EHSTime& time )
{
  return (*this > (const unsigned char *)&time.m_time);
}


bool EHSTime::operator>= ( const unsigned char* time )
{
  if (compare(time) >= 0)
    return true;
  else
    return false;
}


bool EHSTime::operator>= ( const EHSTime& time )
{
  return (*this >= (const unsigned char *)&time.m_time);
}


bool EHSTime::operator< ( const unsigned char* time )
{
  if (compare(time) < 0)
    return true;
  else
    return false;
}


bool EHSTime::operator< ( const EHSTime& time )
{
  return (*this < (const unsigned char *)&time.m_time);
}


bool EHSTime::operator<= ( const unsigned char* time )
{
  if (compare(time) <= 0)
    return true;
  else
    return false;
}


bool EHSTime::operator<= ( const EHSTime& time )
{
  return (*this <= (const unsigned char *)&time.m_time);
}


bool EHSTime::isZero()
{
  unsigned char zero[sizeof(m_time)];
  memset ( zero, 0, sizeof(m_time) );
  if (memcmp ( &m_time, zero, sizeof(m_time) ) == 0) return true;
  return false;
}


// check to see if the time value is invalid
bool EHSTime::isInvalid()
{
  // should we consider zero to be an invalid time?
  if ( m_zeroIsInvalid )
  {
    if ( isZero() ) return true;
  }

  // check for other obviously invalid time values
  //
  // note: the year could realistically be just about
  // anything (especially if this happens to be a Sim)
  // so we're not gonna make any checks on it.

  int day = ((int)m_time.Day_High << 8)  |  (int)m_time.Day_Low;
  if ( day < 0  ||  day > 366 ) return true;

  int hour = (int)m_time.Hour;
  if ( hour < 0  ||  hour > 23 ) return true;

  int minute = (int)m_time.Minute;
  if ( minute < 0  ||  minute > 59 ) return true;

  int second = (int)m_time.Second;
  if ( second < 0  ||  second > 59 ) return true;

  int tenths = ( ((int)m_time.Tenths_Flags >> 4)  &  0xf );
  if ( tenths < 0  ||  tenths > 9 ) return true;

  // looks like a valid time value
  return false;

}


// get zero is invalid flag
bool EHSTime::get_zeroIsInvalid() const
{
  return m_zeroIsInvalid;
}


// set zero is invalid flag
void EHSTime::set_zeroIsInvalid ( bool flag )
{
  m_zeroIsInvalid = flag;
}


// this method returns the following:
// -1 m_time < time
//  0 m_time = time
//  1 m_time > time
//
int EHSTime::compare ( const unsigned char* timeptr )
{
  EHS_Time_Type_t time;
  memcpy (&time, timeptr, sizeof(time));

  // not equal(?), continue comparison
  if (m_time.Year < time.Year) return -1;
  if (m_time.Year > time.Year) return 1;

  // year is the same, continue comparison
  int day1 = ((int)m_time.Day_High << 8) | (int)m_time.Day_Low;
  int day2 = ((int)time.Day_High << 8) | (int)time.Day_Low;
  if (day1 < day2) return -1;
  if (day1 > day2) return 1;

  // day is the same, continue comparison

  // convert what is left from both time values to
  // tenths of seconds and compare the results
  long t1 = (m_time.Hour * 3600 * 10) + (m_time.Minute * 60 * 10) +
    (m_time.Second * 10) + ((m_time.Tenths_Flags >> 4) & 0xf);

  long t2 = (time.Hour * 3600 * 10) + (time.Minute * 60 * 10) +
    (time.Second * 10) + ((time.Tenths_Flags >> 4) & 0xf);

  if (t1 < t2)
    return -1;
  else if (t1 > t2)
    return 1;

  // must be equal
  return 0;

}

