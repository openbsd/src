/*	$OpenBSD: tftpd.c,v 1.34 2004/04/28 15:18:57 deraadt Exp $	*/

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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)tftpd.c	5.13 (Berkeley) 2/26/91";*/
static char rcsid[] = "$OpenBSD: tftpd.c,v 1.34 2004/04/28 15:18:57 deraadt Exp $";
#endif /* not lint */

/*
 * Trivial file transfer protocol server.
 *
 * This version includes many modifications by Jim Guyton <guyton@rand-unix>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/tftp.h>
#include <netdb.h>

#include <setjmp.h>
#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>

#define	TIMEOUT		5
#define	MAX_TIMEOUTS	5

extern	char *__progname;
struct	sockaddr_storage s_in;
int	peer;
int	rexmtval = TIMEOUT;
int	max_rexmtval = 2*TIMEOUT;

#define	PKTSIZE	SEGSIZE+4
char	buf[PKTSIZE];
char	ackbuf[PKTSIZE];
struct	sockaddr_storage from;
socklen_t fromlen;

int	ndirs;
char	**dirs;

int	secure;
int	cancreate;

struct	formats;
int	validate_access(char *filename, int mode);
int	recvfile(struct formats *pf);
int	sendfile(struct formats *pf);

struct formats {
	const char	*f_mode;
	int	(*f_validate)(char *, int);
	int	(*f_send)(struct formats *);
	int	(*f_recv)(struct formats *);
	int	f_convert;
} formats[] = {
	{ "netascii",	validate_access,	sendfile,	recvfile, 1 },
	{ "octet",	validate_access,	sendfile,	recvfile, 0 },
	{ NULL,		NULL,			NULL,		NULL,	  0 }
};
struct options {
	const char	*o_type;
	char		*o_request;
	int		o_reply;	/* turn into union if need be */
} options[] = {
	{ "tsize",	NULL, 0 },	/* OPT_TSIZE */
	{ "timeout",	NULL, 0 },	/* OPT_TIMEOUT */
	{ NULL,		NULL, 0 }
};
enum opt_enum {
	OPT_TSIZE = 0,
	OPT_TIMEOUT
};

int	validate_access(char *filename, int mode);
void	tftp(struct tftphdr *tp, int size);
void	nak(int error);
void	oack(void);

int	readit(FILE *file, struct tftphdr **dpp, int convert);
void	read_ahead(FILE *file, int convert);
int	writeit(FILE *file, struct tftphdr **dpp, int ct, int convert);
int	write_behind(FILE *file, int convert);
int	synchnet(int f);

