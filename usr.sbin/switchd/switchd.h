/*	$OpenBSD: switchd.h,v 1.30 2019/11/21 06:22:57 akoshibe Exp $	*/

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

#ifndef SWITCHD_H
#define SWITCHD_H

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/uio.h>

#include <net/ofp.h>

#include <limits.h>
#include <imsg.h>

#include "ofp10.h"
#include "types.h"
#include "proc.h"
#include "ofp_map.h"

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

struct switch_table {
	TAILQ_ENTRY(switch_table)	 st_entry;

	int				 st_table;
	unsigned int			 st_entries;
	unsigned int			 st_maxentries;

	uint32_t			 st_actions;
	uint32_t			 st_actionsmiss;

	uint32_t			 st_instructions;
	uint32_t			 st_instructionsmiss;

	uint64_t			 st_setfield;
	uint64_t			 st_setfieldmiss;
	uint64_t			 st_match;
	uint64_t			 st_wildcard;

	/* Maximum of 256 tables (64 * 4). */
	uint64_t			 st_nexttable[4];
	uint64_t			 st_nexttablemiss[4];
};
TAILQ_HEAD(switch_table_list, switch_table);

struct multipart_message {
	SLIST_ENTRY(multipart_message)
				 mm_entry;

	uint32_t		 mm_xid;
	uint8_t			 mm_type;
};
SLIST_HEAD(multipart_list, multipart_message);

struct switch_address {
	enum switch_conn_type	 swa_type;
	struct sockaddr_storage	 swa_addr;
};

struct switch_connection {
	unsigned int		 con_id;
	unsigned int		 con_instance;

	int			 con_fd;
	int			 con_inflight;

	struct sockaddr_storage	 con_peer;
	struct sockaddr_storage	 con_local;
	in_port_t		 con_port;
	uint32_t		 con_xidnxt;
	int			 con_version;
	enum ofp_state		 con_state;

	struct event		 con_ev;
	struct ibuf		*con_rbuf;
	struct msgbuf		 con_wbuf;

	struct switch_control	*con_switch;
	struct switchd		*con_sc;
	struct switch_server	*con_srv;

	struct multipart_list	 con_mmlist;
	struct switch_table_list
				 con_stlist;

	TAILQ_ENTRY(switch_connection)
				 con_entry;
};
TAILQ_HEAD(switch_connections, switch_connection);

struct switch_server {
	int			 srv_fd;
	int			 srv_tls;
	struct sockaddr_storage	 srv_addr;
	struct event		 srv_ev;
	struct event		 srv_evt;
	struct switchd		*srv_sc;
};

struct switch_client {
	struct switch_address	 swc_addr;
	struct switch_address	 swc_target;
	struct event		 swc_ev;
	void			*swc_arg;
	TAILQ_ENTRY(switch_client)
				 swc_next;
};
TAILQ_HEAD(switch_clients, switch_client);

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
	struct switch_clients	 sc_clients;
	struct switch_connections
				 sc_conns;
};

struct ofp_callback {
	uint8_t		 cb_type;
	int		(*cb)(struct switchd *, struct switch_connection *,
			    struct ofp_header *, struct ibuf *);
	int		(*validate)(struct switchd *, struct sockaddr_storage *,
			    struct sockaddr_storage *, struct ofp_header *,
			    struct ibuf *);
};

#define SWITCHD_OPT_VERBOSE		0x01
#define SWITCHD_OPT_NOACTION		0x04

struct oflowmod_ctx {
	uint8_t				 ctx_flags;
#define OFMCTX_IBUF			 0x01
	enum oflowmod_state		 ctx_state;

	struct ibuf			*ctx_ibuf;
	struct ofp_flow_mod		*ctx_fm;
	size_t				 ctx_start;
	size_t				 ctx_ostart;
	size_t				 ctx_oend;
	size_t				 ctx_istart;
	size_t				 ctx_iend;
	size_t				 ctx_oioff;
	struct ofp_instruction		*ctx_oi;
};

/* switchd.c */
int		 switchd_socket(struct sockaddr *, int);
int		 switchd_listen(struct sockaddr *);
int		 switchd_sockaddr(const char *, in_port_t, struct sockaddr_storage *);
int		 switchd_tap(void);
int		 switchd_open_device(struct privsep *, const char *, size_t);
struct switch_connection *
		 switchd_connbyid(struct switchd *, unsigned int, unsigned int);
struct switch_connection *
		 switchd_connbyaddr(struct switchd *, struct sockaddr *);

