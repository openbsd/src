/*	$OpenBSD: rwall.c,v 1.13 2015/01/16 06:40:11 deraadt Exp $	*/

/*
 * Copyright (c) 1993 Christopher G. Demetriou
 * Copyright (c) 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * This program is not related to David Wall, whose Stanford Ph.D. thesis
 * is entitled "Mechanisms for Broadcast and Selective Broadcast".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <paths.h>
#include <err.h>

#include <rpc/rpc.h>
#include <rpcsvc/rwall.h>

struct timeval timeout = { 25, 0 };
int mbufsize;
char *mbuf;

void makemsg(char *);

int
main(int argc, char *argv[])
{
	extern char *__progname;
	char *wallhost, res;
	CLIENT *cl;

	if ((argc < 2) || (argc > 3)) {
		fprintf(stderr, "usage: %s host [file]\n", __progname);
		exit(1);
	}

	wallhost = argv[1];

	makemsg(argv[2]);

	/*
	 * Create client "handle" used for calling MESSAGEPROG on the
	 * server designated on the command line. We tell the rpc package
	 * to use the "tcp" protocol when contacting the server.
	*/
	cl = clnt_create(wallhost, WALLPROG, WALLVERS, "udp");
	if (cl == NULL) {
		/*
		 * Couldn't establish connection with server.
		 * Print error message and die.
		 */
		clnt_pcreateerror(wallhost);
		exit(1);
	}

	if (clnt_call(cl, WALLPROC_WALL, xdr_wrapstring, &mbuf, xdr_void,
	    &res, timeout) != RPC_SUCCESS) {
		/*
		 * An error occurred while calling the server.
		 * Print error message and die.
		 */
		clnt_perror(cl, wallhost);
		exit(1);
	}

	exit(0);
}

void
makemsg(char *fname)
{
	struct tm *lt;
	struct passwd *pw;
	struct stat sbuf;
	time_t now;
	FILE *fp;
	int fd;
	char *whom, hostname[HOST_NAME_MAX+1], lbuf[100], tmpname[PATH_MAX];

	snprintf(tmpname, sizeof(tmpname), "%s/wall.XXXXXXXXXX", _PATH_TMP);
	if ((fd = mkstemp(tmpname)) == -1 || !(fp = fdopen(fd, "r+")))
		err(1, "can't open temporary file");
	(void)unlink(tmpname);

	if (!(whom = getlogin()))
		whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";
	(void)gethostname(hostname, sizeof(hostname));
	(void)time(&now);
	lt = localtime(&now);

	/*
	 * all this stuff is to blank out a square for the message;
	 * we wrap message lines at column 79, not 80, because some
	 * terminals wrap after 79, some do not, and we can't tell.
	 * Which means that we may leave a non-blank character
	 * in column 80, but that can't be helped.
	 */
	(void)fprintf(fp, "Remote Broadcast Message from %s@%s\n",
	    whom, hostname);
	(void)fprintf(fp, "        (%s) at %d:%02d ...\n", ttyname(2),
	    lt->tm_hour, lt->tm_min);

	putc('\n', fp);

	if (fname && !(freopen(fname, "r", stdin)))
		err(1, "%s", fname);
	while (fgets(lbuf, sizeof(lbuf), stdin))
		fputs(lbuf, fp);
	rewind(fp);

	if (fstat(fd, &sbuf))
		err(1, "can't stat temporary file");
	mbufsize = sbuf.st_size;
	if (!(mbuf = malloc((u_int)mbufsize)))
		err(1, "malloc");
	if (fread(mbuf, sizeof(*mbuf), mbufsize, fp) != mbufsize)
		err(1, "can't read temporary file");
	(void)close(fd);
}
