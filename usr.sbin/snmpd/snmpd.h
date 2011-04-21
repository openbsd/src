/*	$OpenBSD: snmpd.h,v 1.31 2011/04/21 14:55:22 sthen Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _SNMPD_H
#define _SNMPD_H

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/route.h>

#include <ber.h>
#include <snmp.h>

#include <imsg.h>

/*
 * common definitions for snmpd
 */

#define CONF_FILE		"/etc/snmpd.conf"
#define SNMPD_SOCKET		"/var/run/snmpd.sock"
#define SNMPD_USER		"_snmpd"
#define SNMPD_PORT		161
#define SNMPD_TRAPPORT		162

#define SNMPD_MAXSTRLEN		484
#define SNMPD_MAXCOMMUNITYLEN	SNMPD_MAXSTRLEN
#define SNMPD_MAXVARBIND	0x7fffffff
#define SNMPD_MAXVARBINDLEN	1210

#define SMALL_READ_BUF_SIZE	1024
#define READ_BUF_SIZE		65535
#define	RT_BUF_SIZE		16384
#define	MAX_RTSOCK_BUF		(128 * 1024)

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to snmpctl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_END,
	IMSG_CTL_NOTIFY
};

enum {
	PROC_PARENT,	/* Parent process and application interface */
	PROC_SNMPE	/* SNMP engine */
} snmpd_process;

/* initially control.h */
struct control_sock {
	const char	*cs_name;
	struct event	 cs_ev;
	int		 cs_fd;
	int		 cs_restricted;
};

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	void			*data;
	short			 events;
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
#define CTL_CONN_LOCKED		 0x02	/* restricted mode */
	struct imsgev		 iev;

};
TAILQ_HEAD(ctl_connlist, ctl_conn);
extern  struct ctl_connlist ctl_conns;

/*
 * kroute
 */

union kaddr {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
	char			pad[32];
};

struct kroute {
	struct in_addr	prefix;
	struct in_addr	nexthop;
	u_long		ticks;
	u_int16_t	flags;
	u_short		if_index;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

struct kroute6 {
	struct in6_addr	prefix;
	struct in6_addr	nexthop;
	u_long		ticks;
	u_int16_t	flags;
	u_short		if_index;
	u_int8_t	prefixlen;
	u_int8_t	priority;
};

struct kif_addr {
	u_short			 if_index;
	union kaddr		 addr;
	union kaddr		 mask;
	union kaddr		 dstbrd;

	TAILQ_ENTRY(kif_addr)	 entry;
	RB_ENTRY(kif_addr)	 node;
};

struct kif {
	char			 if_name[IF_NAMESIZE];
	char			 if_descr[IFDESCRSIZE];
	u_int8_t		 if_lladdr[ETHER_ADDR_LEN];
	struct if_data		 if_data;
	u_long			 if_ticks;
	int			 if_flags;
	u_short			 if_index;
};

#define F_CONNECTED		0x0001
#define F_STATIC		0x0002
#define F_BLACKHOLE		0x0004
#define F_REJECT		0x0008
#define F_DYNAMIC		0x0010

/*
 * Message Processing Subsystem (mps)
 */

struct oid {
	struct ber_oid		 o_id;
#define o_oid			 o_id.bo_id
#define o_oidlen		 o_id.bo_n

	char			*o_name;

	u_int			 o_flags;

	int			 (*o_get)(struct oid *, struct ber_oid *,
				    struct ber_element **);
	int			 (*o_set)(struct oid *, struct ber_oid *,
				    struct ber_element **);
	struct ber_oid		*(*o_table)(struct oid *, struct ber_oid *,
				    struct ber_oid *);

	long long		 o_val;
	void			*o_data;

