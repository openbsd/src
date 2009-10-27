/*	$OpenBSD: getconf.c,v 1.12 2009/10/27 23:59:38 deraadt Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * POSIX.2 getconf utility
 *
 * Written by:
 *	J.T. Conklin (jtc@wimsey.com), Winning Strategies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

static void usage(void);

struct conf_variable
{
  const char *name;
  enum { SYSCONF, CONFSTR, PATHCONF, CONSTANT } type;
  long value;
};

const struct conf_variable conf_table[] =
{
  { "PATH",			CONFSTR,	_CS_PATH		},

  /* Utility Limit Minimum Values */
  { "POSIX2_BC_BASE_MAX",	CONSTANT,	_POSIX2_BC_BASE_MAX	},
  { "POSIX2_BC_DIM_MAX",	CONSTANT,	_POSIX2_BC_DIM_MAX	},
  { "POSIX2_BC_SCALE_MAX",	CONSTANT,	_POSIX2_BC_SCALE_MAX	},
  { "POSIX2_BC_STRING_MAX",	CONSTANT,	_POSIX2_BC_STRING_MAX	},
  { "POSIX2_COLL_WEIGHTS_MAX",	CONSTANT,	_POSIX2_COLL_WEIGHTS_MAX },
  { "POSIX2_EXPR_NEST_MAX",	CONSTANT,	_POSIX2_EXPR_NEST_MAX	},
  { "POSIX2_LINE_MAX",		CONSTANT,	_POSIX2_LINE_MAX	},
  { "POSIX2_RE_DUP_MAX",	CONSTANT,	_POSIX2_RE_DUP_MAX	},
  { "POSIX2_VERSION",		CONSTANT,	_POSIX2_VERSION		},

  /* POSIX.1 Minimum Values */
  { "_POSIX_ARG_MAX",		CONSTANT,	_POSIX_ARG_MAX		},
  { "_POSIX_CHILD_MAX",		CONSTANT,	_POSIX_CHILD_MAX	},
  { "_POSIX_LINK_MAX",		CONSTANT,	_POSIX_LINK_MAX		},
  { "_POSIX_MAX_CANON",		CONSTANT,	_POSIX_MAX_CANON	},
  { "_POSIX_MAX_INPUT",		CONSTANT,	_POSIX_MAX_INPUT	},
  { "_POSIX_NAME_MAX",		CONSTANT,	_POSIX_NAME_MAX		},
  { "_POSIX_NGROUPS_MAX",	CONSTANT,	_POSIX_NGROUPS_MAX	},
  { "_POSIX_OPEN_MAX",		CONSTANT,	_POSIX_OPEN_MAX		},
  { "_POSIX_PATH_MAX",		CONSTANT,	_POSIX_PATH_MAX		},
  { "_POSIX_PIPE_BUF",		CONSTANT,	_POSIX_PIPE_BUF		},
  { "_POSIX_SSIZE_MAX",		CONSTANT,	_POSIX_SSIZE_MAX	},
  { "_POSIX_STREAM_MAX",	CONSTANT,	_POSIX_STREAM_MAX	},
  { "_POSIX_TZNAME_MAX",	CONSTANT,	_POSIX_TZNAME_MAX	},

  /* Symbolic Utility Limits */
  { "BC_BASE_MAX",		SYSCONF,	_SC_BC_BASE_MAX		},
  { "BC_DIM_MAX",		SYSCONF,	_SC_BC_DIM_MAX		},
  { "BC_SCALE_MAX",		SYSCONF,	_SC_BC_SCALE_MAX	},
  { "BC_STRING_MAX",		SYSCONF,	_SC_BC_STRING_MAX	},
  { "COLL_WEIGHTS_MAX",		SYSCONF,	_SC_COLL_WEIGHTS_MAX	},
  { "EXPR_NEST_MAX",		SYSCONF,	_SC_EXPR_NEST_MAX	},
  { "LINE_MAX",			SYSCONF,	_SC_LINE_MAX		},
  { "RE_DUP_MAX",		SYSCONF,	_SC_RE_DUP_MAX		},

  /* Optional Facility Configuration Values */
#if 0
  { "POSIX2_C_BIND",		SYSCONF,	???			},
