
// This class provides some time manipulation functionality.
//
// NOTE: it is assumed throughout this class (input and output)
// that time values are 1-relative, i.e. the first day (day 1) of
// the week in Sunday, the last (day 7) is Saturday.  The first
// month (month 1) of the year is January, the last (month 12)
// is December, and so on.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <time.h>

#include "PThread.h"
#include "TimeUtil.h"


const int TimeUtil::m_Days[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

const char* TimeUtil::m_Dow[8] =
{
  "",
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

const char* TimeUtil::m_Months[13] =
{
  "",
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// constructor
TimeUtil::TimeUtil()
{
}


// destructor
TimeUtil::~TimeUtil()
{
}


// return 0 if not a leap year, 1 if it is
int TimeUtil::Leap ( int year )
{
  return (year%4 == 0  &&  year%100 != 0)  ||  (year%400 == 0);
}


// get the number of days in the specified month.  if the
// year is not specified, the current year is assumed.
int TimeUtil::monthdays ( int month, int year )
{
  // stupid
  if (month < 1  ||  month > 12) return 0;

  // get current year if year was not specified
  if (year < 0)
  {
    struct tm ltm;
    memset ( (void *)&ltm, 0, sizeof(ltm) );

    time_t utc = time(0);
    struct tm* gmtptr = PThread::gmtime ( &utc, &ltm );
    if ( gmtptr == NULL ) gmtptr = &ltm;

    year = gmtptr->tm_year + 1900;
  }

  return m_Days[Leap(year)][month];

}

// get the day of the week for a gregorian or julian calendar
// date as an integer day of the week or as a 3-character string
//
// NOTE: this algorithm was adapted from Peter Baum's
// "Date Algorithms" web page under "Day of Week Implementations"
// http://www.capecod.net/~pbaum/date/weekimp.htm
//
int TimeUtil::dow ( int year, int month, int day )
{
  if (month < 3)
  {
    --year;
    month += 12;
  }

  int dow = day + (153*month - 147)/5 + 365*year + year/4 - year/100 +
    year/400 + 2;

  // not sure why I had to deviate from Peter Baums actual algorithm
  // (there was no "+1" in his modulus).  it may have something to
  // do with the difference between the QBASIC modulus and the C++
  // modulus, but in any case, this seems to work
  dow = (dow+1) % 7;
  if (dow < 0) dow += 7;

  // we are using 1-relative throughout this class as opposed
  // to 0-relative as in Peter Baums examples
  return dow + 1;

}


int TimeUtil::dow ( int year, int julianday )
{
  int month, day;
  gregorian (year, julianday, &month, &day);
  return dow (year, month, day);
}


const char* TimeUtil::strdow ( int year, int month, int day )
{
  return m_Dow [ dow (year, month, day) ];
}


const char* TimeUtil::strdow ( int year, int julianday )
{
  return m_Dow [ dow (year, julianday) ];
}


const char* TimeUtil::strdow ( int wday )
{
  if (wday > 0  &&  wday < 8)
    return m_Dow[wday];
  else
    return "";
}


// get the month of the year from a julian day of year
// as an integer or as a 3-character string
int TimeUtil::month ( int year, int julianday )
{
  int leap = Leap(year);

  for (int j=1; j < 13; j++)
  {
    julianday -= m_Days[leap][j];
    if (julianday <= 0) return j;
  }

  return 12;

}


const char* TimeUtil::strmonth ( int year, int julianday )
{
  return m_Months [ month (year, julianday) ];
}


const char* TimeUtil::strmonth ( int month )
{
  if (month > 0  &&  month < 13)
    return m_Months[month];
  else
    return "";
}


// get the julian day of year from the gregorian month and day
int TimeUtil::julian ( int year, int month, int day )
{
  int jday = 0;
  int leap = Leap(year);

  for (int j=1; j < month; j++) jday += m_Days[leap][j];
  jday += day;

  return jday;

}


// get the gregorian month and day from the julian day of year
void TimeUtil::gregorian ( int year, int julianday,
  int* month, int* day )
{
  // stupid
  if (! month  ||  ! day) return;

  *month = 1;
  *day = julianday;

  int leap = Leap(year);

  for (int j=1; j < 13  &&  *day > m_Days[leap][j]; j++)
  {
    ++(*month);
    *day -= m_Days[leap][j];
  };

}


// convert a gregorian or julian date to UTC (seconds since 1/1/1970)
long TimeUtil::toUTC ( int year, int month, int day, int hour,
  int minute, int second )
{
  int julianday = julian (year, month, day);
  return toUTC (year, julianday, hour, minute, second);
}


long TimeUtil::toUTC ( int year, int julianday, int hour,
  int minute, int second )
{
  // stupid
  if (year < 1970) return 0;

  long utc = 0;

  for (int yr=1970; yr < year; yr++)
    utc += (Leap(yr)  ?  366 : 365) * 24 * 60 * 60;

  utc += (julianday-1) * 24 * 60 * 60;
  utc += hour * 60 * 60;
  utc += minute * 60;
  utc += second;

  return utc;

}


// convert UTC time to a gregorian or julian date
void TimeUtil::fromUTC ( long utc, int* year, int* month, int* day,
  int* hour, int* minute, int* second )
{
  int julianday;
  fromUTC (utc, year, &julianday, hour, minute, second);
  gregorian (*year, julianday, month, day);
}


void TimeUtil::fromUTC ( long utc, int* year, int* julianday,
  int* hour, int* minute, int* second )
{
  // stupid
  if ( ! year  ||  ! julianday  ||  ! hour
    ||  ! minute  ||  ! second ) return;

  *year = 1970;
  *julianday = 0;
  *hour = 0;
  *minute = 0;
  *second = 0;

  int days_yr[2];
  long left, secs;

  days_yr[0] = days_yr[1] = 0;

  for (int j=1; j < 13; j++)
  {
    days_yr[0] += m_Days[0][j];
    days_yr[1] += m_Days[1][j];
  }

  left = utc;
  secs = days_yr[Leap(*year)] * 24 * 60 * 60;

  while (left > secs)
  {
    ++(*year);
    left -= secs;
    secs = days_yr[Leap(*year)] * 24 * 60 * 60;
  }

  *julianday = (left / (24 * 60 * 60)) + 1;
  left = left % (24 * 60 * 60);

  *hour = left / (60 * 60);
  left = left % (60 * 60);

  *minute = left / 60;

  *second = left % 60;

}

