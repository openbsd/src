/*	$OpenBSD: ntpd.h,v 1.5 2004/06/05 12:29:15 alexander Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>

#include "ntp.h"

#define	NTPD_USER	"_ntp"
#define	CONFFILE	"/etc/ntpd.conf"

#define	READ_BUF_SIZE	65535

#define	NTPD_OPT_VERBOSE		0x0001
#define	NTPD_OPT_VERBOSE2		0x0002

struct listen_addr {
	TAILQ_ENTRY(listen_addr)	 entry;
	struct sockaddr_storage		 sa;
	int				 fd;
};

struct ntpd_conf {
	TAILQ_HEAD(listen_addrs, listen_addr)	listen_addrs;
	u_int8_t				opts;
};

struct buf {
	TAILQ_ENTRY(buf)	 entries;
	u_char			*buf;
	ssize_t			 size;
	ssize_t			 wpos;
	ssize_t			 rpos;
};

struct msgbuf {
	u_int32_t		 queued;
	int			 fd;
	TAILQ_HEAD(bufs, buf)	 bufs;
};

struct buf_read {
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
	ssize_t			 wpos;
};

/* ipc messages */

#define	IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define	MAX_IMSGSIZE		8192

struct imsgbuf {
	int			fd;
	pid_t			pid;
	struct buf_read		r;
	struct msgbuf		w;
};

enum imsg_type {
	IMSG_NONE
};

struct imsg_hdr {
	enum imsg_type	type;
	u_int16_t	len;
	u_int32_t	peerid;
	pid_t		pid;
};

struct imsg {
	struct imsg_hdr	 hdr;
	void		*data;
};

/* prototypes */
/* log.c */
void		 log_init(int);
void		 vlog(int, const char *, va_list);
void		 log_warn(const char *, ...);
void		 log_warnx(const char *, ...);
void		 log_info(const char *, ...);
void		 log_debug(const char *, ...);
void		 fatal(const char *);
void		 fatalx(const char *);

/* buffer.c */
struct buf	*buf_open(ssize_t);
int		 buf_add(struct buf *, void *, ssize_t);
int		 buf_close(struct msgbuf *, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);

/* imsg.c */
void	 imsg_init(struct imsgbuf *, int);
int	 imsg_read(struct imsgbuf *);
int	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, int, u_int32_t, void *, u_int16_t);
int	 imsg_compose_pid(struct imsgbuf *, int, pid_t, void *, u_int16_t);
struct buf *imsg_create(struct imsgbuf *, int, u_int32_t, u_int16_t);
struct buf *imsg_create_pid(struct imsgbuf *, int, pid_t, u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);

/* ntp.c */
pid_t	 ntp_main(int[2], struct ntpd_conf *);

/* parse.y */
int	 parse_config(char *, struct ntpd_conf *);
int	 cmdline_symset(char *);

/* config.c */
int	 check_file_secrecy(int, const char *);
int	 host(const char *, struct sockaddr *, u_int8_t *);

/* ntp_msg.c */
void	get_ts(struct l_fixedpt *);
int	ntp_getmsg(char *, ssize_t, struct ntp_msg *);
int	ntp_sendmsg(int, struct sockaddr *, struct ntp_msg *, ssize_t, int);

/* server.c */
int	setup_listeners(struct servent *, struct ntpd_conf *);
int	ntp_reply(int, struct sockaddr *, struct ntp_msg *, int);
