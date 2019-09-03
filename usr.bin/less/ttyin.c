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
 * Routines dealing with getting input from the keyboard (i.e. from the user).
 */

#include "less.h"

int tty;
extern int utf_mode;

/*
 * Open keyboard for input.
 */
void
open_getchr(void)
{
	/*
	 * Try /dev/tty.
	 * If that doesn't work, use file descriptor 2,
	 * which in Unix is usually attached to the screen,
	 * but also usually lets you read from the keyboard.
	 */
	tty = open("/dev/tty", O_RDONLY);
	if (tty == -1)
		tty = STDERR_FILENO;
}

/*
 * Get a character from the keyboard.
 */
int
getchr(void)
{
	unsigned char c;
	int result;

	do {
		result = iread(tty, &c, sizeof (char));
		if (result == READ_INTR)
			return (READ_INTR);
		if (result < 0) {
			/*
			 * Don't call error() here,
			 * because error calls getchr!
			 */
			quit(QUIT_ERROR);
		}
		/*
		 * Various parts of the program cannot handle
		 * an input character of '\0'.
		 * If a '\0' was actually typed, convert it to '\340' here.
		 */
		if (c == '\0')
			c = 0340;
	} while (result != 1);

	return (c & 0xFF);
}
