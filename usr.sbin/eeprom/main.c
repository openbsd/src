/*	$OpenBSD: main.c,v 1.10 2002/12/08 16:50:07 millert Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <unistd.h>

#ifdef __sparc__
#include <fcntl.h>
#include <limits.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>

#include <machine/openpromio.h>

static	char *system = NULL;
#endif /* __sparc__ */

#include <machine/eeprom.h>

#include "defs.h"

struct	keytabent eekeytab[] = {
	{ "hwupdate",		0x10,	ee_hwupdate },
	{ "memsize",		0x14,	ee_num8 },
	{ "memtest",		0x15,	ee_num8 },
#ifdef __sparc64__
	{ "scrsize",		0x16,	ee_notsupp },
#else
	{ "scrsize",		0x16,	ee_screensize },
#endif
	{ "watchdog_reboot",	0x17,	ee_truefalse },
	{ "default_boot",	0x18,	ee_truefalse },
	{ "bootdev",		0x19,	ee_bootdev },
	{ "kbdtype",		0x1e,	ee_kbdtype },
#ifdef __sparc64__
	{ "console",		0x1f,	ee_notsupp },
#else
	{ "console",		0x1f,	ee_constype },
#endif
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

static	void action(char *);
static	void dump_prom(void);
static	void usage(void);
#ifdef __sparc__
static	int getcputype(void);
#endif /* __sparc__ */

char	*path_eeprom = "/dev/eeprom";
char	*path_openprom = "/dev/openprom";
int	fix_checksum = 0;
int	ignore_checksum = 0;
int	update_checksums = 0;
int	cksumfail = 0;
u_short	writecount;
int	eval = 0;
int	use_openprom = 0;
int	verbose = 0;

extern	char *__progname;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, do_stdin = 0;
	char *cp, line[BUFSIZE];
#ifdef __sparc__
	char *optstring = "cf:ivN:-";
#else
	char *optstring = "cf:i-";
#endif /* __sparc__ */

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
#ifdef __sparc__
		case 'v':
			verbose = 1;
			break;

		case 'N':
			system = optarg;
			break;

#endif /* __sparc__ */

		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#ifdef __sparc__
	if (system != NULL) {
		setegid(getgid());
		setgid(getgid());
	}
	if (getcputype() != CPU_SUN4)
		use_openprom = 1;
#endif /* __sparc__ */

	if (use_openprom == 0) {
		ee_verifychecksums();
		if (fix_checksum || cksumfail)
			exit(cksumfail);
	}

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

	if (use_openprom == 0)
		if (update_checksums) {
			++writecount;
			ee_updatechecksums();
		}

	exit(eval + cksumfail);
}

#ifdef __sparc__
static int
getcputype()
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
#endif /* __sparc__ */

/*
 * Separate the keyword from the argument (if any), find the keyword in
 * the table, and call the corresponding handler function.
 */
static void
action(line)
	char *line;
{
	char *keyword, *arg, *cp;
	struct keytabent *ktent;

	keyword = strdup(line);
	if ((arg = strrchr(keyword, '=')) != NULL)
		*arg++ = '\0';

#ifdef __sparc__
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
	} else
#endif /* __sparc__ */
		for (ktent = eekeytab; ktent->kt_keyword != NULL; ++ktent) {
			if (strcmp(ktent->kt_keyword, keyword) == 0) {
				(*ktent->kt_handler)(ktent, arg);
				return; 
			}
		}

	warnx("unknown keyword %s", keyword);
	++eval;
}

/*
 * Dump the contents of the prom corresponding to all known keywords.
 */
static void
dump_prom()
{
	struct keytabent *ktent;

#ifdef __sparc__
	if (use_openprom) {
		/*
		 * We have a special dump routine for this.
		 */
		op_dump();
	} else
#endif /* __sparc__ */
		for (ktent = eekeytab; ktent->kt_keyword != NULL; ++ktent)
			(*ktent->kt_handler)(ktent, NULL);
}

static void
usage()
{

#ifdef __sparc__
	fprintf(stderr, "usage: %s %s %s\n", __progname,
	    "[-] [-c] [-f device] [-i] [-v]",
	    "[-N system] [field[=value] ...]");
#else
	fprintf(stderr, "usage: %s %s\n", __progname,
	    "[-] [-c] [-f device] [-i] [field[=value] ...]");
#endif /* __sparc__ */
	exit(1);
}