static void
usage(void)
{
	syslog(LOG_ERR, "Usage: %s [-cs] [directory ...]", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int n = 0, on = 1, fd = 0, i, c;
	struct tftphdr *tp;
	struct passwd *pw;
	pid_t pid = 0;
	socklen_t j;

	openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	while ((c = getopt(argc, argv, "cs")) != -1)
		switch (c) {
		case 'c':
			cancreate = 1;
			break;
		case 's':
			secure = 1;
			break;
		default:
			usage();
			break;
		}

	for (; optind != argc; optind++) {
		char **d;

		d = realloc(dirs, (ndirs+2) * sizeof (char *));
		if (d == NULL) {
			syslog(LOG_ERR, "malloc: %m");
			exit(1);
		}
		dirs = d;
		dirs[n++] = argv[optind];
		dirs[n] = NULL;
		ndirs++;
	}

	pw = getpwnam("_tftpd");
	if (!pw) {
		syslog(LOG_ERR, "no _tftpd: %m");
		exit(1);
	}

	if (secure) {
		if (ndirs == 0) {
			syslog(LOG_ERR, "no -s directory");
			exit(1);
		}
		if (ndirs > 1) {
			syslog(LOG_ERR, "too many -s directories");
			exit(1);
		}
		if (chroot(dirs[0])) {
			syslog(LOG_ERR, "chroot %s: %m", dirs[0]);
			exit(1);
		}
		if (chdir("/")) {
			syslog(LOG_ERR, "chdir: %m");
			exit(1);
		}
	}

	setegid(pw->pw_gid);
	setgid(pw->pw_gid);
	seteuid(pw->pw_uid);
	setuid(pw->pw_uid);

	if (ioctl(fd, FIONBIO, &on) < 0) {
		syslog(LOG_ERR, "ioctl(FIONBIO): %m");
		exit(1);
	}
	fromlen = sizeof (from);
	n = recvfrom(fd, buf, sizeof (buf), 0,
	    (struct sockaddr *)&from, &fromlen);
	if (n < 0) {
		syslog(LOG_ERR, "recvfrom: %m");
		exit(1);
	}
	/*
	 * Now that we have read the message out of the UDP
	 * socket, we fork and exit.  Thus, inetd will go back
	 * to listening to the tftp port, and the next request
	 * to come in will start up a new instance of tftpd.
	 *
	 * We do this so that inetd can run tftpd in "wait" mode.
	 * The problem with tftpd running in "nowait" mode is that
	 * inetd may get one or more successful "selects" on the
	 * tftp port before we do our receive, so more than one
	 * instance of tftpd may be started up.  Worse, if tftpd
	 * break before doing the above "recvfrom", inetd would
	 * spawn endless instances, clogging the system.
	 */
	for (i = 1; i < 20; i++) {
		pid = fork();
		if (pid < 0) {
			sleep(i);
			/*
			 * flush out to most recently sent request.
			 *
			 * This may drop some request, but those
			 * will be resent by the clients when
			 * they timeout.  The positive effect of
			 * this flush is to (try to) prevent more
			 * than one tftpd being started up to service
			 * a single request from a single client.
			 */
			j = sizeof from;
			i = recvfrom(fd, buf, sizeof (buf), 0,
			    (struct sockaddr *)&from, &j);
			if (i > 0) {
				n = i;
				fromlen = j;
			}
		} else
			break;
	}
	if (pid < 0) {
		syslog(LOG_ERR, "fork: %m");
		exit(1);
	} else if (pid != 0)
		exit(0);

	alarm(0);
	close(fd);
	close(1);
	peer = socket(from.ss_family, SOCK_DGRAM, 0);
	if (peer < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	memset(&s_in, 0, sizeof(s_in));
	s_in.ss_family = from.ss_family;
	s_in.ss_len = from.ss_len;
	if (bind(peer, (struct sockaddr *)&s_in, s_in.ss_len) < 0) {
		syslog(LOG_ERR, "bind: %m");
		exit(1);
	}
	if (connect(peer, (struct sockaddr *)&from, from.ss_len) < 0) {
		syslog(LOG_ERR, "connect: %m");
		exit(1);
	}
	tp = (struct tftphdr *)buf;
	tp->th_opcode = ntohs(tp->th_opcode);
	if (tp->th_opcode == RRQ || tp->th_opcode == WRQ)
		tftp(tp, n);
	exit(1);
}

/*
 * Handle initial connection protocol.
 */
void
tftp(struct tftphdr *tp, int size)
{
	char *cp;
	int i, first = 1, has_options = 0, ecode;
	struct formats *pf;
	char *filename, *mode = NULL, *option, *ccp;
	char fnbuf[MAXPATHLEN];

	cp = tp->th_stuff;
again:
	while (cp < buf + size) {
		if (*cp == '\0')
			break;
		cp++;
	}
	if (*cp != '\0') {
		nak(EBADOP);
		exit(1);
	}
	i = cp - tp->th_stuff;
	if (i >= sizeof(fnbuf)) {
		nak(EBADOP);
		exit(1);
	}
	memcpy(fnbuf, tp->th_stuff, i);
	fnbuf[i] = '\0';
	filename = fnbuf;
	if (first) {
		mode = ++cp;
		first = 0;
		goto again;
	}
	for (cp = mode; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);
	for (pf = formats; pf->f_mode; pf++)
		if (strcmp(pf->f_mode, mode) == 0)
			break;
	if (pf->f_mode == 0) {
		nak(EBADOP);
		exit(1);
	}
	while (++cp < buf + size) {
		for (i = 2, ccp = cp; i > 0; ccp++) {
			if (ccp >= buf + size) {
				/*
				 * Don't reject the request, just stop trying
				 * to parse the option and get on with it.
				 * Some Apple OpenFirmware versions have
				 * trailing garbage on the end of otherwise
				 * valid requests.
				 */
				goto option_fail;
			} else if (*ccp == '\0')
				i--;
		}
		for (option = cp; *cp; cp++)
			if (isupper(*cp))
				*cp = tolower(*cp);
		for (i = 0; options[i].o_type != NULL; i++)
			if (strcmp(option, options[i].o_type) == 0) {
				options[i].o_request = ++cp;
				has_options = 1;
			}
		cp = ccp-1;
	}

option_fail:
	if (options[OPT_TIMEOUT].o_request) {
		int to = atoi(options[OPT_TIMEOUT].o_request);
		if (to < 1 || to > 255) {
			nak(EBADOP);
			exit(1);
		}
		else if (to <= max_rexmtval)
			options[OPT_TIMEOUT].o_reply = rexmtval = to;
		else
			options[OPT_TIMEOUT].o_request = NULL;
	}

	ecode = (*pf->f_validate)(filename, tp->th_opcode);
	if (has_options)
		oack();
	if (ecode) {
		nak(ecode);
		exit(1);
	}
	if (tp->th_opcode == WRQ)
		(*pf->f_recv)(pf);
	else
		(*pf->f_send)(pf);
	exit(0);
}


FILE *file;

/*
 * Validate file access.  Since we
 * have no uid or gid, for now require
 * file to exist and be publicly
 * readable/writable.
 * If we were invoked with arguments
 * from inetd then the file must also be
 * in one of the given directory prefixes.
 * Note also, full path name must be
 * given as we have no login directory.
 */
int
validate_access(char *filename, int mode)
{
	struct stat stbuf;
	char *cp, **dirp;
	int fd, wmode;

	if (!secure) {
		if (*filename != '/')
			return (EACCESS);
		/*
		 * prevent tricksters from getting around the directory
		 * restrictions
		 */
		for (cp = filename + 1; *cp; cp++)
			if (*cp == '.' && strncmp(cp-1, "/../", 4) == 0)
				return(EACCESS);
		for (dirp = dirs; *dirp; dirp++)
			if (strncmp(filename, *dirp, strlen(*dirp)) == 0)
				break;
		if (*dirp==0 && dirp!=dirs)
			return (EACCESS);
	}

	/*
	 * We use a different permissions scheme if `cancreate' is
	 * set.
	 */
	wmode = O_TRUNC;
	if (stat(filename, &stbuf) < 0) {
		if (!cancreate)
			return (errno == ENOENT ? ENOTFOUND : EACCESS);
		else {
			if ((errno == ENOENT) && (mode != RRQ))
				wmode |= O_CREAT;
			else
				return(EACCESS);
		}
	} else {
		if (mode == RRQ) {
			if ((stbuf.st_mode&(S_IREAD >> 6)) == 0)
				return (EACCESS);
		} else {
			if ((stbuf.st_mode&(S_IWRITE >> 6)) == 0)
				return (EACCESS);
		}
	}
	if (options[OPT_TSIZE].o_request) {
		if (mode == RRQ) 
			options[OPT_TSIZE].o_reply = stbuf.st_size;
		else
			/* XXX Allows writes of all sizes. */
			options[OPT_TSIZE].o_reply =
				atoi(options[OPT_TSIZE].o_request);
	}
	fd = open(filename, mode == RRQ ? O_RDONLY : (O_WRONLY|wmode), 0666);
	if (fd < 0)
		return (errno + 100);
	/*
	 * If the file was created, set default permissions.
	 */
	if ((wmode & O_CREAT) && fchmod(fd, 0666) < 0) {
		int serrno = errno;

		close(fd);
		unlink(filename);

		return (serrno + 100);
	}
	file = fdopen(fd, (mode == RRQ)? "r":"w");
	if (file == NULL) {
		close(fd);
		return (errno + 100);
	}
	return (0);
}

int	timeouts;
jmp_buf	timeoutbuf;

static void
timer(int signo)
{
	/* XXX longjmp/signal resource leaks */
	if (++timeouts >= MAX_TIMEOUTS)
		_exit(1);
	longjmp(timeoutbuf, 1);
}

/*
 * Send the requested file.
 */
int
sendfile(struct formats *pf)
{
	struct tftphdr *dp, *r_init(void);
	struct tftphdr *ap;    /* ack packet */
	volatile unsigned short block = 1;
	int size, n;

	signal(SIGALRM, timer);
	dp = r_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		size = readit(file, &dp, pf->f_convert);
		if (size < 0) {
			nak(errno + 100);
			goto abort;
		}
		dp->th_opcode = htons((u_short)DATA);
		dp->th_block = htons((u_short)block);
		timeouts = 0;
		setjmp(timeoutbuf);

send_data:
		if (send(peer, dp, size + 4, 0) != size + 4) {
			syslog(LOG_ERR, "tftpd: write: %m");
			goto abort;
		}
		read_ahead(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);	/* read the ack */
			n = recv(peer, ackbuf, sizeof (ackbuf), 0);
			alarm(0);
			if (n < 0) {
				syslog(LOG_ERR, "tftpd: read: %m");
				goto abort;
			}
			ap->th_opcode = ntohs((u_short)ap->th_opcode);
			ap->th_block = ntohs((u_short)ap->th_block);

			if (ap->th_opcode == ERROR)
				goto abort;

			if (ap->th_opcode == ACK) {
				if (ap->th_block == block) {
					break;
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (ap->th_block == (block -1)) {
					goto send_data;
				}
			}

		}
		block++;
	} while (size == SEGSIZE);
abort:
	fclose(file);
	return (1);
}

static void
justquit(int signo)
{
	_exit(0);
}


/*
 * Receive a file.
 */
int
recvfile(struct formats *pf)
{
	struct tftphdr *dp, *w_init(void);
	struct tftphdr *ap;    /* ack buffer */
	volatile unsigned short block = 0;
	int n, size;

	signal(SIGALRM, timer);
	dp = w_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		timeouts = 0;
		ap->th_opcode = htons((u_short)ACK);
		ap->th_block = htons((u_short)block);
		block++;
		setjmp(timeoutbuf);
send_ack:
		if (send(peer, ackbuf, 4, 0) != 4) {
			syslog(LOG_ERR, "tftpd: write: %m");
			goto abort;
		}
		write_behind(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);
			n = recv(peer, dp, PKTSIZE, 0);
			alarm(0);
			if (n < 0) {		/* really? */
				syslog(LOG_ERR, "tftpd: read: %m");
				goto abort;
			}
			dp->th_opcode = ntohs((u_short)dp->th_opcode);
			dp->th_block = ntohs((u_short)dp->th_block);
			if (dp->th_opcode == ERROR)
				goto abort;
			if (dp->th_opcode == DATA) {
				if (dp->th_block == block) {
					break;   /* normal */
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (dp->th_block == (block-1))
					goto send_ack;		/* rexmit */
			}
		}
		/*  size = write(file, dp->th_data, n - 4); */
		size = writeit(file, &dp, n - 4, pf->f_convert);
		if (size != (n-4)) {			/* ahem */
			if (size < 0)
				nak(errno + 100);
			else nak(ENOSPACE);
			goto abort;
		}
	} while (size == SEGSIZE);
	write_behind(file, pf->f_convert);
	(void) fclose(file);		/* close data file */

	ap->th_opcode = htons((u_short)ACK);    /* send the "final" ack */
	ap->th_block = htons((u_short)(block));
	(void) send(peer, ackbuf, 4, 0);

	signal(SIGALRM, justquit);      /* just quit on timeout */
	alarm(rexmtval);
	n = recv(peer, buf, sizeof (buf), 0); /* normally times out and quits */
	alarm(0);
	if (n >= 4 &&			/* if read some data */
	    dp->th_opcode == DATA &&    /* and got a data block */
	    block == dp->th_block) {	/* then my last ack was lost */
		(void) send(peer, ackbuf, 4, 0);     /* resend final ack */
	}
abort:
	return (1);
}