	RB_ENTRY(oid)		 o_element;
};

#define OID_ROOT		0x00
#define OID_RD			0x01
#define OID_WR			0x02
#define OID_IFSET		0x04	/* only if user-specified value */
#define OID_DYNAMIC		0x08	/* free allocated data */
#define OID_TABLE		0x10	/* dynamic sub-elements */
#define OID_MIB			0x20	/* root-OID of a supported MIB */
#define OID_KEY			0x40	/* lookup tables */

#define OID_RS			(OID_RD|OID_IFSET)
#define OID_WS			(OID_WR|OID_IFSET)
#define OID_RW			(OID_RD|OID_WR)
#define OID_RWS			(OID_RW|OID_IFSET)

#define OID_TRD			(OID_RD|OID_TABLE)
#define OID_TWR			(OID_WR|OID_TABLE)
#define OID_TRS			(OID_RD|OID_IFSET|OID_TABLE)
#define OID_TWS			(OID_WR|OID_IFSET|OID_TABLE)
#define OID_TRW			(OID_RD|OID_WR|OID_TABLE)
#define OID_TRWS		(OID_RW|OID_IFSET|OID_TABLE)

#define OID_NOTSET(_oid)						\
	(((_oid)->o_flags & OID_IFSET) &&				\
	((_oid)->o_data == NULL) && ((_oid)->o_val == 0))

#define OID(...)		{ { __VA_ARGS__ } }
#define MIBDECL(...)		{ { MIB_##__VA_ARGS__ } }, #__VA_ARGS__
#define MIB(...)		{ { MIB_##__VA_ARGS__ } }, NULL
#define MIBEND			{ { 0 } }, NULL

/*
 * daemon structures
 */

struct snmp_message {
	u_int			 sm_version;
	char			 sm_community[SNMPD_MAXCOMMUNITYLEN];
	u_int			 sm_context;

	struct ber_element	*sm_header;
	struct ber_element	*sm_headerend;

	long long		 sm_request;

	long long		 sm_error;
#define sm_nonrepeaters		 sm_error
	long long		 sm_errorindex;
#define sm_maxrepetitions	 sm_errorindex

	struct ber_element	*sm_pdu;
	struct ber_element	*sm_pduend;

	struct ber_element	*sm_varbind;
	struct ber_element	*sm_varbindresp;
};

/* Defined in SNMPv2-MIB.txt (RFC 3418) */
struct snmp_stats {
	u_int32_t		snmp_inpkts;
	u_int32_t		snmp_outpkts;
	u_int32_t		snmp_inbadversions;
	u_int32_t		snmp_inbadcommunitynames;
	u_int32_t		snmp_inbadcommunityuses;
	u_int32_t		snmp_inasnparseerrs;
	u_int32_t		snmp_intoobigs;
	u_int32_t		snmp_innosuchnames;
	u_int32_t		snmp_inbadvalues;
	u_int32_t		snmp_inreadonlys;
	u_int32_t		snmp_ingenerrs;
	u_int32_t		snmp_intotalreqvars;
	u_int32_t		snmp_intotalsetvars;
	u_int32_t		snmp_ingetrequests;
	u_int32_t		snmp_ingetnexts;
	u_int32_t		snmp_insetrequests;
	u_int32_t		snmp_ingetresponses;
	u_int32_t		snmp_intraps;
	u_int32_t		snmp_outtoobigs;
	u_int32_t		snmp_outnosuchnames;
	u_int32_t		snmp_outbadvalues;
	u_int32_t		snmp_outgenerrs;
	u_int32_t		snmp_outgetrequests;
	u_int32_t		snmp_outgetnexts;
	u_int32_t		snmp_outsetrequests;
	u_int32_t		snmp_outgetresponses;
	u_int32_t		snmp_outtraps;
	int			snmp_enableauthentraps;
	u_int32_t		snmp_silentdrops;
	u_int32_t		snmp_proxydrops;
};

struct address {
	struct sockaddr_storage	 ss;
	in_port_t		 port;

	TAILQ_ENTRY(address)	 entry;

	/* For SNMP trap receivers etc. */
	char			*sa_community;
	struct ber_oid		*sa_oid;
};
TAILQ_HEAD(addresslist, address);

struct snmpd {
	u_int8_t		 sc_flags;
#define SNMPD_F_VERBOSE		 0x01
#define SNMPD_F_NONAMES		 0x02

	const char		*sc_confpath;
	struct address		 sc_address;
	int			 sc_sock;
	struct event		 sc_ev;
	struct timeval		 sc_starttime;

	struct control_sock	 sc_csock;
	struct control_sock	 sc_rcsock;