/* packet.c */
int		 packet_ether_input(struct ibuf *, size_t, struct packet *);
int		 packet_input(struct switchd *, struct switch_control *,
		    uint32_t, uint32_t *, struct packet *);

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
int		 accept4_reserve(int, struct sockaddr *, socklen_t *,
		    int, int, volatile int *);
in_port_t	 socket_getport(struct sockaddr_storage *);
int		 socket_setport(struct sockaddr_storage *, in_port_t);
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
void		 print_hex(uint8_t *, off_t, size_t);
void		 getmonotime(struct timeval *);
int		 parsehostport(const char *, struct sockaddr *, socklen_t);

/* ofrelay.c */
void		 ofrelay(struct privsep *, struct privsep_proc *);
void		 ofrelay_run(struct privsep *, struct privsep_proc *, void *);
int		 ofrelay_attach(struct switch_server *, int,
		    struct sockaddr *);
void		 ofrelay_close(struct switch_connection *);
void		 ofrelay_write(struct switch_connection *, struct ibuf *);

/* ofp.c */
void		 ofp(struct privsep *, struct privsep_proc *);
void		 ofp_close(struct switch_connection *);
int		 ofp_open(struct privsep *, struct switch_connection *);
void		 ofp_accept(int, short, void *);
int		 ofp_input(struct switch_connection *, struct ibuf *);
int		 ofp_nextstate(struct switchd *, struct switch_connection *,
		    enum ofp_state);
struct switch_table *
		 switch_tablelookup(struct switch_connection *, int);
struct switch_table *
		 switch_newtable(struct switch_connection *, int);
void		 switch_deltable(struct switch_connection *,
		    struct switch_table *);
void		 switch_freetables(struct switch_connection *);

/* ofp10.c */
int		 ofp10_hello(struct switchd *, struct switch_connection *,
		    struct ofp_header *, struct ibuf *);
int		 ofp10_validate(struct switchd *,
		    struct sockaddr_storage *, struct sockaddr_storage *,
		    struct ofp_header *, struct ibuf *);
int		 ofp10_input(struct switchd *, struct switch_connection *,
		    struct ofp_header *, struct ibuf *);

/* ofp13.c */
int		 ofp13_input(struct switchd *, struct switch_connection *,
		    struct ofp_header *, struct ibuf *);
int		 ofp13_hello(struct switchd *, struct switch_connection *,
		    struct ofp_header *oh, struct ibuf *);
int		 ofp13_validate(struct switchd *,
		    struct sockaddr_storage *, struct sockaddr_storage *,
		    struct ofp_header *, struct ibuf *);
int		 ofp13_desc(struct switchd *, struct switch_connection *);
int		 ofp13_flow_stats(struct switchd *, struct switch_connection *,
		    uint32_t, uint32_t, uint8_t);
int		 ofp13_table_features(struct switchd *,
		    struct switch_connection *, uint8_t);
int		 ofp13_featuresrequest(struct switchd *,
		    struct switch_connection *);
struct ofp_flow_mod *
		 ofp13_flowmod(struct switch_connection *, struct ibuf *,
		    uint8_t);
int		 ofp13_setconfig(struct switchd *, struct switch_connection *,
		    uint16_t, uint16_t);
int		 ofp13_tablemiss_sendctrl(struct switchd *,
		    struct switch_connection *, uint8_t);

/* ofp_common.c */
int		 ofp_validate_header(struct switchd *,
		    struct sockaddr_storage *, struct sockaddr_storage *,
		    struct ofp_header *, uint8_t);
int		 ofp_validate(struct switchd *,
		    struct sockaddr_storage *, struct sockaddr_storage *,
		    struct ofp_header *, struct ibuf *, uint8_t);
int		 ofp_output(struct switch_connection *, struct ofp_header *,
		    struct ibuf *);
struct multipart_message *
		    ofp_multipart_lookup(struct switch_connection *, uint32_t);
int		 ofp_multipart_add(struct switch_connection *, uint32_t,
		    uint8_t);
void		 ofp_multipart_del(struct switch_connection *, uint32_t);
void		 ofp_multipart_free(struct switch_connection *,
		    struct multipart_message *);
void		 ofp_multipart_clear(struct switch_connection *);
int		 action_new(struct ibuf *, uint16_t);
int		 action_group(struct ibuf *, uint32_t);
int		 action_output(struct ibuf *, uint32_t, uint16_t);
int		 action_push(struct ibuf *, uint16_t, uint16_t);
int		 action_pop_vlan(struct ibuf *);
int		 action_pop_mpls(struct ibuf *, uint16_t);
int		 action_copyttlout(struct ibuf *);
int		 action_copyttlin(struct ibuf *);
int		 action_decnwttl(struct ibuf *);
struct ofp_action_set_field *
		 action_setfield(struct ibuf *ibuf);
