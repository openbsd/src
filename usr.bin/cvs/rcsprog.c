/*	$OpenBSD: rcsprog.c,v 1.2 2005/03/05 23:22:10 jmc Exp $	*/
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

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sysexits.h>

#include "log.h"
#include "rcs.h"


extern char *__progname;


const char rcs_version[] = "OpenCVS RCS version 3.6";

void  rcs_usage (void);
int   rcs_main  (int, char **);



struct rcs_prog {
	char  *prog_name;
	int  (*prog_hdlr)(int, char **);
} programs[] = {
	{ "rcs",        rcs_main  },
	{ "ci",         NULL      },
	{ "co",         NULL      },
	{ "rcsclean",   NULL      },
	{ "rcsdiff",    NULL      },
};


int
main(int argc, char **argv)
{
	u_int i;

	for (i = 0; i < (sizeof(programs)/sizeof(programs[0])); i++)
		if (strcmp(__progname, programs[i].prog_name) == 0)
			return (*programs[i].prog_hdlr)(argc, argv);

	errx(1, "not too sure what you expect me to do!");

	return (0);
}


void
rcs_usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-hiLMUV] [-a users] [-b [rev]] [-c string] "
	    "[-e users] [-k opt] file ...\n"
	    "\t-a users\tAdd the login names in the comma-separated <users>\n"
	    "\t-b rev\t\tSet the head revision to <rev>\n"
	    "\t-c string\tSet the comment leader to <string>\n"
	    "\t-e users\tRemove the login names in the comma-separated <users>\n"
	    "\t-h\t\tPrint the program's usage and exit\n"
	    "\t-i\t\tCreate a new empty RCS file\n"
	    "\t-k opt\t\tSet the keyword expansion mode to <opt>\n"
	    "\t-L\t\tEnable strict locking on the specified files\n"
	    "\t-M\t\tDisable mail warning about lock breaks\n"
	    "\t-U\t\tDisable strict locking on the specified files\n"
	    "\t-V\t\tPrint the program's version string and exit\n",
	    __progname);
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
	char *oldfile, *alist, *comment, *elist, *unp, *sp;
	mode_t fmode;
	RCSFILE *file;

	kflag = lkmode = -1;
	fmode = 0;
	flags = RCS_READ;
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
			rcs_usage();
			exit(0);
		case 'i':
			flags |= (RCS_WRITE | RCS_CREATE);
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
			rcs_usage();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		printf("RCS file: %s\n", argv[0]);
		file = rcs_open(argv[0], flags, fmode);
		if (file == NULL) {
			return (1);
		}

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
