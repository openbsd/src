/*	$OpenBSD: rmt.c,v 1.10 2002/11/08 05:07:34 millert Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)rmt.c	5.6 (Berkeley) 6/1/90";*/
static char rcsid[] = "$Id: rmt.c,v 1.10 2002/11/08 05:07:34 millert Exp $";
#endif /* not lint */

/*
 * rmt
 */
#include <stdio.h>
#include <sgtty.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mtio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int	tape = -1;

char	*record;
int	maxrecsize = -1;

#define	STRSIZE	64
char	device[STRSIZE];
char	count[STRSIZE], mode[STRSIZE], pos[STRSIZE], op[STRSIZE];

char	resp[BUFSIZ];

FILE	*debug;
#define	DEBUG(f)	if (debug) fprintf(debug, f)
#define	DEBUG1(f,a)	if (debug) fprintf(debug, f, a)
#define	DEBUG2(f,a1,a2)	if (debug) fprintf(debug, f, a1, a2)

char	*checkbuf(char *, int);
void	getstring(char *, int);
void	error(int);

int
main(int argc, char *argv[])
{
	off_t orval;
	int rval;
	char c;
	int n, i, cc;

	argc--, argv++;
	if (argc > 0) {
		debug = fopen(*argv, "w");
		if (debug == 0)
			exit(1);
		(void) setbuf(debug, (char *)0);
	}
top:
	errno = 0;
	rval = 0;
	if (read(STDIN_FILENO, &c, 1) != 1)
		exit(0);
	switch (c) {

	case 'O':
		if (tape >= 0)
			(void) close(tape);
		getstring(device, sizeof(device));
		getstring(mode, sizeof(mode));
		DEBUG2("rmtd: O %s %s\n", device, mode);
		tape = open(device, atoi(mode),
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
		if (tape == -1)
			goto ioerror;
		goto respond;

	case 'C':
		DEBUG("rmtd: C\n");
		getstring(device, sizeof(device));	/* discard */
		if (close(tape) == -1)
			goto ioerror;
		tape = -1;
		goto respond;

	case 'L':
		getstring(count, sizeof(count));
		getstring(pos, sizeof(pos));
		DEBUG2("rmtd: L %s %s\n", count, pos);
		orval = lseek(tape, strtoq(count, NULL, 0), atoi(pos));
		if (orval == -1)
			goto ioerror;
		goto respond;

	case 'W':
		getstring(count, sizeof(count));
		n = atoi(count);
		DEBUG1("rmtd: W %s\n", count);
		record = checkbuf(record, n);
		for (i = 0; i < n; i += cc) {
			cc = read(STDIN_FILENO, &record[i], n - i);
			if (cc <= 0) {
				DEBUG("rmtd: premature eof\n");
				exit(2);
			}
		}
		rval = write(tape, record, n);
		if (rval < 0)
			goto ioerror;
		goto respond;

	case 'R':
		getstring(count, sizeof(count));
		DEBUG1("rmtd: R %s\n", count);
		n = atoi(count);
		record = checkbuf(record, n);
		rval = read(tape, record, n);
		if (rval < 0)
			goto ioerror;
		(void) snprintf(resp, sizeof resp, "A%d\n", rval);
		(void) write(STDOUT_FILENO, resp, strlen(resp));
		(void) write(STDOUT_FILENO, record, rval);
		goto top;

	case 'I':
		getstring(op, sizeof(op));
		getstring(count, sizeof(count));
		DEBUG2("rmtd: I %s %s\n", op, count);
		{ struct mtop mtop;
		  mtop.mt_op = atoi(op);
		  mtop.mt_count = atoi(count);
		  if (ioctl(tape, MTIOCTOP, (char *)&mtop) == -1)
			goto ioerror;
		  rval = mtop.mt_count;
		}
		goto respond;

	case 'S':		/* status */
		DEBUG("rmtd: S\n");
		{ struct mtget mtget;
		  if (ioctl(tape, MTIOCGET, (char *)&mtget) == -1)
			goto ioerror;
		  rval = sizeof (mtget);
		  (void) snprintf(resp, sizeof resp, "A%d\n", rval);
		  (void) write(STDOUT_FILENO, resp, strlen(resp));
		  (void) write(STDOUT_FILENO, (char *)&mtget, sizeof (mtget));
		  goto top;
		}

	default:
		DEBUG1("rmtd: garbage command %c\n", c);
		exit(3);
	}
respond:
	DEBUG1("rmtd: A %d\n", rval);
	(void) snprintf(resp, sizeof resp, "A%d\n", rval);
	(void) write(STDOUT_FILENO, resp, strlen(resp));
	goto top;
ioerror:
	error(errno);
	goto top;
}

void
getstring(char *bp, int size)
{
	char *cp = bp;
	char *ep = bp + size - 1;

	do {
		if (read(STDIN_FILENO, cp, 1) != 1)
			exit(0);
	} while (*cp != '\n' && ++cp < ep);
	*cp = '\0';
}

char *
checkbuf(char *record, int size)
{
	if (size <= maxrecsize)
		return (record);
	if (record != 0)
		free(record);
	record = malloc(size);
	if (record == 0) {
		DEBUG("rmtd: cannot allocate buffer space\n");
		exit(4);
	}
	maxrecsize = size;
	while (size > 1024 &&
	       setsockopt(0, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size)) == -1)
		size -= 1024;
	return (record);
}

void
error(int num)
{

	DEBUG2("rmtd: E %d (%s)\n", num, strerror(num));
	(void) snprintf(resp, sizeof (resp), "E%d\n%s\n", num, strerror(num));
	(void) write(STDOUT_FILENO, resp, strlen(resp));
}
