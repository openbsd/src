/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Operating system dependent routines.
 *
 * Most of the stuff in here is based on Unix, but an attempt
 * has been made to make things work on other operating systems.
 * This will sometimes result in a loss of functionality, unless
 * someone rewrites code specifically for the new operating system.
 *
 * The makefile provides defines to decide whether various
 * Unix features are present.
 */

#include <errno.h>
#include <signal.h>
#include <time.h>

#include "less.h"

extern volatile sig_atomic_t sigs;

/*
 * Like read() system call, but is deliberately interruptible.
 */
int
iread(int fd, unsigned char *buf, unsigned int len)
{
	int n;

start:
	flush(0);
	n = read(fd, buf, len);
	if (n < 0) {
		/*
		 * Certain values of errno indicate we should just retry the
		 * read.
		 */
		if (errno == EINTR)
			return (READ_INTR);
		if (errno == EAGAIN)
			goto start;
		return (-1);
	}
	return (n);
}

/*
 * errno_message: Return an error message based on the value of "errno".
 */
char *
errno_message(char *filename)
{
	return (easprintf("%s: %s", filename, strerror(errno)));
}

static off_t
muldiv(off_t val, off_t num, off_t den)
{
	double v = (((double)val) * num) / den;
	return ((off_t)(v + 0.5));
}

/*
 * Return the ratio of two off_t, as a percentage.
 * {{ Assumes a off_t is a long int. }}
 */
int
percentage(off_t num, off_t den)
{
	return ((int)muldiv(num, (off_t)100, den));
}

/*
 * Return the specified percentage of a off_t.
 */
off_t
percent_pos(off_t pos, int percent, long fraction)
{
	/*
	 * Change percent (parts per 100) to perden
	 * (parts per NUM_FRAC_DENOM).
	 */
	off_t perden = (percent * (NUM_FRAC_DENOM / 100)) + (fraction / 100);

	if (perden == 0)
		return (0);
	return (muldiv(pos, perden, (off_t)NUM_FRAC_DENOM));
}
