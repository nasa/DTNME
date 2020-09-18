
// This class provides some time manipulation functionality
// for time values in the CCSDS secondary packet header
// "coarse" time format.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EmbeddedTime.h"
#include "CUtils.h"


// static constant variable initialization
long EmbeddedTime::m_utcdiff = 0;


// compute static constant gps-utc time difference
void EmbeddedTime::compute_utcdiff()
{
  if (m_utcdiff == 0)
  {
    // compute the static constant difference in seconds
    // between midnight 5-6 January 1980 (GPS time) and
    // seconds since 1/1/1970 (UTC time) just this once
    long utcdiff = 0;

    for (int yr=1970; yr < 1980; yr++)
      utcdiff += (Leap(yr)  ?  366 : 365) * 24 * 60 * 60;

    utcdiff += 5 * 24 * 60 * 60;  // five days of January 1980

    m_utcdiff = utcdiff;
  }

}


// get a simple byte pointer to the embedded time type in this object
const unsigned char* EmbeddedTime::getEmbeddedTimePtr() const
{
  return m_time;
}


// get the difference between GPS and UTC epoch times
long EmbeddedTime::getUTCdiff() const
{
  return m_utcdiff;
}


// convert coarse + fine times and store in m_time
void EmbeddedTime::convert_time()
{
  int seconds = m_coarse;
  byte_swap4 ( (unsigned char *)&seconds, 4 );
  memcpy ( m_time, &seconds, 4 );
  m_time[4] = m_fine;
}


// set a new time
void EmbeddedTime::settime ( const unsigned char* time )
{
  memset ( m_stringtime, 0, sizeof(m_stringtime) );
  memset ( m_juliantime, 0, sizeof(m_juliantime) );

  if (time)
  {
    memcpy ( m_time, time, Embedded_Time_Size );
    memcpy ( &m_coarse, time, sizeof(m_coarse) );
    byte_swap4 ( (unsigned char *)&m_coarse, 4 );
    m_fine = time[4];
  }
  else
  {
    memset ( m_time, 0, Embedded_Time_Size );
    m_coarse = 0;
    m_fine = 0;
  }

}


// set a new time
void EmbeddedTime::settime ( int coarse_time, int fine_time )
{
  memset ( m_stringtime, 0, sizeof(m_stringtime) );
  memset ( m_juliantime, 0, sizeof(m_juliantime) );

  m_coarse = 0;
  m_fine = 0;

  if ( coarse_time >= 0 )
  {
    m_coarse = coarse_time;
  }

  if ( fine_time >= 0  &&  fine_time < 256 )
  {
    m_fine = fine_time;
  }

  // make conversion to embedded time format
  // and store in m_time data member
  convert_time();

}


// set a new time
void EmbeddedTime::settime ( struct timeval time )
{
  memset ( m_stringtime, 0, sizeof(m_stringtime) );
  memset ( m_juliantime, 0, sizeof(m_juliantime) );

  m_coarse = 0;
  m_fine = 0;

  long seconds = time.tv_sec - m_utcdiff;

  if ( seconds >= 0 )
  {
    m_coarse = seconds;

    if ( time.tv_usec >= 0  &&  time.tv_usec < 1000000 )
    {
      long long fraction = ( (long long)time.tv_usec * 256LL ) / 1000000LL;
      m_fine = fraction;
    }
  }

  // make conversion to embedded time format
  // and store in m_time data member
  convert_time();

}


// default constructor
EmbeddedTime::EmbeddedTime()
{
  compute_utcdiff();
  settime ((const unsigned char *)NULL);
}

// copy constructor
EmbeddedTime::EmbeddedTime ( const EmbeddedTime& other )
  : TimeUtil()
{
  compute_utcdiff();
  *this = other;
}

// overloaded constructors
EmbeddedTime::EmbeddedTime ( const unsigned char* time )
{
  compute_utcdiff();
  settime (time);
}

EmbeddedTime::EmbeddedTime ( int coarse_time, int fine_time )
{
  compute_utcdiff();
  settime ( coarse_time, fine_time );
}

EmbeddedTime::EmbeddedTime ( struct timeval time )
{
  compute_utcdiff();
  settime ( time );
}


// destructor
EmbeddedTime::~EmbeddedTime()
{
}


// overloaded assignment operator
EmbeddedTime& EmbeddedTime::operator= ( const EmbeddedTime& other )
{
  m_coarse = other.m_coarse;
  m_fine = other.m_fine;
  memcpy ( m_time, other.m_time, Embedded_Time_Size );
  memcpy ( m_stringtime, other.m_stringtime, sizeof(m_stringtime) );
  memcpy ( m_juliantime, other.m_juliantime, sizeof(m_juliantime) );
  return *this;
}


