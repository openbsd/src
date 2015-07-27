/*	$OpenBSD: radiusd_local.h,v 1.2 2015/07/27 08:58:09 yasuoka Exp $	*/

/*
 * Copyright (c) 2013 Internet Initiative Japan Inc.
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

#include <sys/socket.h>		/* for struct sockaddr_storage */
#include <sys/queue.h>		/* for TAILQ_* */
#include <netinet/in.h>		/* for struct sockaddr_in* */

#include <event.h>		/* for struct event */
#include <imsg.h>		/* for struct imsgbuf */
#include <stdarg.h>		/* for va_list */
#include <stdbool.h>		/* for bool */

#include <radius.h>		/* for RADIUS_PACKET */

#define	MODULE_IO_TIMEOUT	2000

#define	CONFFILE			"/etc/radiusd.conf"
struct radius_query;	/* XXX */

struct radiusd_addr {
	union {
		struct in_addr			 ipv4;
		struct in6_addr			 ipv6;
		uint32_t			 addr32[4];
	} addr;
};

struct radiusd_listen {
	struct radiusd				*radiusd;
	struct event				 ev;
	int					 sock;
	union {
		struct sockaddr_in		 ipv4;
		struct sockaddr_in6		 ipv6;
	} addr;
	int					 stype;
	int					 sproto;
	TAILQ_ENTRY(radiusd_listen)		 next;
};

TAILQ_HEAD(radiusd_listen_head, radiusd_listen);

struct radiusd_client {
	char					 secret[RADIUSD_SECRET_MAX];
	bool					 msgauth_required;
	int					 af;
	struct radiusd_addr			 addr;
	struct radiusd_addr			 mask;
	TAILQ_ENTRY(radiusd_client)		 next;
};

struct radiusd_module {
	char				 name[RADIUSD_MODULE_NAME_LEN];
	struct radiusd			*radiusd;
	pid_t				 pid;
	int				 fd;
	struct imsgbuf			 ibuf;
	struct event			 ev;
	bool				 writeready;
	bool				 stopped;
	uint32_t			 capabilities;
	u_char				*radpkt;
	int 				 radpktsiz;
	int				 radpktoff;
	char				*secret;
	TAILQ_ENTRY(radiusd_module)	 next;
	int	 (*request_decoration)(void *, struct radius_query *);
	int	 (*response_decoration)(void *, struct radius_query *);
};

struct radiusd_module_ref {
	struct radiusd_module		*module;
	TAILQ_ENTRY(radiusd_module_ref)	 next;
};

struct radiusd_authentication {
	char					**username;
	char					 *secret;
	struct radiusd_module_ref 	  	 *auth;
	TAILQ_HEAD(,radiusd_module_ref) 	  deco;
	TAILQ_ENTRY(radiusd_authentication)	  next;
};

struct radiusd {
	struct radiusd_listen_head		 listen;
	struct event				 ev_sigterm;
	struct event				 ev_sighup;
	struct event				 ev_sigint;
	struct event				 ev_sigchld;
	TAILQ_HEAD(,radiusd_module) 	 	 module;
	TAILQ_HEAD(,radiusd_authentication)	 authen;
	TAILQ_HEAD(,radiusd_client)		 client;
	TAILQ_HEAD(,radius_query)		 query;
};

struct radius_query {
	u_int				 id;
	struct sockaddr_storage		 clientaddr;
	int				 clientaddrlen;
	int				 req_id;
	u_char				 req_auth[16];
	struct radiusd_listen		*listen;
	struct radiusd_client		*client;
	struct radiusd_authentication	*authen;
	RADIUS_PACKET			*req;
	RADIUS_PACKET			*res;
	int				 req_modified;
	int				 res_modified;
	char				 username[256]; /* original username */
	TAILQ_ENTRY(radius_query)	 next;
};
#ifndef nitems
#define nitems(_x)    (sizeof((_x)) / sizeof((_x)[0]))
#endif

#ifdef RADIUSD_DEBUG
#define	RADIUSD_DBG(x)	log_debug x
#else
#define	RADIUSD_DBG(x)
#endif
#define	RADIUSD_ASSERT(_cond)					\
	do {							\
		if (!(_cond)) {					\
			log_warnx(				\
			    "ASSERT(%s) failed in %s() at %s:%d",\
			    #_cond, __func__, __FILE__, __LINE__);\
			if (debug) abort();			\
		}						\
	} while (0/* CONSTCOND */)


#define	MODULE_DO_USERPASS(_m)					\
	((_m)->fd >= 0 &&					\
	    ((_m)->capabilities & RADIUSD_MODULE_CAP_USERPASS) != 0)
#define	MODULE_DO_ACCSREQ(_m)					\
	((_m)->fd >= 0 &&					\
	    ((_m)->capabilities & RADIUSD_MODULE_CAP_ACCSREQ) != 0)

extern struct radiusd_module mod_standard;
extern struct radiusd_module mod_radius;

int	 parse_config(const char *, struct radiusd *);
void	 radiusd_conf_init(struct radiusd *);


struct radiusd_module	*radiusd_module_load(struct radiusd *, const char *,
			    const char *);
void			 radiusd_module_unload(struct radiusd_module *);

void	 	 radiusd_access_request_answer(struct radius_query *);
int	 	 radiusd_access_request_fixup(struct radius_query *);
void	 	 radiusd_access_request_aborted(struct radius_query *);
void		 radius_attr_hide(const char *, const char *, const u_char *,
		    u_char *, int);
void		 radius_attr_unhide(const char *, const char *, const u_char *,
		    u_char *, int);

int radiusd_module_set(struct radiusd_module *, const char *, int, char * const *);
