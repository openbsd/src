/*	$OpenBSD: login_tis.c,v 1.9 2008/03/24 16:11:00 deraadt Exp $	*/

/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/resource.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <login_cap.h>
#include <bsd_auth.h>
#include <des.h>			/* openssl/des.h */

#include "login_tis.h"

#define	MODE_LOGIN	0
#define	MODE_CHALLENGE	1
#define	MODE_RESPONSE	2

enum response_type {
	error,
	password,
	challenge,
	chalnecho,
	display
};

ssize_t tis_recv(struct tis_connection *, u_char *, size_t);
ssize_t tis_send(struct tis_connection *, u_char *, size_t);
void quit(int);
void send_fd(struct tis_connection *, int);
void tis_getconf(struct tis_connection *, char *);
ssize_t tis_decode(u_char *, size_t);
ssize_t tis_encode(u_char *, size_t, size_t);
int tis_getkey(struct tis_connection *);
int tis_open(struct tis_connection *, const char *, char *);
int tis_verify(struct tis_connection *, const char *, char *);
enum response_type tis_authorize(struct tis_connection *, const char *,
    const char *, char *);

int
main(int argc, char *argv[])
{
	struct tis_connection tc;
	struct passwd *pw;
	struct sigaction sa;
	sigset_t sigset;
	uid_t uid;
	enum response_type rtype = error;
	FILE *back = NULL;
	char *user = NULL, *class = NULL, *cp, *ep;
	char chalbuf[TIS_BUFSIZ], respbuf[TIS_BUFSIZ], ebuf[TIS_BUFSIZ];
	int ch, mode = MODE_LOGIN;

	(void)setpriority(PRIO_PROCESS, 0, 0);

	(void)sigemptyset(&sigset);
	(void)sigaddset(&sigset, SIGSTOP);
	(void)sigaddset(&sigset, SIGTSTP);
	(void)sigaddset(&sigset, SIGTTIN);
	(void)sigaddset(&sigset, SIGTTOU);
	(void)sigprocmask(SIG_BLOCK, &sigset, NULL);

	memset(&sa, 0, sizeof(sa));
	(void)sigemptyset(&sa.sa_mask);
	sa.sa_handler = quit;
	(void)sigaction(SIGINT, &sa, NULL);
	(void)sigaction(SIGQUIT, &sa, NULL);
	(void)sigaction(SIGALRM, &sa, NULL);

	openlog(NULL, LOG_ODELAY, LOG_AUTH);

	tc.fd = -1;
	while ((ch = getopt(argc, argv, "ds:v:")) != -1) {
		switch (ch) {
		case 'd':
			back = stdout;
			break;
		case 's':	/* service */
			if (strcmp(optarg, "login") == 0)
				mode = MODE_LOGIN;
			else if (strcmp(optarg, "challenge") == 0)
				mode = MODE_CHALLENGE;
			else if (strcmp(optarg, "response") == 0)
				mode = MODE_RESPONSE;
			else {
				syslog(LOG_ERR, "%s: invalid service", optarg);
				exit(1);
			}
			break;
		case 'v':
			if (strncmp(optarg, "fd=", 3) == 0) {
				const char *errstr;
				tc.fd =
				    strtonum(optarg + 3, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					syslog(LOG_ERR, "fd is %s: %s",
					    errstr, optarg + 3);
					tc.fd = -1;
				}
			}
			/* silently ignore unsupported variables */
			break;
		default:
			syslog(LOG_ERR, "usage error");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	switch (argc) {
	case 2:
		user = argv[0];
		class = argv[1];
		break;
	case 1:
		/* class is not really optional so check passwd(5) entry */
		user = argv[0];
		if ((pw = getpwnam(user)) != NULL && pw->pw_class != NULL)
			class = strdup(pw->pw_class);
		break;
	default:
		syslog(LOG_ERR, "usage error");
		exit(1);
	}

	if (back == NULL && (back = fdopen(3, "r+")) == NULL)  {
		syslog(LOG_ERR, "reopening back channel: %m");
		exit(1);
	}

	tis_getconf(&tc, class);
	if (tc.keyfile != NULL && tis_getkey(&tc) != 0)
		exit(1);

	/* no longer need special perms */
	if ((uid = getuid()) != geteuid()) {
		seteuid(uid);
		setuid(uid);
	}
	
	if (tc.fd == -1) {
		if (tis_open(&tc, tc.server, ebuf) == -1 && (!tc.altserver ||
		    tis_open(&tc, tc.altserver, ebuf) == -1)) {
			syslog(LOG_ERR, "unable to connect to authsrv: %s",
			    ebuf);
			exit(1);
		}
		if ((rtype = tis_authorize(&tc, user, class, chalbuf)) == error)
			exit(1);
	}

	switch (mode) {
	case MODE_LOGIN:
		if (rtype == display) {
			printf("%s", chalbuf);
			exit(1);
		}
		alarm(TIS_PASSWD_TIMEOUT);
		if (!readpassphrase(chalbuf, respbuf, sizeof(respbuf),
		    rtype == challenge ? RPP_ECHO_ON : RPP_ECHO_OFF))
			exit(1);
		alarm(0);
		break;

	case MODE_CHALLENGE:
		switch (rtype) {
		case display:
			(void)fprintf(back, "value errormsg %s\n",
			    auth_mkvalue(chalbuf));
			exit(1);
		case password:
			fprintf(back, BI_SILENT "\n");
			break;
		default:
			/* XXX - should distinguish chalnecho from challenge */
			fprintf(back, BI_VALUE " challenge %s\n",
			    auth_mkvalue(chalbuf));
			fprintf(back, BI_CHALLENGE "\n");
		}
		fprintf(back, BI_FDPASS "\n");
		fflush(back);
		send_fd(&tc, fileno(back));
		exit(0);

	case MODE_RESPONSE:
		/* read challenge from backchannel */
		mode = -1;
		cp = chalbuf;
		ep = chalbuf + sizeof(chalbuf);
		while (cp < ep && read(fileno(back), cp, 1) == 1) {
			if (*cp++ == '\0') {
				mode = MODE_CHALLENGE;
				break;
			}
		}
		if (mode != MODE_CHALLENGE) {
			syslog(LOG_ERR,
			    "protocol error: bad/missing challenge");
			exit(1);
		}
		if (rtype == error) {
			/* didn't read the challenge ourselves so just guess */
			if (strcmp(chalbuf, "Password:") == 0)
				rtype = password;
			else
				rtype = challenge;
		}

		/* read user's response from backchannel */
		cp = respbuf;
		ep = respbuf + sizeof(respbuf);
		while (cp < ep && read(fileno(back), cp, 1) == 1) {
			if (*cp++ == '\0') {
				mode = MODE_RESPONSE;
				break;
			}
		}
		if (mode != MODE_RESPONSE) {
			syslog(LOG_ERR,
			    "protocol error: bad/missing response");
			exit(1);
		}
		break;
	}

	if (tis_verify(&tc, respbuf, ebuf) == 0) {
		if (ebuf[0] != '\0')
			(void)fprintf(back, "value errormsg %s\n",
			    auth_mkvalue(ebuf));
		fprintf(back, BI_AUTH "\n");
		if (rtype == challenge)
			fprintf(back, BI_SECURE "\n");
		exit(0);
	}
	if (ebuf[0] != '\0')
		(void)fprintf(back, "value errormsg %s\n", auth_mkvalue(ebuf));
	fprintf(back, BI_REJECT "\n");
	exit(1);
}

void
quit(int signo)
{
	struct syslog_data data;

	if (signo == SIGALRM)
		syslog_r(LOG_ERR, &data, "timed out talking to authsrv");
	_exit(1);
}

/*
 * Send the file descriptor in struct tis_connection over a socketpair
 * to the parent process to keep the connection to authsrv open.
 */
void
send_fd(struct tis_connection *tc, int sock)
{
	struct msghdr msg;
	struct cmsghdr *cmp;
	union {
		struct cmsghdr hdr;
		char buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);

	cmp = CMSG_FIRSTHDR(&msg);
	cmp->cmsg_len = CMSG_LEN(sizeof(int));
	cmp->cmsg_level = SOL_SOCKET;
	cmp->cmsg_type = SCM_RIGHTS;

	*(int *)CMSG_DATA(cmp) = tc->fd;

	if (sendmsg(sock, &msg, 0) < 0)
		syslog(LOG_ERR, "sendmsg: %m");
}

/*
 * Look up the given login class and populate struct tis_connection.
 */
void
tis_getconf(struct tis_connection *tc, char *class)
{
	login_cap_t *lc;

	if ((lc = login_getclass(class)) == NULL) {
		tc->port = TIS_DEFPORT;
		tc->timeout = TIS_DEFTIMEOUT;
		tc->server = TIS_DEFSERVER;
		tc->altserver = NULL;
		tc->keyfile = NULL;
		return;
	}

	tc->port = login_getcapstr(lc, "tis-port", TIS_DEFPORT, TIS_DEFPORT);
	tc->timeout = login_getcapnum(lc, "tis-timeout", TIS_DEFTIMEOUT,
	    TIS_DEFTIMEOUT);
	tc->server = login_getcapstr(lc, "tis-server", TIS_DEFSERVER,
	    TIS_DEFSERVER);
	tc->altserver = login_getcapstr(lc, "tis-server-alt", NULL, NULL);
	tc->keyfile = login_getcapstr(lc, "tis-keyfile", NULL, NULL);
}

/*
 * Read an ASCII string from a file and convert it to a DES key.
 */
int
tis_getkey(struct tis_connection *tc)
{
	size_t len;
	struct stat sb;
	des_cblock cblock;
	char *key, *tbuf = NULL;
	FILE *fp;
	int error;

	if ((fp = fopen(tc->keyfile, "r")) == NULL) {
		syslog(LOG_ERR, "%s: %m", tc->keyfile);
		return (-1);
	}
	if (fstat(fileno(fp), &sb) == -1) {
		syslog(LOG_ERR, "%s: %m", tc->keyfile);
		fclose(fp);
		return (-1);
	}
	if (sb.st_uid != 0) {
		syslog(LOG_ERR, "%s: not owned by root", tc->keyfile);
		fclose(fp);
		return (-1);
	}
	if (!S_ISREG(sb.st_mode)) {
		syslog(LOG_ERR, "%s: not a regular file", tc->keyfile);
		fclose(fp);
		return (-1);
	}
	if (sb.st_mode & (S_IRWXG|S_IRWXO)) {
		syslog(LOG_ERR, "%s: readable or writable by non-owner",
		    tc->keyfile);
		fclose(fp);
		return (-1);
	}
	if ((key = fgetln(fp, &len)) == NULL || (len == 1 && key[0] == '\n')) {
		if (ferror(fp))
			syslog(LOG_ERR, "%s: %m", tc->keyfile);
		else
			syslog(LOG_ERR, "%s: empty key file", tc->keyfile);
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	if (key[len - 1] == '\n')
		key[--len] = '\0';
	else {
		if ((tbuf = malloc(len + 1)) == NULL) {
			syslog(LOG_ERR, "%s: %m", tc->keyfile);
			return (-1);
		}
		memcpy(tbuf, key, len);
		tbuf[len] = '\0';
		key = tbuf;
	}
	des_string_to_key(key, &cblock);
	error = des_set_key(&cblock, tc->keysched);
	memset(key, 0, len);
	memset(&cblock, 0, sizeof(cblock));
	free(tbuf);
	return (error);
}

/*
 * Open a connection to authsrv and read the welcom banner.
 * Returns the file descriptor on success and -1 on error
 * or unrecognized welcome banner.
 */
int
tis_open(struct tis_connection *tc, const char *server, char *ebuf)
{
	struct addrinfo hints, *res, *res0;
	char buf[TIS_BUFSIZ];
	int error;

	ebuf[0] = '\0';
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = 0;
	error = getaddrinfo(server, tc->port, &hints, &res0);
	if (error) {
		strlcpy(ebuf, gai_strerror(error), TIS_BUFSIZ);
		return (-1);
	}

	/* connect to the first address that succeeds */
	for (res = res0; res != NULL; res = res->ai_next) {
		tc->fd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (tc->fd != -1) {
			if (connect(tc->fd, res->ai_addr, res->ai_addrlen) == 0)
				break;
			close(tc->fd);
		}
	}
	if (res == NULL) {
		strlcpy(ebuf, strerror(errno), TIS_BUFSIZ);
		freeaddrinfo(res0);
		tc->fd = -1;
		return (-1);
	}
	freeaddrinfo(res0);

	/* read welcome banner */
	if (tis_recv(tc, buf, sizeof(buf)) == -1) {
		strlcpy(ebuf, strerror(errno), TIS_BUFSIZ);
		close(tc->fd);
		tc->fd = -1;
		return (-1);
	}
	if (strncmp(buf, "Authsrv ready", 13) != 0) {
		strlcpy(ebuf, buf, TIS_BUFSIZ);
		close(tc->fd);
		tc->fd = -1;
		return (-1);
	}

	return (tc->fd);
}

/*
 * Read a one-line response from authsrv.
 * On success, returns 0.  On error, returns non-zero and calls syslog().
 */
ssize_t
tis_recv(struct tis_connection *tc, u_char *buf, size_t bufsiz)
{
	des_key_schedule ks;
	des_cblock iv;
	ssize_t len;
	u_char *cp, *ep, tbuf[TIS_BUFSIZ];

	for (cp = buf, ep = buf + bufsiz; cp < ep; cp++) {
		alarm(tc->timeout);
		len = read(tc->fd, cp, 1);
		alarm(0);
		if (len != 1) {
			if (len == -1)
				syslog(LOG_ERR,
				    "error reading data from authsrv: %m");
			else
				syslog(LOG_ERR, "EOF reading data from authsrv");
			return (-1);
		}
		if (*cp == '\n')
			break;
	}
	if (*cp != '\n') {
		syslog(LOG_ERR, "server response too large");
		return (-1);
	}
	*cp = '\0';
	len = cp - buf;

	if (tc->keyfile != NULL) {
		if ((len = tis_decode(buf, len)) < 0) {
		    syslog(LOG_ERR, "improperly encoded data from authsrv");
		    return (-1);
		}
		if (len > sizeof(tbuf)) {
			syslog(LOG_ERR, "encrypted data too large to store");
			return (-1);
		}
		memcpy(ks, tc->keysched, sizeof(ks));
		memset(iv, 0, sizeof(iv));
		des_ncbc_encrypt((des_cblock *)buf, (des_cblock *)tbuf,
		    len, ks, &iv, DES_DECRYPT);
		if (strlcpy(buf, tbuf, bufsiz) >= bufsiz) {
			syslog(LOG_ERR, "unencrypted data too large to store");
			memset(tbuf, 0, sizeof(tbuf));
			return (-1);
		}
		memset(tbuf, 0, sizeof(tbuf));
	}
	return (len);
}

/*
 * Send a line to authsrv.
 * On success, returns 0.  On error, returns non-zero and calls syslog().
 */
ssize_t
tis_send(struct tis_connection *tc, u_char *buf, size_t len)
{
	struct iovec iov[2];
	des_key_schedule ks;
	des_cblock iv;
	ssize_t nwritten;
	size_t n;
	u_char cbuf[TIS_BUFSIZ];

	if (tc->keyfile != NULL) {
		memcpy(ks, tc->keysched, sizeof(ks));
		memset(iv, 0, sizeof(iv));

		len++;				/* we need to encrypt the NUL */
		if ((n = len % 8) != 0)
			len += 8 - n;		/* make multiple of 8 bytes */
		if (len * 2 > sizeof(cbuf)) {
			syslog(LOG_ERR, "encoded data too large to store");
			return (-1);
		}
		des_ncbc_encrypt((des_cblock *)buf, (des_cblock *)cbuf, len,
		    ks, &iv, DES_ENCRYPT);
		len = tis_encode(cbuf, len, sizeof(cbuf));
		buf = cbuf;
	}
	iov[0].iov_base = buf;
	iov[0].iov_len = len;
	iov[1].iov_base = "\n";
	iov[1].iov_len = 1;

	alarm(tc->timeout);
	nwritten = writev(tc->fd, iov, 2);
	alarm(0);
	if (nwritten != len + 1) {
		if (nwritten < 0)
			syslog(LOG_ERR, "error writing to authsrv: %m");
		else
			syslog(LOG_ERR, "short write sending to authsrv");
		return (-1);
	}
	return (nwritten - 1);		/* don't include the newline */
}

/*
 * Convert a stream of bytes hex digits to hex octects in place.
 * The passed in buffer must have space for len*2 bytes
 * plus a NUL.
 */
ssize_t
tis_encode(u_char *buf, size_t inlen, size_t bufsiz)
{
	u_char *in, *out;
	size_t outlen;
	const static char hextab[] = "0123456789ABCDEF";

	outlen = inlen * 2;
	if (bufsiz <= outlen)
		return (-1);

	/* convert from the end -> beginning so we can do it in place */
	for (in = &buf[inlen - 1], out = &buf[outlen - 1]; in >= buf; in--) {
		*out-- = hextab[*in & 0x0f];
		*out-- = hextab[*in >> 4];
	}
	buf[outlen] = '\0';

	return (outlen);
}

/*
 * Convert a stream of hex digits to bytes in place.
 */
ssize_t
tis_decode(u_char *buf, size_t len)
{
	u_char *end, *in, *out;
	int byte;

	if (len & 1)
		return (-1);		/* len must be even */

	for (in = out = buf, end = buf + len; in < end; in += 2) {
		if (in[1] >= 'A' && in[1] <= 'F')
			byte = in[1] - 'A' + 10;
		else
			byte = in[1] - '0';
		if (in[0] >= 'A' && in[0] <= 'F')
			byte += (in[0] - 'A' + 10) * 16;
		else
			byte += (in[0] - '0') * 16;
		if (byte > 0xff || byte < 0)
			return (-1);
		*out++ = byte;
	}
	*out = '\0';
	return (out - buf);
}

/*
 * Send an authorization string to authsrv and check the result.
 * Returns the type of response and an output buffer.
 */
enum response_type
tis_authorize(struct tis_connection *tc, const char *user,
    const char *class, char *obuf)
{
	enum response_type resp;
	u_char buf[TIS_BUFSIZ];
	int len;

	*obuf = '\0';
	/* class is not used by authsrv (it is effectively a comment) */
	len = snprintf(buf, sizeof(buf), "authenticate %s %s", user, class);
	if (len == -1 || len >= sizeof(buf)) {
		syslog(LOG_ERR, "user/class too large");
		resp = error;
	} else if (tis_send(tc, buf, len) < 0)
		resp = error;
	else if (tis_recv(tc, buf, sizeof(buf)) < 0)
		resp = error;
	else if (strncmp(buf, "password", 8) == 0) {
		strlcpy(obuf, "Password:", TIS_BUFSIZ);
		resp = password;
	} else if (strncmp(buf, "challenge ", 10) == 0) {
		strlcpy(obuf, buf + 10, TIS_BUFSIZ);
		resp = challenge;
	} else if (strncmp(buf, "chalnecho ", 10) == 0) {
		strlcpy(obuf, buf + 10, TIS_BUFSIZ);
		resp = chalnecho;
	} else if (strncmp(buf, "display ", 8) == 0) {
		strlcpy(obuf, buf, TIS_BUFSIZ);
		resp = display;
	} else {
		syslog(LOG_ERR, "unexpected response from authsrv: %s", obuf);
		resp = error;
	}
	memset(buf, 0, sizeof(buf));

	return (resp);
}

/*
 * Send a response string to authsrv and check the result.
 * Returns the type of response, and an error buffer.
 */
int
tis_verify(struct tis_connection *tc, const char *response, char *ebuf)
{
	u_char buf[TIS_BUFSIZ];
	int len;

	ebuf[0] = '\0';
	len = snprintf(buf, sizeof(buf), "response '%s'", response);
	if (len == -1 || len >= sizeof(buf)) {
		syslog(LOG_ERR, "response too large");
		return (-1);
	}
	if (tis_send(tc, buf, len) < 0)
		return (-1);
	if (tis_recv(tc, buf, sizeof(buf)) < 0)
		return (-1);
	if (strncmp(buf, "ok", 2) == 0) {
		if (buf[2] != '\0')
			strlcpy(ebuf, buf + 3, TIS_BUFSIZ);
		memset(buf, 0, sizeof(buf));
		return (0);
	}
	strlcpy(ebuf, buf, TIS_BUFSIZ);
	memset(buf, 0, sizeof(buf));
	return (-1);
}