// convert to UTC (seconds since midnight 1/1/1970)
long EmbeddedTime::getUTC() const
{
  // since we already have seconds since midnight 5-6 January 1980
  // (a.k.a. GPS time) all we have to do to get UTC is add to that
  // value the number of seconds from 1/1/1970 (UTC time) to the
  // GPS start time.
  return m_coarse + m_utcdiff;
}


// get coarse time
long EmbeddedTime::getCoarseTime() const
{
  return m_coarse;
}


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
long EmbeddedTime::getFineTime ( int granularity ) const
{
  if ( granularity <= 0 ) return ( (long)m_fine & 0xff );

  long fine_time = ( (long)granularity * ( (long)m_fine & 0xffL ) ) / 256L;
  return fine_time;
}


// convert to string.  format is "Fri Sep 13 00:00:00.0 1986".
const char* EmbeddedTime::toString()
{
  if (*m_stringtime != 0) return m_stringtime;

  long utc = getUTC();
  int year, month, day, hour, minute, second, tenths;

  fromUTC (utc, &year, &month, &day, &hour, &minute, &second);
  tenths = ( 10 * ( (int)m_fine & 0xff ) ) / 256;

  snprintf ( m_stringtime, sizeof(m_stringtime),
    "%s %s %02d %02d:%02d:%02d.%01d %4d",
    strdow (year, month, day), strmonth (month),
    day, hour, minute, second, tenths, year );

  return m_stringtime;

}


// convert to string.  format is julian time "ddd:hh:mm:ss.hh".
//
// note: the argument is the accuracy at which you wish to
// format the fractional seconds.  the default is tenths
// of seconds, but you may also specify centiseconds or
// milliseconds by specifying 100 or 1000 as the argument.
// any other values will use the default tenths of seconds.
const char* EmbeddedTime::toString ( int arg )
{
  static const char* fmt10   = "%04d/%03d:%02d:%02d:%02d.%01d";
  static const char* fmt100  = "%04d/%03d:%02d:%02d:%02d.%02d";
  static const char* fmt1000 = "%04d/%03d:%02d:%02d:%02d.%03d";

  long utc = getUTC();
  int year, julianday, hour, minute, second, fraction;
  int multiplier = 10;
  const char* fmt = fmt10;

  fromUTC ( utc, &year, &julianday, &hour, &minute, &second );

  switch ( arg )
  {
  case 100:
    multiplier = 100;
    fmt = fmt100;
    break;

  case 1000:
    multiplier = 1000;
    fmt = fmt1000;
    break;

  case 10:
  default:
    multiplier = 10;
    fmt = fmt10;
    break;
  }

  fraction = ( multiplier * ( (int)m_fine & 0xff ) ) / 256;

  snprintf ( m_juliantime, sizeof(m_juliantime), fmt,
    year, julianday, hour, minute, second, fraction );

  return m_juliantime;

}


// comparison operators
bool EmbeddedTime::operator== ( const unsigned char* time )
{
  return (*this == EmbeddedTime(time));
}


bool EmbeddedTime::operator== ( const EmbeddedTime& time )
{
  if (compare(time) == 0)
    return true;
  else
    return false;
}


bool EmbeddedTime::operator!= ( const unsigned char* time )
{
  return (*this != EmbeddedTime(time));
}


bool EmbeddedTime::operator!= ( const EmbeddedTime& time )
{
  if (compare(time) != 0)
    return true;
  else
    return false;
}


bool EmbeddedTime::operator> ( const unsigned char* time )
{
  return (*this > EmbeddedTime(time));
}


bool EmbeddedTime::operator> ( const EmbeddedTime& time )
{
  if (compare(time) > 0)
    return true;
  else
    return false;
}


bool EmbeddedTime::operator>= ( const unsigned char* time )
{
  return (*this >= EmbeddedTime(time));
}


bool EmbeddedTime::operator>= ( const EmbeddedTime& time )
{
  if (compare(time) >= 0)
    return true;
  else
    return false;
}


bool EmbeddedTime::operator< ( const unsigned char* time )
{
  return (*this < EmbeddedTime(time));
}


bool EmbeddedTime::operator< ( const EmbeddedTime& time )
{
  if (compare(time) < 0)
    return true;
  else
    return false;
}


bool EmbeddedTime::operator<= ( const unsigned char* time )
{
  return (*this <= EmbeddedTime(time));
}


bool EmbeddedTime::operator<= ( const EmbeddedTime& time )
{
  if (compare(time) <= 0)
    return true;
  else
    return false;
}


bool EmbeddedTime::isZero()
{
  return (m_coarse == 0  &&  m_fine == 0);
}


// this method returns the following:
// -1 this < time
//  0 this = time
//  1 this > time
//
int EmbeddedTime::compare ( const EmbeddedTime& time )
{
  if (m_coarse > time.m_coarse)
    return 1;
  else if (m_coarse < time.m_coarse)
    return -1;
  else if (m_fine > time.m_fine)
    return 1;
  else if (m_fine < time.m_fine)
    return -1;
  else
    return 0;

}