struct ofp_ox_match *
		 oxm_get(struct ibuf *, uint16_t, int, uint8_t);
int		 oxm_inport(struct ibuf *, uint32_t);
int		 oxm_inphyport(struct ibuf *, uint32_t);
int		 oxm_metadata(struct ibuf *, int, uint64_t, uint64_t);
int		 oxm_etheraddr(struct ibuf *, int, uint8_t *, uint8_t *);
int		 oxm_ethertype(struct ibuf *, uint16_t);
int		 oxm_vlanvid(struct ibuf *, int, uint16_t, uint16_t);
int		 oxm_vlanpcp(struct ibuf *, uint8_t);
int		 oxm_ipdscp(struct ibuf *, uint8_t);
int		 oxm_ipecn(struct ibuf *, uint8_t);
int		 oxm_ipproto(struct ibuf *, uint8_t);
int		 oxm_ipaddr(struct ibuf *, int, int, uint32_t, uint32_t);
int		 oxm_tcpport(struct ibuf *, int, uint16_t);
int		 oxm_udpport(struct ibuf *, int, uint16_t);
int		 oxm_sctpport(struct ibuf *, int, uint16_t);
int		 oxm_icmpv4type(struct ibuf *, uint8_t);
int		 oxm_icmpv4code(struct ibuf *, uint8_t);
int		 oxm_arpop(struct ibuf *, uint16_t);
int		 oxm_arpaddr(struct ibuf *, int, int, uint32_t, uint32_t);
int		 oxm_arphaddr(struct ibuf *, int, uint8_t *, uint8_t *);
int		 oxm_ipv6addr(struct ibuf *, int, struct in6_addr *,
		    struct in6_addr *);
int		 oxm_ipv6flowlabel(struct ibuf *, int, uint32_t, uint32_t);
int		 oxm_icmpv6type(struct ibuf *, uint8_t);
int		 oxm_icmpv6code(struct ibuf *, uint8_t);
int		 oxm_ipv6ndtarget(struct ibuf *, struct in6_addr *);
int		 oxm_ipv6ndlinkaddr(struct ibuf *, int, uint8_t *);
int		 oxm_mplslabel(struct ibuf *, uint32_t);
int		 oxm_mplstc(struct ibuf *, uint8_t);
int		 oxm_mplsbos(struct ibuf *, uint8_t);
int		 oxm_tunnelid(struct ibuf *, int, uint64_t, uint64_t);
int		 oxm_ipv6exthdr(struct ibuf *, int, uint16_t, uint16_t);
struct ofp_instruction *
		 ofp_instruction(struct ibuf *, uint16_t, uint16_t);
struct ibuf *
		 oflowmod_open(struct oflowmod_ctx *,
		    struct switch_connection *, struct ibuf *, uint8_t);
int		 oflowmod_close(struct oflowmod_ctx *);
int		 oflowmod_mopen(struct oflowmod_ctx *);
int		 oflowmod_mclose(struct oflowmod_ctx *);
int		 oflowmod_iopen(struct oflowmod_ctx *);
int		 oflowmod_iclose(struct oflowmod_ctx *);
int		 oflowmod_instruction(struct oflowmod_ctx *, unsigned int);

int		 oflowmod_instructionclose(struct oflowmod_ctx *);
int		 oflowmod_state(struct oflowmod_ctx *,
		    unsigned int, unsigned int);
int		 oflowmod_err(struct oflowmod_ctx *, const char *, int);
int		 ofp_validate_hello(struct switchd *,
		    struct sockaddr_storage *, struct sockaddr_storage *,
		    struct ofp_header *, struct ibuf *);
int		 ofp_recv_hello(struct switchd *, struct switch_connection *,
		    struct ofp_header *, struct ibuf *);
int		 ofp_send_hello(struct switchd *, struct switch_connection *,
		    int);
int		 ofp_send_featuresrequest(struct switchd *,
		    struct switch_connection *);
/* ofcconn.c */
void		 ofcconn(struct privsep *, struct privsep_proc *);
void		 ofcconn_shutdown(void);

/* imsg_util.c */
struct ibuf	*ibuf_new(void *, size_t);
struct ibuf	*ibuf_static(void);
int		 ibuf_cat(struct ibuf *, struct ibuf *);
void		 ibuf_release(struct ibuf *);
size_t		 ibuf_length(struct ibuf *);
int		 ibuf_setsize(struct ibuf *, size_t);
int		 ibuf_setmax(struct ibuf *, size_t);
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

#endif /* SWITCHD_H */
