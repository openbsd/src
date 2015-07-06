/*	$OpenBSD: syslogd.h,v 1.18 2015/07/06 16:12:16 millert Exp $ */

/*
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

/* Privilege separation */
int   priv_init(char *, int, int, int, char **);
int   priv_open_tty(const char *);
int   priv_open_log(const char *);
FILE *priv_open_utmp(void);
FILE *priv_open_config(void);
void  priv_config_parse_done(void);
int   priv_config_modified(void);
int   priv_getaddrinfo(char *, char *, char *, struct sockaddr *, size_t);
int   priv_getnameinfo(struct sockaddr *, socklen_t, char *, size_t);

/* Terminal message */
char *ttymsg(struct iovec *, int, char *, int);

/* File descriptor send/recv */
void send_fd(int, int);
int  receive_fd(int);

/* The list of domain sockets */
#define MAXUNIX	21
extern int nunix;
extern char *path_unix[MAXUNIX];
extern char *path_ctlsock;
extern int fd_ctlsock, fd_ctlconn, fd_klog, fd_sendsys;
extern int fd_udp, fd_udp6, fd_bind, fd_unix[MAXUNIX];

#define dprintf(_f...)	do { if (Debug) printf(_f); } while (0)
extern int Debug;
extern int Startup;

struct ringbuf {
	char *buf;
	size_t len, start, end;
};

struct ringbuf *ringbuf_init(size_t);
void		ringbuf_free(struct ringbuf *);
void		ringbuf_clear(struct ringbuf *);
size_t		ringbuf_used(struct ringbuf *);
int		ringbuf_append_line(struct ringbuf *, char *);
ssize_t		ringbuf_to_string(char *, size_t, struct ringbuf *);
