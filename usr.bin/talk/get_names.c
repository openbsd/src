/*	$OpenBSD: get_names.c,v 1.6 1998/08/18 04:02:13 millert Exp $	*/
/*	$NetBSD: get_names.c,v 1.4 1994/12/09 02:14:16 jtc Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)get_names.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: get_names.c,v 1.6 1998/08/18 04:02:13 millert Exp $";
#endif /* not lint */

#include "talk.h"
#include <sys/param.h>
#include <pwd.h>
#include <unistd.h>

extern	CTL_MSG msg;

/*
 * Determine the local and remote user, tty, and machines
 */
void
get_names(argc, argv)
	int argc;
	char *argv[];
{
	char hostname[MAXHOSTNAMELEN];
	char *his_name, *my_name;
	char *my_machine_name, *his_machine_name;
	char *his_tty;
	register char *cp;
	char *names;

	if ((argc < 2 ) || ('@' == argv[1][0])) {
		fprintf(stderr, "usage: talk user [ttyname]\n"
				"       talk user@hostname [ttyname]\n");
		exit(-1);
	}
	if (!isatty(0))
		errx(1, "standard input must be a tty, not a pipe or a file");

	if ((my_name = getlogin()) == NULL) {
		struct passwd *pw;

		if ((pw = getpwuid(getuid())) == NULL)
			errx(1, "you don't exist in the passwd file.");
		my_name = pw->pw_name;
	}
	gethostname(hostname, sizeof (hostname));
	my_machine_name = hostname;
	/* check for, and strip out, the machine name of the target */
	names = strdup(argv[1]);
	for (cp = names; *cp && !strchr("@:!.", *cp); cp++)
		;
	if (*cp == '\0') {
		/* this is a local to local talk */
		his_name = names;
		his_machine_name = my_machine_name;
	} else {
		if (*cp++ == '@') {
			/* user@host */
			his_name = names;
			his_machine_name = cp;
		} else {
			/* host.user or host!user or host:user */
			his_name = cp;
			his_machine_name = names;
		}
		*--cp = '\0';
	}
	if (argc > 2)
		his_tty = argv[2];	/* tty name is arg 2 */
	else
		his_tty = "";
	get_addrs(my_machine_name, his_machine_name);
	/*
	 * Initialize the message template.
	 */
	msg.vers = TALK_VERSION;
	msg.addr.sa_family = htons(AF_INET);
	msg.ctl_addr.sa_family = htons(AF_INET);
	msg.id_num = htonl(0);
	strncpy(msg.l_name, my_name, NAME_SIZE);
	msg.l_name[NAME_SIZE - 1] = '\0';
	strncpy(msg.r_name, his_name, NAME_SIZE);
	msg.r_name[NAME_SIZE - 1] = '\0';
	strncpy(msg.r_tty, his_tty, TTY_SIZE);
	msg.r_tty[TTY_SIZE - 1] = '\0';
}
