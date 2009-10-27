/*	$OpenBSD: tftpd.c,v 1.63 2009/10/27 23:59:32 deraadt Exp $	*/

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

/*
 * Trivial file transfer protocol server.
 *
 * This version includes many modifications by Jim Guyton <guyton@rand-unix>
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/tftp.h>
#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <vis.h>

#define TIMEOUT		5		/* packet rexmt timeout */
#define TIMEOUT_MIN	1		/* minimal packet rexmt timeout */
#define TIMEOUT_MAX	255		/* maximal packet rexmt timeout */

struct formats;

int		readit(FILE *, struct tftphdr **, int, int);
void		read_ahead(FILE *, int, int);
int		writeit(FILE *, struct tftphdr **, int, int);
int		write_behind(FILE *, int);
int		synchnet(int);

__dead void	usage(void);
void		tftp(struct tftphdr *, int);
int		validate_access(char *, int);
int		sendfile(struct formats *);
int		recvfile(struct formats *);
void		nak(int);
void		oack(int);
static char	*getip(struct sockaddr *);

FILE			 *file;
extern char		 *__progname;
struct sockaddr_storage	  s_in;
int			  peer;
int			  rexmtval = TIMEOUT;
int			  maxtimeout = 5 * TIMEOUT;
char			 *buf;
char			 *ackbuf;
struct sockaddr_storage	  from;
int			  ndirs;
char 			**dirs;
int			  secure;
int			  cancreate;
int			  logging;
unsigned int		  segment_size = SEGSIZE;
unsigned int		  packet_size = SEGSIZE + 4;
int			  has_options = 0;

struct formats {
	const char	*f_mode;
	int		 (*f_validate)(char *, int);
	int		 (*f_send)(struct formats *);
	int		 (*f_recv)(struct formats *);
	int		 f_convert;
} formats[] = {
	{ "netascii",	validate_access,	sendfile,	recvfile, 1 },
	{ "octet",	validate_access,	sendfile,	recvfile, 0 },
	{ NULL,		NULL,			NULL,		NULL,	  0 }
};

struct options {
	const char	*o_type;
	char		*o_request;
	long long	 o_reply;	/* turn into union if need be */
} options[] = {
	{ "tsize",	NULL, 0 },	/* OPT_TSIZE */
	{ "timeout",	NULL, 0 },	/* OPT_TIMEOUT */
	{ "blksize",	NULL, 0 },	/* OPT_BLKSIZE */
	{ NULL,		NULL, 0 }
};

enum opt_enum {
	OPT_TSIZE = 0,
	OPT_TIMEOUT,
	OPT_BLKSIZE
};

