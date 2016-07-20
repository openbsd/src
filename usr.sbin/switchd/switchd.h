/*	$OpenBSD: switchd.h,v 1.5 2016/07/20 21:01:06 reyk Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _SWITCHD_H
#define _SWITCHD_H

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/uio.h>

#include <limits.h>
#include <imsg.h>

#include "ofp.h"
#include "ofp10.h"
#include "types.h"
#include "proc.h"

struct switchd;

struct timer {
	struct event	 tmr_ev;
	struct switchd	*tmr_sc;
	void		(*tmr_cb)(struct switchd *, void *);
	void		*tmr_cbarg;
};

struct packet {
	union {
		struct ether_header	*pkt_eh;
		uint8_t			*pkt_buf;
	};
	size_t				 pkt_len;
};

struct macaddr {
	uint8_t			 mac_addr[ETHER_ADDR_LEN];
	uint32_t			 mac_port;
	time_t			 mac_age;
	RB_ENTRY(macaddr)	 mac_entry;
};
RB_HEAD(macaddr_head, macaddr);

struct switch_control {
	unsigned int		 sw_id;
	struct sockaddr_storage	 sw_addr;
	struct macaddr_head	 sw_addrcache;
	struct timer		 sw_timer;
	unsigned int		 sw_cachesize;
	RB_ENTRY(switch_control) sw_entry;
};
RB_HEAD(switch_head, switch_control);

struct switch_connection {
	unsigned int		 con_id;
	int			 con_fd;
	struct sockaddr_storage	 con_peer;
	struct sockaddr_storage	 con_local;
	struct switch_control	*con_switch;
	in_port_t		 con_port;
	struct event		 con_ev;
	struct switchd		*con_sc;
	uint32_t		 con_xidnxt;
	TAILQ_ENTRY(switch_connection)
				 con_next;
};

struct switch_server {
	int			 srv_fd;
	int			 srv_tls;
	struct sockaddr_storage	 srv_addr;
	struct event		 srv_ev;
	struct switchd		*srv_sc;
};

struct switch_controller {
	enum switch_conn_type	 swc_type;
	struct sockaddr_storage	 swc_addr;
};

struct switch_device {
	char			 sdv_device[PATH_MAX];
	struct switch_controller sdv_swc;
	TAILQ_ENTRY(switch_device)
				 sdv_next;
};

struct switchd {
	struct privsep		 sc_ps;
	struct switch_server	 sc_server;
	int			 sc_tap;
	struct switch_head	 sc_switches;
	uint32_t		 sc_swid;
	unsigned int		 sc_cache_max;
	unsigned int		 sc_cache_timeout;
	char			 sc_conffile[PATH_MAX];
	uint8_t			 sc_opts;
	TAILQ_HEAD(, switch_device)
				 sc_conns;
};

struct ofp_callback {
	uint8_t		 cb_type;
	int		(*cb)(struct switchd *, struct switch_connection *,
			    struct ofp_header *, struct ibuf *);
	int		(*debug)(struct switchd *, struct sockaddr_storage *,
			    struct sockaddr_storage *, struct ofp_header *,
			    struct ibuf *);
};

#define SWITCHD_OPT_VERBOSE		0x01
#define SWITCHD_OPT_NOACTION		0x04

/* switchd.c */
int		 switchd_socket(struct sockaddr *, int);
int		 switchd_listen(struct sockaddr *);
int		 switchd_sockaddr(const char *, in_port_t, struct sockaddr_storage *);
int		 switchd_tap(void);
int		 switchd_open_device(struct privsep *, const char *, size_t);

/* packet.c */
uint32_t	 packet_input(struct switchd *, struct switch_control *,
		    uint32_t, struct ibuf *, size_t, struct packet *);

/* switch.c */
void		 switch_init(struct switchd *);
int		 switch_dispatch_control(int, struct privsep_proc *,
		    struct imsg *);
