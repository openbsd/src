/* $OpenBSD: main.c,v 1.1 1999/09/27 21:40:04 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "stand.h"
#include "gzip.h"
#include "extern.h"

#ifdef __OpenBSD__
extern char *__progname;
#define argv0	__progname
#else
static char *argv0;
#endif

#define NM_SIGN	"pkg_sign"

static void 
usage()
{
	fprintf(stderr, "usage: %s [-sc] [-u userid] pkg1 ...\n", argv0);
	exit(EXIT_FAILURE);
}

#define SIGN 0
#define CHECK 1

static int
check(filename, userid, envp)
	/*@observer@*/const char *filename;
	/*@null@*/const char *userid;
	char *envp[];
{
	int result;
	FILE *file;

	if (strcmp(filename, "-") == 0)
		return check_signature(stdin, userid, envp, "stdin");
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Can't open %s\n", filename);
		return 0;
	}
	result = check_signature(file, userid, envp, filename);
	if (fclose(file) == 0)
		return result;
	else
		return 0;
}

int 
main(argc, argv, envp)
	int argc; 
	char *argv[];
	char *envp[];
{
	int success = 1;
	int ch;
	char *userid = NULL;
	int mode;
	int i;

#ifdef CHECKER_ONLY
	mode = CHECK;
#else
#ifndef __OpenBSD__
	if ((argv0 = strrchr(argv[0], '/')) != NULL)
		argv0++;
	else
		argv0 = argv[0];
#endif
	if (strcmp(argv0, NM_SIGN) == 0)
		mode = SIGN;
	else
		mode = CHECK;
#endif

	if (check_helpers() == 0)
		exit(EXIT_FAILURE);
	while ((ch = getopt(argc, argv, "u:sc")) != -1) {
		switch(ch) {
		case 'u':
			userid = strdup(optarg);
			break;
#ifndef CHECKER_ONLY
		case 's':
			mode = SIGN;
			break;
#endif
		case 'c':
			mode = CHECK;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		if (mode == CHECK)
			success &= check("-", userid, envp);
		else
			usage();
	}
	
#ifndef CHECKER_ONLY
	if (mode == SIGN)
		handle_passphrase();
#endif
	for (i = 0; i < argc; i++)
		success &= (mode == SIGN ? sign : check)(argv[i], userid, envp);
	exit(success == 1 ? EXIT_SUCCESS : EXIT_FAILURE);
}