struct errmsg {
	int		 e_code;
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

__dead void
usage(void)
{
	syslog(LOG_ERR, "usage: %s [-cl] [directory ...]", __progname);
	syslog(LOG_ERR, "usage: %s [-cl] -s directory", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		 n = 0, on = 1, fd = 0, i, c, dobind = 1;
	struct tftphdr	*tp;
	struct passwd	*pw;
	union {
		struct cmsghdr hdr;
		char	buf[CMSG_SPACE(sizeof(struct sockaddr_storage))];
	} cmsgbuf;
	struct cmsghdr	*cmsg;
	struct msghdr	 msg;
	struct iovec	 iov;
	pid_t		 pid = 0;
	socklen_t	 j;

	openlog(__progname, LOG_PID|LOG_NDELAY, LOG_DAEMON);

	while ((c = getopt(argc, argv, "cls")) != -1) {
		switch (c) {
		case 'c':
			cancreate = 1;
			break;
		case 'l':
			logging = 1;
			break;
		case 's':
			secure = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	for (; optind != argc; optind++) {
		char **d;

		d = realloc(dirs, (ndirs + 2) * sizeof(char *));
		if (d == NULL) {
			syslog(LOG_ERR, "realloc: %m");
			exit(1);
		}
		dirs = d;
		dirs[n++] = argv[optind];
		dirs[n] = NULL;
		ndirs++;
	}

	pw = getpwnam("_tftpd");
	if (pw == NULL) {
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
		tzset();
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

	j = sizeof(s_in);
	if (getsockname(fd, (struct sockaddr *)&s_in, &j) == -1) {
		syslog(LOG_ERR, "getsockname: %m");
		exit(1);
	}

	switch (s_in.ss_family) {
	case AF_INET:
		if (setsockopt(fd, IPPROTO_IP, IP_RECVDSTADDR, &on,
		    sizeof(on)) == -1) {
			syslog(LOG_ERR, "setsockopt(IP_RECVDSTADDR): %m");
			exit (1);
		}
		break;
	case AF_INET6:
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
		    sizeof(on)) == -1) {
			syslog(LOG_ERR, "setsockopt(IPV6_RECVPKTINFO): %m");
			exit (1);
		}
		break;
	}

	if ((buf = malloc(SEGSIZE_MAX + 4)) == NULL) {
		syslog(LOG_ERR, "malloc: %m");
		exit(1);
	}
	if ((ackbuf = malloc(SEGSIZE_MAX + 4)) == NULL) {
		syslog(LOG_ERR, "malloc: %m");
		exit(1);
	}

	bzero(&msg, sizeof(msg));
	iov.iov_base = buf;
	iov.iov_len = packet_size;
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);

	n = recvmsg(fd, &msg, 0);
	if (n < 0) {
		syslog(LOG_ERR, "recvmsg: %m");
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
	 * breaks before doing the above "recvfrom", inetd would
	 * spawn endless instances, clogging the system.
	 */
	for (i = 1; i < 20; i++) {
		pid = fork();
		if (pid < 0) {
			sleep(i);
			/*
			 * Flush out to most recently sent request.
			 *
			 * This may drop some requests, but those
			 * will be resent by the clients when
			 * they timeout.  The positive effect of
			 * this flush is to (try to) prevent more
			 * than one tftpd being started up to service
			 * a single request from a single client.
			 */
			bzero(&msg, sizeof(msg));
			iov.iov_base = buf;
			iov.iov_len = packet_size;
			msg.msg_name = &from;
			msg.msg_namelen = sizeof(from);
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_control = &cmsgbuf.buf;
			msg.msg_controllen = sizeof(cmsgbuf.buf);

			i = recvmsg(fd, &msg, 0);
			if (i > 0)
				n = i;
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

	/* get local address if possible */
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IP &&
		    cmsg->cmsg_type == IP_RECVDSTADDR) {
			memcpy(&((struct sockaddr_in *)&s_in)->sin_addr,
			    CMSG_DATA(cmsg), sizeof(struct in_addr));
			if (((struct sockaddr_in *)&s_in)->sin_addr.s_addr ==
			    INADDR_BROADCAST)
				dobind = 0;
			break;
		}
		if (cmsg->cmsg_level == IPPROTO_IPV6 &&
		    cmsg->cmsg_type == IPV6_PKTINFO) {
			struct in6_pktinfo *ipi;

			ipi = (struct in6_pktinfo *)CMSG_DATA(cmsg);
			memcpy(&((struct sockaddr_in6 *)&s_in)->sin6_addr,
			    &ipi->ipi6_addr, sizeof(struct in6_addr));
#ifdef __KAME__
			if (IN6_IS_ADDR_LINKLOCAL(&ipi->ipi6_addr))
				((struct sockaddr_in6 *)&s_in)->sin6_scope_id =
				    ipi->ipi6_ifindex;
#endif
			break;
		}
	}

	if (dobind) {
		(void) setsockopt(peer, SOL_SOCKET, SO_REUSEADDR, &on,
		    sizeof(on));
		(void) setsockopt(peer, SOL_SOCKET, SO_REUSEPORT, &on,
		    sizeof(on));

		if (bind(peer, (struct sockaddr *)&s_in, s_in.ss_len) < 0) {
			syslog(LOG_ERR, "bind to %s: %m",
			    inet_ntoa(((struct sockaddr_in *)&s_in)->sin_addr));
			exit(1);
		}
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
	char		*cp;
	int		 i, first = 1, ecode, opcode, to;
	struct formats	*pf;
	char		*filename, *mode = NULL, *option, *ccp;
	char		 fnbuf[MAXPATHLEN], nicebuf[MAXPATHLEN];
	const char	*errstr;

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
		cp = ccp - 1;
	}

option_fail:
	if (options[OPT_TIMEOUT].o_request) {
		to = strtonum(options[OPT_TIMEOUT].o_request,
		    TIMEOUT_MIN, TIMEOUT_MAX, &errstr);
		if (errstr) {
			nak(EBADOP);
			exit(1);
		}
		options[OPT_TIMEOUT].o_reply = rexmtval = to;
	}

	if (options[OPT_BLKSIZE].o_request) {
		segment_size = strtonum(options[OPT_BLKSIZE].o_request,
		    SEGSIZE_MIN, SEGSIZE_MAX, &errstr);
		if (errstr) {
			nak(EBADOP);
			exit(1);
		}
		packet_size = segment_size + 4;
		options[OPT_BLKSIZE].o_reply = segment_size;
	}

	/* save opcode before it gets overwritten by oack() */
	opcode = tp->th_opcode;

	(void)strnvis(nicebuf, filename, MAXPATHLEN, VIS_SAFE|VIS_OCTAL);
	ecode = (*pf->f_validate)(filename, opcode);
	if (logging)
		syslog(LOG_INFO, "%s: %s request for '%s'",
		    getip((struct sockaddr *)&from),
		    opcode == WRQ ? "write" : "read",
		    nicebuf);
	if (has_options)
		oack(opcode);
	if (ecode) {
		syslog(LOG_INFO, "%s: denied %s access to '%s'",
		    getip((struct sockaddr *)&from),
		    opcode == WRQ ? "write" : "read", nicebuf);
		nak(ecode);
		exit(1);
	}
	if (opcode == WRQ)
		(*pf->f_recv)(pf);
	else
		(*pf->f_send)(pf);
	exit(0);
}

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
	struct stat	 stbuf;
	char		*cp, **dirp;
	int		 fd, wmode;
	const char	*errstr;

	if (!secure) {
		if (*filename != '/')
			return (EACCESS);
		/*
		 * Prevent tricksters from getting around the directory
		 * restrictions.
		 */
		for (cp = filename + 1; *cp; cp++)
			if (*cp == '.' && strncmp(cp - 1, "/../", 4) == 0)
				return (EACCESS);
		for (dirp = dirs; *dirp; dirp++)
			if (strncmp(filename, *dirp, strlen(*dirp)) == 0)
				break;
		if (*dirp == 0 && dirp != dirs)
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
				return (EACCESS);
		}
	} else {
		if (mode == RRQ) {
			if ((stbuf.st_mode & (S_IREAD >> 6)) == 0)
				return (EACCESS);
		} else {
			if ((stbuf.st_mode & (S_IWRITE >> 6)) == 0)
				return (EACCESS);
		}
	}
	if (options[OPT_TSIZE].o_request) {
		if (mode == RRQ)
			options[OPT_TSIZE].o_reply = stbuf.st_size;
		else {
			/* allows writes of 65535 blocks * SEGSIZE_MAX bytes */
			options[OPT_TSIZE].o_reply =
			    strtonum(options[OPT_TSIZE].o_request,
			    1, 65535LL * SEGSIZE_MAX, &errstr);
			if (errstr) {
				nak(EOPTNEG);
				exit(1);
			}
		}
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
	file = fdopen(fd, mode == RRQ ? "r" : "w");
	if (file == NULL) {
		close(fd);
		return (errno + 100);
	}
	return (0);
}

/*
 * Send the requested file.
 */
int
sendfile(struct formats *pf)
{
	struct tftphdr		*dp, *r_init(void);
	struct tftphdr		*ap;	/* ack packet */
	struct pollfd		 pfd[1];
	volatile unsigned short	 block = 1;
	int			 n, size, nfds, error, timeouts;

	dp = r_init();
	ap = (struct tftphdr *)ackbuf;

	do {
		/* read data from file */
		size = readit(file, &dp, pf->f_convert, segment_size);
		if (size < 0) {
			nak(errno + 100);
			goto abort;
		}
		dp->th_opcode = htons((u_short)DATA);
		dp->th_block = htons((u_short)block);

		/* send data to client and wait for client ACK */
		for (timeouts = 0, error = 0;;) {
			if (timeouts >= maxtimeout)
				exit(1);

			if (!error) {
				if (send(peer, dp, size + 4, 0) != size + 4) {
					syslog(LOG_ERR, "send: %m");
					goto abort;
				}
				read_ahead(file, pf->f_convert, segment_size);
			}
			error = 0;

			pfd[0].fd = peer;
			pfd[0].events = POLLIN;
			nfds = poll(pfd, 1, rexmtval * 1000);
			if (nfds == 0) {
				timeouts += rexmtval;
				continue;
			}
			if (nfds == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "poll: %m");
				goto abort;
			}
			n = recv(peer, ackbuf, packet_size, 0);
			if (n == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "recv: %m");
				goto abort;
			}
			ap->th_opcode = ntohs((u_short)ap->th_opcode);
			ap->th_block = ntohs((u_short)ap->th_block);

			if (ap->th_opcode == ERROR)
				goto abort;
			if (ap->th_opcode == ACK) {
				if (ap->th_block == block)
					break;
				/* re-synchronize with the other side */
				(void)synchnet(peer);
				if (ap->th_block == (block - 1))
					continue;
			}
			error = 1;	/* FALLTHROUGH */
		}

		block++;
	} while (size == segment_size);

abort:
	fclose(file);
	return (1);
}