struct switch_control
		*switch_add(struct switch_connection *);
void		 switch_remove(struct switchd *, struct switch_control *);
struct switch_control
		*switch_get(struct switch_connection *);
struct macaddr	*switch_learn(struct switchd *, struct switch_control *,
		    uint8_t *, uint32_t);
struct macaddr	*switch_cached(struct switch_control *, uint8_t *);
RB_PROTOTYPE(switch_head, switch_control, sw_entry, switch_cmp);
RB_PROTOTYPE(macaddr_head, macaddr, mac_entry, switch_maccmp);

/* timer.c */
void		 timer_set(struct switchd *, struct timer *,
		    void (*)(struct switchd *, void *), void *);
void		 timer_add(struct switchd *, struct timer *, int);
void		 timer_del(struct switchd *, struct timer *);

/* util.c */
void		 socket_set_blockmode(int, enum blockmodes);
in_port_t	 socket_getport(struct sockaddr_storage *);
int		 sockaddr_cmp(struct sockaddr *, struct sockaddr *, int);
struct in6_addr *prefixlen2mask6(uint8_t, uint32_t *);
uint32_t	 prefixlen2mask(uint8_t);
const char	*print_host(struct sockaddr_storage *, char *, size_t);
const char	*print_ether(const uint8_t *)
		    __attribute__ ((__bounded__(__minbytes__,1,ETHER_ADDR_LEN)));
const char	*print_map(unsigned int, struct constmap *);
void		 print_verbose(const char *emsg, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		 print_debug(const char *emsg, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		 getmonotime(struct timeval *);
int		 parsehostport(const char *, struct sockaddr *, socklen_t);

/* ofp.c */
pid_t		 ofp(struct privsep *, struct privsep_proc *);
void		 ofp_close(struct switch_connection *);
void		 ofp_read(int, short, void *);
int		 ofp_send(struct switch_connection *, struct ofp_header *,
		    struct ibuf *);
void		 ofp_accept(int, short, void *);

/* ofp10.c */
int		 ofp10_hello(struct switchd *, struct switch_connection *,
		    struct ofp_header *, struct ibuf *);
void		 ofp10_debug(struct switchd *,
		    struct sockaddr_storage *, struct sockaddr_storage *,
		    struct ofp_header *, struct ibuf *);
int		 ofp10_input(struct switchd *, struct switch_connection *,
		    struct ofp_header *, struct ibuf *);

/* ofp13.c */
int		 ofp13_input(struct switchd *, struct switch_connection *,
		    struct ofp_header *, struct ibuf *);

/* ofcconn.c */
pid_t		 ofcconn(struct privsep *, struct privsep_proc *);
void		 ofcconn_shutdown(void);

/* imsg_util.c */
struct ibuf	*ibuf_new(void *, size_t);
struct ibuf	*ibuf_static(void);
int		 ibuf_cat(struct ibuf *, struct ibuf *);
void		 ibuf_release(struct ibuf *);
size_t		 ibuf_length(struct ibuf *);
int		 ibuf_setsize(struct ibuf *, size_t);
uint8_t		*ibuf_data(struct ibuf *);
void		*ibuf_getdata(struct ibuf *, size_t);
ssize_t		 ibuf_dataleft(struct ibuf *);
size_t		 ibuf_dataoffset(struct ibuf *);
struct ibuf	*ibuf_get(struct ibuf *, size_t);
struct ibuf	*ibuf_dup(struct ibuf *);
struct ibuf	*ibuf_random(size_t);
int		 ibuf_prepend(struct ibuf *, void *, size_t);
void		*ibuf_advance(struct ibuf *, size_t);
void		 ibuf_zero(struct ibuf *);
void		 ibuf_reset(struct ibuf *);

/* parse.y */
int		 cmdline_symset(char *);
int		 parse_config(const char *, struct switchd *);

#endif /* _SWITCHD_H */
