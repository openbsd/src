/*	$OpenBSD: exec.c,v 1.8 2003/09/05 19:40:42 tedu Exp $	*/

#ifndef lint
static const char rcsid[] = "$OpenBSD: exec.c,v 1.8 2003/09/05 19:40:42 tedu Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous system routines.
 *
 */

#include <err.h>
#include "lib.h"

/*
 * Unusual system() substitute.  Accepts format string and args,
 * builds and executes command.  Returns exit code.
 */

int
vsystem(const char *fmt, ...)
{
	va_list	args;
	char	*cmd;
	size_t	maxargs;
	int	ret;

	maxargs = (size_t) sysconf(_SC_ARG_MAX);
	if ((long)maxargs == -1) {
		pwarnx("vsystem can't retrieve max args");
		return 1;
	}
	maxargs -= 32;			/* some slop for the sh -c */
	if ((cmd = (char *) malloc(maxargs)) == (char *) NULL) {
		pwarnx("vsystem can't alloc arg space");
		return 1;
	}

	va_start(args, fmt);
	if (vsnprintf(cmd, maxargs, fmt, args) >= maxargs) {
		pwarnx("vsystem args are too long");
		free(cmd);
		return 1;
	}
#ifdef DEBUG
	printf("Executing %s\n", cmd);
#endif
	ret = system(cmd);
	va_end(args);
	free(cmd);
	return ret;
}

