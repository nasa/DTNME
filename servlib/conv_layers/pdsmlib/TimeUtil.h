
#ifndef __TIMEUTIL_H__
#define __TIMEUTIL_H__


// This class provides some time manipulation functionality.
//
// NOTE: it is assumed throughout this class (input and output)
// that time values are 1-relative, i.e. the first day (day 1) of
// the week is Sunday, the last (day 7) is Saturday.  The first
// month (month 1) of the year is January, the last (month 12)
// is December, and so on.

class TimeUtil
{

// *** member variables ***
public:

protected:
  // class wide constants for the number of days in a month,
  // and ascii strings for the day of week, and the month name
  static const int m_Days[2][13];
  static const char* m_Dow[8];
  static const char* m_Months[13];

private:


// *** methods ***
public:
  // constructor
  TimeUtil();

  // destructor
  virtual ~TimeUtil();

  // returns 0 if year is not a leap year, 1 if it is
  virtual int Leap ( int year );

  // get the number of days in the specified month.  if the
  // year is not specified, the current year is assumed.
  virtual int monthdays ( int month, int year = -1 );

  // get the day of the week for a gregorian or julian calendar
  // date as an integer day of the week
  virtual int dow ( int year, int month, int day );
  virtual int dow ( int year, int julianday );

  // get the day of the week for a gregorian or julian calendar
  // date as a 3-character string
  virtual const char* strdow ( int year, int month, int day );
  virtual const char* strdow ( int year, int julianday );
  virtual const char* strdow ( int wday );

  // get the month of the year from a julian day
  // of year as an integer
  virtual int month ( int year, int julianday );

  // get the month of the year from a julian day
  // of year as a 3-character string
  virtual const char* strmonth ( int year, int julianday );

  // get the string value of the month
  virtual const char* strmonth ( int month );

  // get the julian day of year from the gregorian month and day
  virtual int julian ( int year, int month, int day );

  // get the gregorian month and day from the julian day of year
  virtual void gregorian ( int year, int julianday,
    int* month, int* day );

  // convert a gregorian or julian date to UTC (seconds since 1/1/1970)
  virtual long toUTC ( int year, int month, int day, int hour,
    int minute, int second );
  virtual long toUTC ( int year, int julianday, int hour,
    int minute, int second );

  // convert UTC time to a gregorian or julian date
  virtual void fromUTC ( long utc, int* year, int* month, int* day,
    int* hour, int* minute, int* second );
  virtual void fromUTC ( long utc, int* year, int* julianday,
    int* hour, int* minute, int* second );

protected:

private:

};

#endif  // __TIMEUTIL_H__

