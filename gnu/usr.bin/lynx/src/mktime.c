/*
 * mktime.c -- converts a struct tm into a time_t
 *
 * Copyright (C) 1997 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Written by Philippe De Muyter <phdm@macqel.be>.  */

#include	<time.h>

static time_t mkgmtime(register struct tm *t)
{
    register short month, year;
    register time_t result;
    static int m_to_d[12] =
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    month = t->tm_mon;
    year = t->tm_year + month / 12 + 1900;
    month %= 12;
    if (month < 0) {
	year -= 1;
	month += 12;
    }
    result = (year - 1970) * 365 + m_to_d[month];
    if (month <= 1)
	year -= 1;
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    result += t->tm_mday;
    result -= 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    return (result);
}

/*
 *  mktime -- convert tm struct to time_t
 *		if tm_isdst >= 0 use it, else compute it
 */

time_t mktime(struct tm * t)
{
    time_t result;

    tzset();
    result = mkgmtime(t) + timezone;
    if (t->tm_isdst > 0
	|| (t->tm_isdst < 0 && localtime(&result)->tm_isdst))
	result -= 3600;
    return (result);
}