struct errmsg {
	int	e_code;
	const char	*e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ EOPTNEG,	"Option negotiation failed" },
	{ -1,		NULL }
};

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
void
nak(int error)
{
	struct tftphdr *tp;
	struct errmsg *pe;
	int length;

	tp = (struct tftphdr *)buf;
	tp->th_opcode = htons((u_short)ERROR);
	tp->th_code = htons((u_short)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = strerror(error - 100);
		tp->th_code = EUNDEF;   /* set 'undef' errorcode */
	}
	length = strlcpy(tp->th_msg, pe->e_msg, sizeof(buf)) + 5;
	if (length > sizeof(buf))
		length = sizeof(buf);
	if (send(peer, buf, length, 0) != length)
		syslog(LOG_ERR, "nak: %m");
}

/*
 * Send an oack packet (option acknowledgement).
 */
void
oack(void)
{
	struct tftphdr *tp, *ap;
	int size, i, n;
	char *bp;

	tp = (struct tftphdr *)buf;
	bp = buf + 2;
	size = sizeof(buf) - 2;
	tp->th_opcode = htons((u_short)OACK);
	for (i = 0; options[i].o_type != NULL; i++) {
		if (options[i].o_request) {
			n = snprintf(bp, size, "%s%c%d", options[i].o_type,
				     0, options[i].o_reply);
			bp += n+1;
			size -= n+1;
			if (size < 0) {
				syslog(LOG_ERR, "oack: buffer overflow");
				exit(1);
			}
		}
	}
	size = bp - buf;
	ap = (struct tftphdr *)ackbuf;
	signal(SIGALRM, timer);
	timeouts = 0;

	(void)setjmp(timeoutbuf);
	if (send(peer, buf, size, 0) != size) {
		syslog(LOG_INFO, "oack: %m");
		exit(1);
	}

	for (;;) {
		alarm(rexmtval);
		n = recv(peer, ackbuf, sizeof (ackbuf), 0);
		alarm(0);
		if (n < 0) {
			syslog(LOG_ERR, "recv: %m");
			exit(1);
		}
		ap->th_opcode = ntohs((u_short)ap->th_opcode);
		ap->th_block = ntohs((u_short)ap->th_block);
		if (ap->th_opcode == ERROR)
			exit(1);
		if (ap->th_opcode == ACK && ap->th_block == 0)
			break;
	}
}
