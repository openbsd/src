/* $OpenBSD: utils.c,v 1.23 2011/04/10 03:20:59 guenther Exp $	 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  This file contains various handy utilities used by top.
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "top.h"
#include "machine.h"
#include "utils.h"

int
atoiwi(char *str)
{
	size_t len;
	const char *errstr;
	int i;

	len = strlen(str);
	if (len != 0) {
		if (strncmp(str, "infinity", len) == 0 ||
		    strncmp(str, "all", len) == 0 ||
		    strncmp(str, "maximum", len) == 0) {
			return (Infinity);
		} 
		i = (int)strtonum(str, 0, INT_MAX, &errstr);
		if (errstr) {
			return (Invalid);
		} else
			return (i);
	}
	return (0);
}

/*
 * itoa - convert integer (decimal) to ascii string.
 */
char *
itoa(int val)
{
	static char buffer[16];	/* result is built here */

	/*
	 * 16 is sufficient since the largest number we will ever convert
	 * will be 2^32-1, which is 10 digits.
	 */
	(void)snprintf(buffer, sizeof(buffer), "%d", val);
	return (buffer);
}

/*
 * format_uid(uid) - like itoa, except for uid_t and the number is right
 * justified in a 6 character field to match uname_field in top.c.
 */
char *
format_uid(uid_t uid)
{
	static char buffer[16];	/* result is built here */

	/*
	 * 16 is sufficient since the largest uid we will ever convert
	 * will be 2^32-1, which is 10 digits.
	 */
	(void)snprintf(buffer, sizeof(buffer), "%6u", uid);
	return (buffer);
}

/*
 * digits(val) - return number of decimal digits in val.  Only works for
 * positive numbers.  If val <= 0 then digits(val) == 0.
 */
int
digits(int val)
{
	int cnt = 0;

	while (val > 0) {
		cnt++;
		val /= 10;
	}
	return (cnt);
}

/*
 * string_index(string, array) - find string in array and return index
 */
int
string_index(char *string, char **array)
{
	int i = 0;

	while (*array != NULL) {
		if (strncmp(string, *array, strlen(string)) == 0)
			return (i);
		array++;
		i++;
	}
	return (-1);
}

/*
 * argparse(line, cntp) - parse arguments in string "line", separating them
 * out into an argv-like array, and setting *cntp to the number of
 * arguments encountered.  This is a simple parser that doesn't understand
 * squat about quotes.
 */
char **
argparse(char *line, int *cntp)
{
	char **argv, **argarray, *args, *from, *to;
	int cnt, ch, length, lastch;

	/*
	 * unfortunately, the only real way to do this is to go thru the
	 * input string twice.
	 */

	/* step thru the string counting the white space sections */
	from = line;
	lastch = cnt = length = 0;
	while ((ch = *from++) != '\0') {
		length++;
		if (ch == ' ' && lastch != ' ')
			cnt++;
		lastch = ch;
	}

	/*
	 * add three to the count:  one for the initial "dummy" argument, one
	 * for the last argument and one for NULL
	 */
	cnt += 3;

	/* allocate a char * array to hold the pointers */
	if ((argarray = calloc(cnt, sizeof(char *))) == NULL)
		err(1, NULL);

	/* allocate another array to hold the strings themselves */
	if ((args = malloc(length + 2)) == NULL)
		err(1, NULL);

	/* initialization for main loop */
	from = line;
	to = args;
	argv = argarray;
	lastch = '\0';

	/* create a dummy argument to keep getopt happy */
	*argv++ = to;
	*to++ = '\0';
	cnt = 2;

	/* now build argv while copying characters */
	*argv++ = to;
	while ((ch = *from++) != '\0') {
		if (ch != ' ') {
			if (lastch == ' ') {
				*to++ = '\0';
				*argv++ = to;
				cnt++;
			}
			*to++ = ch;
		}
		lastch = ch;
	}
	*to++ = '\0';

	/* set cntp and return the allocated array */
	*cntp = cnt;
	return (argarray);
}

/*
 * percentages(cnt, out, new, old, diffs) - calculate percentage change
 * between array "old" and "new", putting the percentages in "out".
 * "cnt" is size of each array and "diffs" is used for scratch space.
 * The array "old" is updated on each call.
 * The routine assumes modulo arithmetic.  This function is especially
 * useful on BSD machines for calculating cpu state percentages.
 */