	char			 sc_rdcommunity[SNMPD_MAXCOMMUNITYLEN];
	char			 sc_rwcommunity[SNMPD_MAXCOMMUNITYLEN];
	char			 sc_trcommunity[SNMPD_MAXCOMMUNITYLEN];

	struct snmp_stats	 sc_stats;

	struct addresslist	 sc_trapreceivers;

	int			 sc_ncpu;
	int64_t			*sc_cpustates;
	int			 sc_rtfilter;
};

/* control.c */
int		 control_init(struct control_sock *);
int		 control_listen(struct control_sock *);
void		 control_accept(int, short, void *);
void		 control_dispatch_imsg(int, short, void *);
void		 control_imsg_forward(struct imsg *);
void		 control_cleanup(struct control_sock *);

void		 session_socket_blockmode(int, enum blockmodes);

/* parse.y */
struct snmpd	*parse_config(const char *, u_int);
int		 cmdline_symset(char *);

/* log.c */
void		 log_init(int);
void		 log_warn(const char *, ...);
void		 log_warnx(const char *, ...);
void		 log_info(const char *, ...);
void		 log_debug(const char *, ...);
__dead void	 fatal(const char *);
__dead void	 fatalx(const char *);
const char	*log_in6addr(const struct in6_addr *);
const char	*print_host(struct sockaddr_storage *, char *, size_t);

void		 imsg_event_add(struct imsgev *);
int		 imsg_compose_event(struct imsgev *, enum imsg_type, u_int32_t,
		    pid_t, int, void *, u_int16_t);

/* kroute.c */
int		 kr_init(void);
void		 kr_shutdown(void);

u_int		 kr_ifnumber(void);
u_long		 kr_iflastchange(void);
int		 kr_updateif(u_int);
u_long		 kr_routenumber(void);

struct kif	*kr_getif(u_short);
struct kif	*kr_getnextif(u_short);
struct kif_addr *kr_getaddr(struct sockaddr *);
struct kif_addr *kr_getnextaddr(struct sockaddr *);

struct kroute	*kroute_first(void);
struct kroute	*kroute_getaddr(in_addr_t, u_int8_t, u_int8_t, int);

/* snmpe.c */
pid_t		 snmpe(struct snmpd *, int [2]);
void		 snmpe_debug_elements(struct ber_element *);

/* trap.c */
void		 trap_init(void);
int		 trap_imsg(struct imsgev *, pid_t);
int		 trap_send(struct ber_oid *, struct ber_element *);

/* mps.c */
struct ber_element *
		 mps_getreq(struct ber_element *, struct ber_oid *, u_int);
struct ber_element *
		 mps_getnextreq(struct ber_element *, struct ber_oid *);
int		 mps_setreq(struct ber_element *, struct ber_oid *);
int		 mps_set(struct ber_oid *, void *, long long);
int		 mps_getstr(struct oid *, struct ber_oid *,
		    struct ber_element **);
int		 mps_setstr(struct oid *, struct ber_oid *,
		    struct ber_element **);
int		 mps_getint(struct oid *, struct ber_oid *,
		    struct ber_element **);
int		 mps_setint(struct oid *, struct ber_oid *,
		    struct ber_element **);
int		 mps_getts(struct oid *, struct ber_oid *,
		    struct ber_element **);
void		 mps_encodeinaddr(struct ber_oid *, struct in_addr *, int);
void		 mps_decodeinaddr(struct ber_oid *, struct in_addr *, int);
struct ber_oid	*mps_table(struct oid *, struct ber_oid *, struct ber_oid *);

/* smi.c */
int		 smi_init(void);
u_long		 smi_getticks(void);
void		 smi_mibtree(struct oid *);
struct oid	*smi_find(struct oid *);
struct oid	*smi_next(struct oid *);
struct oid	*smi_foreach(struct oid *, u_int);
void		 smi_oidlen(struct ber_oid *);
char		*smi_oidstring(struct ber_oid *, char *, size_t);
void		 smi_delete(struct oid *);
void		 smi_insert(struct oid *);
int		 smi_oid_cmp(struct oid *, struct oid *);

/* timer.c */
void		 timer_init(void);

/* snmpd.c */
int		 snmpd_socket_af(struct sockaddr_storage *, in_port_t);

#endif /* _SNMPD_H */
