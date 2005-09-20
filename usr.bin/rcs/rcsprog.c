/*	$OpenBSD: rcsprog.c,v 1.6 2005/09/20 05:01:31 joris Exp $	*/
/*
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "rcs.h"
#include "strtab.h"

extern char *__progname;


const char rcs_version[] = "OpenCVS RCS version 3.6";

void  rcs_usage(void);
int   rcs_main(int, char **);
void (*usage)(void);

struct rcs_prog {
	char  *prog_name;
	int  (*prog_hdlr)(int, char **);
	void (*prog_usage)(void);
} programs[] = {
	{ "rcs",	rcs_main,	rcs_usage	},
	{ "ci",		NULL,		NULL		},
	{ "co",		NULL,		NULL		},
	{ "rcsclean",	NULL,		NULL		},
	{ "rcsdiff",	NULL,		NULL		},
};


int
main(int argc, char **argv)
{
	u_int i;
	int ret;

	ret = -1;
	cvs_strtab_init();

	for (i = 0; i < (sizeof(programs)/sizeof(programs[0])); i++)
		if (strcmp(__progname, programs[i].prog_name) == 0) {
			usage = programs[i].prog_usage;
			ret = programs[i].prog_hdlr(argc, argv);
			break;
		}

	cvs_strtab_cleanup();

	return (ret);
}


void
rcs_usage(void)
{
	fprintf(stderr,
	    "usage: %s [-hiLMUV] [-a users] [-b [rev]] [-c string] "
	    "[-e users] [-k opt] file ...\n", __progname);
}


/*
 * rcs_main()
 *
 * Handler for the `rcs' program.
 * Returns 0 on success, or >0 on error.
 */
int
rcs_main(int argc, char **argv)
{
	int i, ch, flags, kflag, lkmode;
	char fpath[MAXPATHLEN];
	char *oldfile, *alist, *comment, *elist, *unp, *sp;
	mode_t fmode;
	RCSFILE *file;

	kflag = lkmode = -1;
	fmode = 0;
	flags = RCS_RDWR;
	oldfile = alist = comment = elist = NULL;

	cvs_log_init(LD_STD, 0);

	while ((ch = getopt(argc, argv, "A:a:b::c:e::hik:LMUV")) != -1) {
		switch (ch) {
		case 'A':
			oldfile = optarg;
			break;
		case 'a':
			alist = optarg;
			break;
		case 'c':
			comment = optarg;
			break;
		case 'e':
			elist = optarg;
			break;
		case 'h':
			(usage)();
			exit(0);
		case 'i':
			flags |= RCS_CREATE;
			break;
		case 'k':
			kflag = rcs_kflag_get(optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid keyword substitution mode `%s'",
				    optarg);
				exit(1);
			}
			break;
		case 'L':
			if (lkmode == RCS_LOCK_LOOSE)
				cvs_log(LP_WARN, "-U overriden by -L");
			lkmode = RCS_LOCK_STRICT;
			break;
		case 'M':
			/* ignore for the moment */
			break;
		case 'U':
			if (lkmode == RCS_LOCK_STRICT)
				cvs_log(LP_WARN, "-L overriden by -U");
			lkmode = RCS_LOCK_LOOSE;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		/*
		 * Our RCS API does not append the RCS_FILE_EXT extension
		 * automaticly in rcs_open(), so we add it here.
		 */
		snprintf(fpath, sizeof(fpath), "%s%s", argv[i], RCS_FILE_EXT);

		printf("RCS file: %s\n", fpath);
		file = rcs_open(fpath, flags, fmode);
		if (file == NULL)
			continue;

		/* entries to add to the access list */
		if (alist != NULL) {
			unp = alist;
			do {
				sp = strchr(unp, ',');
				if (sp != NULL)
					*(sp++) = '\0';

				rcs_access_add(file, unp);

				unp = sp;
			} while (sp != NULL);
		}

		if (comment != NULL)
			rcs_comment_set(file, comment);

		if (kflag != -1)
			rcs_kwexp_set(file, kflag);

		if (lkmode != -1)
			rcs_lock_setmode(file, lkmode);

		rcs_close(file);
		printf("done\n");
	}

	return (0);
}