/*
 * Receive a file.
 */
int
recvfile(struct formats *pf)
{
	struct tftphdr		*dp, *w_init(void);
	struct tftphdr		*ap;	/* ack buffer */
	struct pollfd		 pfd[1];
	volatile unsigned short	 block = 0;
	int			 n, size, nfds, error, timeouts;

	dp = w_init();
	ap = (struct tftphdr *)ackbuf;

	/* if we have options, do not send a first ACK */
	if (has_options) {
		block++;
		goto noack;
	}

	do {
		/* create new ACK packet */
		ap->th_opcode = htons((u_short)ACK);
		ap->th_block = htons((u_short)block);
		block++;

		/* send ACK to client and wait for client data */
		for (timeouts = 0, error = 0;;) {
			if (timeouts >= maxtimeout)
				exit(1);

			if (!error) {
				if (send(peer, ackbuf, 4, 0) != 4) {
					syslog(LOG_ERR, "send: %m");
					goto abort;
				}
				write_behind(file, pf->f_convert);
			}
			error = 0;

			pfd[0].fd = peer;
			pfd[0].events = POLLIN;
			nfds = poll(pfd, 1, rexmtval * 1000);
			if (nfds == 0) {
				timeouts += rexmtval;
				continue;
			}
			if (nfds == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "poll: %m");
				goto abort;
			}
noack:
			n = recv(peer, dp, packet_size, 0);
			if (n == -1) {
				error = 1;
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "recv: %m");
				goto abort;
			}
			dp->th_opcode = ntohs((u_short)dp->th_opcode);
			dp->th_block = ntohs((u_short)dp->th_block);

			if (dp->th_opcode == ERROR)
				goto abort;
			if (dp->th_opcode == DATA) {
				if (dp->th_block == block)
					break;
				/* re-synchronize with the other side */
				(void)synchnet(peer);
				if (dp->th_block == (block - 1))
					continue;
			}
			error = 1;	/* FALLTHROUGH */
		}

		/* write data to file */
		size = writeit(file, &dp, n - 4, pf->f_convert);
		if (size != (n - 4)) {
			if (size < 0)
				nak(errno + 100);
			else
				nak(ENOSPACE);
			goto abort;
		}
	} while (size == segment_size);

	/* close data file */
	write_behind(file, pf->f_convert);
	(void)fclose(file);

	/* send final ack */
	ap->th_opcode = htons((u_short)ACK);
	ap->th_block = htons((u_short)(block));
	(void)send(peer, ackbuf, 4, 0);

	/* just quit on timeout */
	pfd[0].fd = peer;
	pfd[0].events = POLLIN;
	nfds = poll(pfd, 1, rexmtval * 1000);
	if (nfds < 1)
		exit(1);
	n = recv(peer, buf, packet_size, 0);
	/*
	 * If read some data and got a data block then my last ack was lost
	 * resend final ack.
	 */
	if (n >= 4 && dp->th_opcode == DATA && block == dp->th_block)
		(void)send(peer, ackbuf, 4, 0);

