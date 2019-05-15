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

#define	IS_UTF8_TRAIL(c)	(((c) & 0xC0) == 0x80)
#define	IS_UTF8_INVALID(c)	(((c) & 0xFE) == 0xFE)
#define	IS_UTF8_LEAD(c)		(((c) & 0xC0) == 0xC0 && !IS_UTF8_INVALID(c))
