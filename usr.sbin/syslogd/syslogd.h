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

/* Privilege separation */
int   priv_init(char *, int, int, int, char **);
int   priv_open_tty(const char *);
int   priv_open_log(const char *);
FILE *priv_open_utmp(void);
FILE *priv_open_config(void);
void  priv_config_parse_done(void);
int   priv_config_modified(void);
int   priv_gethostbyname(char *, char *, size_t);
int   priv_gethostbyaddr(char *, int, int, char *, size_t);

/* Terminal message */
char *ttymsg(struct iovec *, int, char *, int);

/* File descriptor send/recv */
void send_fd(int, int);
int  receive_fd(int);

/* The list of domain sockets */
#define MAXFUNIX	21
extern int nfunix;
extern char *funixn[MAXFUNIX];
extern int funix[MAXFUNIX];
extern int finet;
extern int fklog;

#define dprintf		if (Debug) printf
extern int Debug;
extern int Startup;