int
percentages(int cnt, int64_t *out, int64_t *new, int64_t *old, int64_t *diffs)
{
	int64_t change, total_change, *dp, half_total;
	int i;

	/* initialization */
	total_change = 0;
	dp = diffs;

	/* calculate changes for each state and the overall change */
	for (i = 0; i < cnt; i++) {
		if ((change = *new - *old) < 0) {
			/* this only happens when the counter wraps */
			change = INT64_MAX - *old + *new;
		}
		total_change += (*dp++ = change);
		*old++ = *new++;
	}

	/* avoid divide by zero potential */
	if (total_change == 0)
		total_change = 1;

	/* calculate percentages based on overall change, rounding up */
	half_total = total_change / 2l;
	for (i = 0; i < cnt; i++)
		*out++ = ((*diffs++ * 1000 + half_total) / total_change);

	/* return the total in case the caller wants to use it */
	return (total_change);
}

/*
 * format_time(seconds) - format number of seconds into a suitable display
 * that will fit within 6 characters.  Note that this routine builds its
 * string in a static area.  If it needs to be called more than once without
 * overwriting previous data, then we will need to adopt a technique similar
 * to the one used for format_k.
 */

/*
 * Explanation: We want to keep the output within 6 characters.  For low
 * values we use the format mm:ss.  For values that exceed 999:59, we switch
 * to a format that displays hours and fractions:  hhh.tH.  For values that
 * exceed 999.9, we use hhhh.t and drop the "H" designator.  For values that
 * exceed 9999.9, we use "???".
 */

char *
format_time(time_t seconds)
{
	static char result[10];

	/* sanity protection */
	if (seconds < 0 || seconds > (99999l * 360l)) {
		strlcpy(result, "   ???", sizeof result);
	} else if (seconds >= (1000l * 60l)) {
		/* alternate (slow) method displaying hours and tenths */
		snprintf(result, sizeof(result), "%5.1fH",
		    (double) seconds / (double) (60l * 60l));

		/*
		 * It is possible that the snprintf took more than 6
		 * characters. If so, then the "H" appears as result[6].  If
		 * not, then there is a \0 in result[6].  Either way, it is
		 * safe to step on.
		 */
		result[6] = '\0';
	} else {
		/* standard method produces MMM:SS */
		/* we avoid printf as must as possible to make this quick */
		snprintf(result, sizeof(result), "%3d:%02d", seconds / 60,
		    seconds % 60);
	}
	return (result);
}

/*
 * format_k(amt) - format a kilobyte memory value, returning a string
 * suitable for display.  Returns a pointer to a static
 * area that changes each call.  "amt" is converted to a
 * string with a trailing "K".  If "amt" is 10000 or greater,
 * then it is formatted as megabytes (rounded) with a
 * trailing "M".
 */

/*
 * Compromise time.  We need to return a string, but we don't want the
 * caller to have to worry about freeing a dynamically allocated string.
 * Unfortunately, we can't just return a pointer to a static area as one
 * of the common uses of this function is in a large call to snprintf where
 * it might get invoked several times.  Our compromise is to maintain an
 * array of strings and cycle thru them with each invocation.  We make the
 * array large enough to handle the above mentioned case.  The constant
 * NUM_STRINGS defines the number of strings in this array:  we can tolerate
 * up to NUM_STRINGS calls before we start overwriting old information.
 * Keeping NUM_STRINGS a power of two will allow an intelligent optimizer
 * to convert the modulo operation into something quicker.  What a hack!
 */

#define NUM_STRINGS 8

char *
format_k(int amt)
{
	static char retarray[NUM_STRINGS][16];
	static int  idx = 0;
	char *ret, tag = 'K';

	ret = retarray[idx];
	idx = (idx + 1) % NUM_STRINGS;

	if (amt >= 10000) {
		amt = (amt + 512) / 1024;
		tag = 'M';
		if (amt >= 10000) {
			amt = (amt + 512) / 1024;
			tag = 'G';
		}
	}
	snprintf(ret, sizeof(retarray[0]), "%d%c", amt, tag);
	return (ret);
}

int
find_pid(pid_t pid)
{
	struct kinfo_proc *pbase, *cur;
	int nproc;

	if ((pbase = getprocs(KERN_PROC_KTHREAD, 0, &nproc)) == NULL)
		quit(23);

	for (cur = pbase; cur < &pbase[nproc]; cur++)
		if (cur->p_pid == pid)
			return 1;
	return 0;
}