abort:
	return (1);
}

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
void
nak(int error)
{
	struct tftphdr	*tp;
	struct errmsg	*pe;
	int		 length;

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
	length = strlcpy(tp->th_msg, pe->e_msg, packet_size) + 5;
	if (length > packet_size)
		length = packet_size;
	if (send(peer, buf, length, 0) != length)
		syslog(LOG_ERR, "nak: %m");
}

/*
 * Send an oack packet (option acknowledgement).
 */
void
oack(int opcode)
{
	struct tftphdr	*tp, *ap;
	struct pollfd	 pfd[1];
	char		*bp;
	int		 i, n, size, nfds, error, timeouts;

	tp = (struct tftphdr *)buf;
	bp = buf + 2;
	size = packet_size - 2;
	tp->th_opcode = htons((u_short)OACK);
	for (i = 0; options[i].o_type != NULL; i++) {
		if (options[i].o_request) {
			n = snprintf(bp, size, "%s%c%lld", options[i].o_type,
			    0, options[i].o_reply);
			if (n == -1 || n >= size) {
				syslog(LOG_ERR, "oack: no buffer space");
				exit(1);
			}
			bp += n + 1;
			size -= n + 1;
			if (size < 0) {
				syslog(LOG_ERR, "oack: no buffer space");
				exit(1);
			}
		}
	}
	size = bp - buf;
	ap = (struct tftphdr *)ackbuf;

	/* send OACK to client and wait for client ACK */
	for (timeouts = 0, error = 0;;) {
		if (timeouts >= maxtimeout)
			exit(1);

		if (!error) {
			if (send(peer, buf, size, 0) != size) {
				syslog(LOG_INFO, "oack: %m");
				exit(1);
			}
		}
		error = 0;

		pfd[0].fd = peer;
		pfd[0].events = POLLIN;
		nfds = poll(pfd, 1, rexmtval * 1000);
		if (nfds == 0) {
			timeouts += rexmtval;
			continue;
		}
		if (nfds == -1) {
			error = 1;
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "poll: %m");
			exit(1);
		}

		/* no client ACK for write requests with options */
		if (opcode == WRQ)
			break;

		n = recv(peer, ackbuf, packet_size, 0);
		if (n == -1) {
			error = 1;
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "recv: %m");
			exit(1);
		}
		ap->th_opcode = ntohs((u_short)ap->th_opcode);
		ap->th_block = ntohs((u_short)ap->th_block);

		if (ap->th_opcode == ERROR)
			exit(1);
		if (ap->th_opcode == ACK && ap->th_block == 0)
			break;
		error = 1;	/* FALLTHROUGH */
	}
}

static char *
getip(struct sockaddr *sa)
{
	static char hbuf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		strlcpy(hbuf, "0.0.0.0", sizeof(hbuf));
	return(hbuf);
}
