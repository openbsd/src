/*
 * Copyright (C) 1984-2012  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */
/*
 * Modified for use with illumos.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 */

#include <regex.h>
#define	REGCOMP_FLAG	REG_EXTENDED
#define	DEFINE_PATTERN(name)	regex_t *name
#define	CLEAR_PATTERN(name)	name = NULL