#endif
  { "POSIX2_C_DEV",		SYSCONF,	_SC_2_C_DEV		},
  { "POSIX2_CHAR_TERM",		SYSCONF,	_SC_2_CHAR_TERM		},
  { "POSIX2_FORT_DEV",		SYSCONF,	_SC_2_FORT_DEV		},
  { "POSIX2_FORT_RUN",		SYSCONF,	_SC_2_FORT_RUN		},
  { "POSIX2_LOCALEDEF",		SYSCONF,	_SC_2_LOCALEDEF		},
  { "POSIX2_SW_DEV",		SYSCONF,	_SC_2_SW_DEV		},
  { "POSIX2_UPE",		SYSCONF,	_SC_2_UPE		},

  /* POSIX.1 Configurable System Variables */
  { "ARG_MAX",			SYSCONF,	_SC_ARG_MAX 		},
  { "CHILD_MAX",		SYSCONF,	_SC_CHILD_MAX		},
  { "CLK_TCK",			SYSCONF,	_SC_CLK_TCK		},
  { "NGROUPS_MAX",		SYSCONF,	_SC_NGROUPS_MAX		},
  { "OPEN_MAX",			SYSCONF,	_SC_OPEN_MAX		},
  { "STREAM_MAX",		SYSCONF,	_SC_STREAM_MAX		},
  { "TZNAME_MAX",		SYSCONF,	_SC_TZNAME_MAX		},
  { "_POSIX_JOB_CONTROL",	SYSCONF,	_SC_JOB_CONTROL 	},
  { "_POSIX_SAVED_IDS",		SYSCONF,	_SC_SAVED_IDS		},
  { "_POSIX_VERSION",		SYSCONF,	_SC_VERSION		},

  { "LINK_MAX",			PATHCONF,	_PC_LINK_MAX		},
  { "MAX_CANON",		PATHCONF,	_PC_MAX_CANON		},
  { "MAX_INPUT",		PATHCONF,	_PC_MAX_INPUT		},
  { "NAME_MAX",			PATHCONF,	_PC_NAME_MAX		},
  { "PATH_MAX",			PATHCONF,	_PC_PATH_MAX		},
  { "PIPE_BUF",			PATHCONF,	_PC_PIPE_BUF		},
  { "_POSIX_CHOWN_RESTRICTED",	PATHCONF,	_PC_CHOWN_RESTRICTED	},
  { "_POSIX_NO_TRUNC",		PATHCONF,	_PC_NO_TRUNC		},
  { "_POSIX_VDISABLE",		PATHCONF,	_PC_VDISABLE		},

  { NULL }
};


int
main(int argc, char *argv[])
{
	int ch;
	const struct conf_variable *cp;

	long val;
	size_t slen;
	char * sval;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1 || argc > 2) {
		usage();
		/* NOTREACHED */
	}

	for (cp = conf_table; cp->name != NULL; cp++) {
		if (strcmp(*argv, cp->name) == 0)
			break;
	}
	if (cp->name == NULL) {
		errx(1, "%s: unknown variable", *argv);
		/* NOTREACHED */
	}

	if (cp->type == PATHCONF) {
		if (argc != 2) usage();
	} else {
		if (argc != 1) usage();
	}

	switch (cp->type) {
	case CONSTANT:
		printf("%ld\n", cp->value);
		break;

	case CONFSTR:
		errno = 0;
		slen = confstr (cp->value, (char *) 0, (size_t) 0);
		
		if (slen == 0 || slen == (size_t)-1) {
			if (errno)
				err(1, "%ld", cp->value);
			else
				errx(1, "%ld", cp->value);
		}
		if ((sval = malloc(slen)) == NULL)
			err(1, NULL);

		confstr(cp->value, sval, slen);
		printf("%s\n", sval);
		break;

	case SYSCONF:
		errno = 0;
		if ((val = sysconf(cp->value)) == -1) {
			if (errno != 0) {
				err(1, NULL);
				/* NOTREACHED */
			}

			printf ("undefined\n");
		} else {
			printf("%ld\n", val);
		}
		break;

	case PATHCONF:
		errno = 0;
		if ((val = pathconf(argv[1], cp->value)) == -1) {
			if (errno != 0) {
				err(1, "%s", argv[1]);
				/* NOTREACHED */
			}

			printf ("undefined\n");
		} else {
			printf ("%ld\n", val);
		}
		break;
	}

	exit (ferror(stdout));
}


static void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s name [pathname]\n", __progname);
	exit(1);
}
