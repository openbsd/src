/* $OpenBSD: time.c,v 1.1 1997/04/28 07:39:01 weingart Exp $ */

#include <libsa.h>
#include <sys/time.h>
#include "biosdev.h"

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)


/*
 * Convert from bcd (packed) to int
 */
static int bcdtoint(char c){
	int tens;
	int ones;

	tens = (c & 0xf0) >> 4;
	tens *= 10;
	ones = c & 0x0f;

	return (tens + ones);
}


/* Number of days per month */
static int monthcount[] = {
	31, 28, 31, 30, 31, 30, 31,
	31, 30, 31, 30, 31, 30, 31
};

/*
 * Quick compute of time in seconds since the Epoch
 */
static time_t
compute(int year, int month, int day, int hour, int min, int sec){
	int yearsec, daysec, timesec;
	int i;

	/* Compute years of seconds */
	yearsec = year - 1970;
	yearsec *= (365 * 24 * 60 * 60);

	/* Compute days of seconds */
	daysec = 0;
	for(i = 1; i < month; i++){
		daysec += monthcount[i];
	}
	daysec += day;

	/* Compute for leap year */
	for(i = 1970; i < year; i++){
		if(isleap(i))
			daysec += 1;
	}
	daysec *= (24 * 60 * 60);

	/* Plus the time */
	timesec = sec;
	timesec += (min * 60);
	timesec += (hour * 60 * 60);

	/* Return sum */
	return (yearsec + daysec + timesec);
}


/*
 * Return time since epoch
 */
time_t getsecs(void){
	char timebuf[4], datebuf[4];
	int st1, st2;
	time_t tt = 0;

	/* Query BIOS for time & date */
	st1 = biostime(timebuf);
	st2 = biosdate(datebuf);

	/* Convert to seconds since Epoch */
	if(!st1 && !st2){
		int year, month, day;
		int hour, min, sec;
		int dst;

		dst = bcdtoint(timebuf[3]);
		sec = bcdtoint(timebuf[2]);
		min = bcdtoint(timebuf[1]);
		hour = bcdtoint(timebuf[0]);

		year = bcdtoint(datebuf[0]);
		year *= 100;
		year += bcdtoint(datebuf[1]);
		month = bcdtoint(datebuf[2]);
		day = bcdtoint(datebuf[3]);

		printf("%d/%d/%d - %d:%d:%d\n", day, month, year, hour, min, sec);

		tt = compute(year, month, day, hour, min, sec);
		return(tt);
	}

	return(1);
}


/*
 * Return time since epoch
 */
void time_print(void){
	char timebuf[4], datebuf[4];
	int st1, st2;

	/* Query BIOS for time & date */
	st1 = biostime(timebuf);
	st2 = biosdate(datebuf);

	/* Convert to sane values */
	if (!st1 && !st2) {
		int year, month, day;
		int hour, min, sec;
		int dst;

		dst = bcdtoint(timebuf[3]);
		sec = bcdtoint(timebuf[2]);
		min = bcdtoint(timebuf[1]);
		hour = bcdtoint(timebuf[0]);

		year = bcdtoint(datebuf[0]);
		year *= 100;
		year += bcdtoint(datebuf[1]);
		month = bcdtoint(datebuf[2]);
		day = bcdtoint(datebuf[3]);

		printf("%d/%d/%d - %d:%d:%d\n", day, month, year, hour, min, sec);

	} else
		printf("Error in biostime() or biosdate().\n");

	return;
}

