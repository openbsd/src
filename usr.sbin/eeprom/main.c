/*	$OpenBSD: main.c,v 1.18 2008/06/26 05:42:21 ray Exp $	*/
/*	$NetBSD: main.c,v 1.3 1996/05/16 16:00:55 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__sparc__) && !defined(__sparc64__)
#include <fcntl.h>
#include <limits.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <machine/eeprom.h>
#endif /* __sparc__ && !__sparc64__ */

#include <machine/openpromio.h>

#include "defs.h"

#if defined(__sparc__) && !defined(__sparc64__)
static	char *nlistf = NULL;

struct	keytabent eekeytab[] = {
	{ "hwupdate",		0x10,	ee_hwupdate },
	{ "memsize",		0x14,	ee_num8 },
	{ "memtest",		0x15,	ee_num8 },
	{ "scrsize",		0x16,	ee_screensize },
	{ "watchdog_reboot",	0x17,	ee_truefalse },
	{ "default_boot",	0x18,	ee_truefalse },
	{ "bootdev",		0x19,	ee_bootdev },
	{ "kbdtype",		0x1e,	ee_kbdtype },
	{ "console",		0x1f,	ee_constype },
	{ "keyclick",		0x21,	ee_truefalse },
	{ "diagdev",		0x22,	ee_bootdev },
	{ "diagpath",		0x28,	ee_diagpath },
	{ "columns",		0x50,	ee_num8 },
	{ "rows",		0x51,	ee_num8 },
	{ "ttya_use_baud",	0x58,	ee_truefalse },
	{ "ttya_baud",		0x59,	ee_num16 },
	{ "ttya_no_rtsdtr",	0x5b,	ee_truefalse },
	{ "ttyb_use_baud",	0x60,	ee_truefalse },
	{ "ttyb_baud",		0x61,	ee_num16 },
	{ "ttyb_no_rtsdtr",	0x63,	ee_truefalse },
	{ "banner",		0x68,	ee_banner },
	{ "secure",		0,	ee_notsupp },
	{ "bad_login",		0,	ee_notsupp },
	{ "password",		0,	ee_notsupp },
	{ NULL,			0,	ee_notsupp },
};
#endif /* __sparc__ && !__sparc64__ */

static	void action(char *);
static	void dump_prom(void);
static	void usage(void);
#if defined(__sparc__) && !defined(__sparc64__)
static	int getcputype(void);
#endif /* __sparc__ && !__sparc64__ */

char	*path_eeprom = "/dev/eeprom";
char	*path_openprom = "/dev/openprom";
int	fix_checksum = 0;
int	ignore_checksum = 0;
int	update_checksums = 0;
int	cksumfail = 0;
u_short	writecount;
int	eval = 0;
int	use_openprom = 0;
int	print_tree = 0;
int	verbose = 0;

extern	char *__progname;

int
main(int argc, char *argv[])
{
	int ch, do_stdin = 0;
	char *cp, line[BUFSIZE];
	gid_t gid;
	char *optstring = "cf:ipvN:-";

	while ((ch = getopt(argc, argv, optstring)) != -1)
		switch (ch) {
		case '-':
			do_stdin = 1;
			break;

		case 'c':
			fix_checksum = 1;
			break;

		case 'f':
			path_eeprom = path_openprom = optarg;
			break;

		case 'i':
			ignore_checksum = 1;
			break;

		case 'p':
			print_tree = 1;
			break;

		case 'v':
			verbose = 1;
			break;

#if defined(__sparc__) && !defined(__sparc64__)
		case 'N':
			nlistf = optarg;
			break;
#endif /* __sparc__ && !__sparc64__ */

		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#if defined(__sparc__) && !defined(__sparc64__)
	if (nlistf != NULL) {
		gid = getgid();
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");
	}
	if (getcputype() != CPU_SUN4)
#endif /* __sparc__ && !__sparc64__ */
		use_openprom = 1;
	if (print_tree && use_openprom) {
		op_tree();
		exit(0);
	}

#if defined(__sparc__) && !defined(__sparc64__)
	if (use_openprom == 0) {
		ee_verifychecksums();
		if (fix_checksum || cksumfail)
			exit(cksumfail);
	}
#endif /* __sparc__ && !__sparc64__ */

	if (do_stdin) {
		while (fgets(line, BUFSIZE, stdin) != NULL) {
			if (line[0] == '\n')
				continue;
			if ((cp = strrchr(line, '\n')) != NULL)
				*cp = '\0';
			action(line);
		}
		if (ferror(stdin))
			err(++eval, "stdin");
	} else {
		if (argc == 0) {
			dump_prom();
			exit(eval + cksumfail);
		}

		while (argc) {
			action(*argv);
			++argv;
			--argc;
		}
	}

#if defined(__sparc__) && !defined(__sparc64__)
	if (use_openprom == 0)
		if (update_checksums) {
			++writecount;
			ee_updatechecksums();
		}
#endif /* __sparc__ && !__sparc64__ */

	exit(eval + cksumfail);
}

#if defined(__sparc__) && !defined(__sparc64__)
static int
getcputype(void)
{
	int mib[2];
	size_t len;
	int cputype;

	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_CPUTYPE;
	len = sizeof(cputype);
	if (sysctl(mib, 2, &cputype, &len, NULL, 0) < 0)
		err(1, "sysctl(machdep.cputype)");

	return (cputype);
}
#endif /* __sparc__ && !__sparc64__ */

/*
 * Separate the keyword from the argument (if any), find the keyword in
 * the table, and call the corresponding handler function.
 */
static void
action(char *line)
{
	char *keyword, *arg, *cp;
	struct keytabent *ktent;

	keyword = strdup(line);
	if (!keyword)
		errx(1, "out of memory");
	if ((arg = strrchr(keyword, '=')) != NULL)
		*arg++ = '\0';

	if (use_openprom) {
		/*
		 * The whole point of the Openprom is that one
		 * isn't required to know the keywords.  With this
		 * in mind, we just dump the whole thing off to
		 * the generic op_handler.
		 */
		if ((cp = op_handler(keyword, arg)) != NULL)
			warnx("%s", cp);
		return;
	}
#if defined(__sparc__) && !defined(__sparc64__)
	  else
		for (ktent = eekeytab; ktent->kt_keyword != NULL; ++ktent) {
			if (strcmp(ktent->kt_keyword, keyword) == 0) {
				(*ktent->kt_handler)(ktent, arg);
				return; 
			}
		}
#endif /* __sparc__ && !__sparc64__ */

	warnx("unknown keyword %s", keyword);
	++eval;
}

/*
 * Dump the contents of the prom corresponding to all known keywords.
 */
static void
dump_prom(void)
{
	struct keytabent *ktent;

	if (use_openprom) {
		/*
		 * We have a special dump routine for this.
		 */
		op_dump();
	}
#if defined(__sparc__) && !defined(__sparc64__)
	  else
		for (ktent = eekeytab; ktent->kt_keyword != NULL; ++ktent)
			(*ktent->kt_handler)(ktent, NULL);
#endif /* __sparc__ && !__sparc64__ */
}

static void
usage(void)
{

#if defined(__sparc__) && !defined(__sparc64__)
	fprintf(stderr,
	    "usage: %s [-cipv] [-f device] [-N system] [field[=value] ...]\n",
	    __progname);
#else
	fprintf(stderr,
	    "usage: %s [-cipv] [-f device] [field[=value] ...]\n",
	    __progname);
#endif /* __sparc__ && !__sparc64__ */
	exit(1);
}
