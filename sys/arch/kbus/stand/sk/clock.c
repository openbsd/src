#include <sys/types.h>

#include "clockreg.h"
#include "config.h"
#include "clock.h"

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROM_BCD(x,y)      ((x) + 10 * (y))

#define SECDAY          (24 * 60 * 60)
#define SECYR           (SECDAY * 365)
#define LEAPYEAR(y)     (((y) & 3) == 0)

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static  u_long
chiptotime(sec, min, hour, day, mon, year)
	register int sec, min, hour, day, mon, year;
{
	register int days, yr;

	if (year < 70)
		year = 70;

	/* simple sanity checks */
	if (year < 70 || mon < 1 || mon > 12 || day < 1 || day > 31)
		return (0);
	days = 0;
	for (yr = 70; yr < year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[mon - 1] + day - 1;
	if (LEAPYEAR(yr) && mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	return (days * SECDAY + hour * 3600 + min * 60 + sec);
}

static u_char *rtc_byte = (u_char *) 0x17020000;

static u_char
rtc_read_reg (int reg)
{
  u_char res;

  reg &= 0x0f;
  *rtc_byte = RTC_WRITE_ADDR | reg;
  *rtc_byte = reg;
  *rtc_byte = RTC_READ | 0x0f;
  res = *rtc_byte & 0x0f;
  *rtc_byte = RTC_WRITE_ADDR | 0x0f;
  *rtc_byte = 0x0f;
  return res;
}

time_t
getsecs()
{
  int     sec, min, hour, hourl, hourh, day, mon, year;

  sec = FROM_BCD (rtc_read_reg (RTC_SEC_LOW),
		  rtc_read_reg (RTC_SEC_HIGH));
  min = FROM_BCD (rtc_read_reg (RTC_MIN_LOW),
		  rtc_read_reg (RTC_MIN_HIGH));
  day = FROM_BCD (rtc_read_reg (RTC_DAY_LOW),
		  rtc_read_reg (RTC_DAY_HIGH));
  mon = FROM_BCD (rtc_read_reg (RTC_MON_LOW),
		  rtc_read_reg (RTC_MON_HIGH));
  year = RTC_YEAR_BASE + FROM_BCD (rtc_read_reg (RTC_YEAR_LOW),
				   rtc_read_reg (RTC_YEAR_HIGH));
  hourl = rtc_read_reg (RTC_HOUR_LOW);
  hourh = rtc_read_reg (RTC_HOUR_HIGH);
  if (hourh & RTC_HOUR_PM)
    hour = 12 + (hourh & 1) * 10 + hourl;
  else
    hour = (hourh & 0x03) * 10 + hourl;
  return (chiptotime(sec, min, hour, day, mon, year));
}

void
disp_date (void)
{
  static char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  int hour, hourl, hourh, day, wday, mon, year;

  wday = rtc_read_reg (RTC_WEEK_DAY);
  day = FROM_BCD (rtc_read_reg (RTC_DAY_LOW),
		  rtc_read_reg (RTC_DAY_HIGH));
  mon = FROM_BCD (rtc_read_reg (RTC_MON_LOW),
		  rtc_read_reg (RTC_MON_HIGH));
  year = RTC_YEAR_BASE + FROM_BCD (rtc_read_reg (RTC_YEAR_LOW),
				   rtc_read_reg (RTC_YEAR_HIGH));
  hourl = rtc_read_reg (RTC_HOUR_LOW);
  hourh = rtc_read_reg (RTC_HOUR_HIGH);

  if (hourh & RTC_HOUR_PM)
    hour = 12 + (hourh & 1) * 10 + hourl;
  else
    hour = (hourh & 0x03) * 10 + hourl;

  printf ("Date: %s %d-%d-%d %d:%d%d:%d%d\n",
	  days[wday],
	  1900 + year, mon, day,
	  hour,
	  rtc_read_reg (RTC_MIN_HIGH),
	  rtc_read_reg (RTC_MIN_LOW),
	  rtc_read_reg (RTC_SEC_HIGH),
	  rtc_read_reg (RTC_SEC_LOW));
}

int
getticks()
{
	return getsecs() * 100;
}
