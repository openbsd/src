/*	$OpenBSD: ftp.h,v 1.2 2019/05/12 20:58:19 jasper Exp $ */

/*
 * Copyright (c) 2015 Sunil Nimmagadda <sunil@openbsd.org>
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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#define	S_HTTP	0
#define S_FTP	1
#define S_FILE	2
#define S_HTTPS	3

#define TMPBUF_LEN	131072
#define	IMSG_OPEN	1

#define P_PRE	100
#define P_OK	200
#define P_INTER	300
#define N_TRANS	400
#define	N_PERM	500

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

struct imsg;
struct imsgbuf;

struct url {
	int	 scheme;
	int	 ipliteral;
	char	*host;
	char	*port;
	char	*path;
	char	*basic_auth;

	char	*fname;
	int	 chunked;
};

/* cmd.c */
void	cmd(const char *, const char *, const char *);

/* main.c */
extern struct imsgbuf	 child_ibuf;
extern const char	*useragent;
extern int		 activemode, family, io_debug, verbose, progressmeter;
extern volatile sig_atomic_t interrupted;
extern FILE		*msgout;

/* file.c */
struct url	*file_request(struct imsgbuf *, struct url *, off_t *, off_t *);
void		 file_save(struct url *, FILE *, off_t *);

/* ftp.c */
void		 ftp_connect(struct url *, struct url *, int);
struct url	*ftp_get(struct url *, struct url *, off_t *, off_t *);
void		 ftp_quit(struct url *);
void		 ftp_save(struct url *, FILE *, off_t *);
int		 ftp_auth(FILE *, const char *, const char *);
int		 ftp_command(FILE *, const char *, ...)
		     __attribute__((__format__ (printf, 2, 3)))
		     __attribute__((__nonnull__ (2)));
int		 ftp_eprt(FILE *);
int		 ftp_epsv(FILE *);
int		 ftp_getline(char **, size_t *, int, FILE *);
int		 ftp_size(FILE *, const char *, off_t *, char **);

/* http.c */
void		 http_connect(struct url *, struct url *, int);
struct url	*http_get(struct url *, struct url *, off_t *, off_t *);
void		 http_close(struct url *);
void		 http_save(struct url *, FILE *, off_t *);
void		 https_init(char *);

/* progressmeter.c */
void	start_progress_meter(const char *, const char *, off_t, off_t *);
void	stop_progress_meter(void);

/* url.c */
int		 scheme_lookup(const char *);
void		 url_connect(struct url *, struct url *, int);
char		*url_encode(const char *);
void		 url_free(struct url *);
struct url	*url_parse(const char *);
struct url	*url_request(struct url *, struct url *, off_t *, off_t *);
void		 url_save(struct url *, FILE *, off_t *);
void		 url_close(struct url *);
char		*url_str(struct url *);
void	 	 log_request(const char *, struct url *, struct url *);

/* util.c */
int	connect_wait(int);
void	copy_file(FILE *, FILE *, off_t *);
int	tcp_connect(const char *, const char *, int);
int	fd_request(char *, int, off_t *);
int	read_message(struct imsgbuf *, struct imsg *);
void	send_message(struct imsgbuf *, int, uint32_t, void *, size_t, int);
void	log_info(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)))
	    __attribute__((__nonnull__ (1)));
