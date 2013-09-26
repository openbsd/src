/*	$OpenBSD: snmpd.h,v 1.44 2013/09/26 09:11:30 reyk Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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
#include <net/pfvar.h>
#include <net/route.h>

#include "ber.h"
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
#define SNMPD_MAXENGINEIDLEN	32
#define SNMPD_MAXUSERNAMELEN	32
#define SNMPD_MAXCONTEXNAMELEN	32

#define SNMP_USM_DIGESTLEN	12
#define SNMP_USM_SALTLEN	8
#define SNMP_USM_KEYLEN		64
#define SNMP_CIPHER_KEYLEN	16

#define SMALL_READ_BUF_SIZE	1024
#define READ_BUF_SIZE		65535
#define	RT_BUF_SIZE		16384
#define	MAX_RTSOCK_BUF		(128 * 1024)

#define SNMP_ENGINEID_OLD	0x00
#define SNMP_ENGINEID_NEW	0x80	/* RFC3411 */

#define SNMP_ENGINEID_FMT_IPv4	1
#define SNMP_ENGINEID_FMT_IPv6	2
#define SNMP_ENGINEID_FMT_MAC	3
#define SNMP_ENGINEID_FMT_TEXT	4
#define SNMP_ENGINEID_FMT_OCT	5
#define SNMP_ENGINEID_FMT_EID	128

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
	struct event	 cs_evt;
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
 * pf
 */

enum {	PFRB_TABLES = 1, PFRB_TSTATS, PFRB_ADDRS, PFRB_ASTATS,
	PFRB_IFACES, PFRB_TRANS, PFRB_MAX };

enum {  IN, OUT };
enum {  IPV4, IPV6 };
enum {  PASS, BLOCK };

enum {  PFI_IFTYPE_GROUP, PFI_IFTYPE_INSTANCE };

struct pfr_buffer {
	int	 pfrb_type;	/* type of content, see enum above */
	int	 pfrb_size;	/* number of objects in buffer */
	int	 pfrb_msize;	/* maximum number of objects in buffer */
	void	*pfrb_caddr;	/* malloc'ated memory area */
};

#define PFRB_FOREACH(var, buf)				\
	for ((var) = pfr_buf_next((buf), NULL);		\
	    (var) != NULL;				\
	    (var) = pfr_buf_next((buf), (var)))

/*
 * daemon structures
 */

#define MSG_HAS_AUTH(m)		(((m)->sm_flags & SNMP_MSGFLAG_AUTH) != 0)
#define MSG_HAS_PRIV(m)		(((m)->sm_flags & SNMP_MSGFLAG_PRIV) != 0)
#define MSG_SECLEVEL(m)		((m)->sm_flags & SNMP_MSGFLAG_SECMASK)
#define MSG_REPORT(m)		(((m)->sm_flags & SNMP_MSGFLAG_REPORT) != 0)

struct snmp_message {
	struct ber_element	*sm_resp;
	u_int8_t		 sm_data[READ_BUF_SIZE];
	size_t			 sm_datalen;

	u_int			 sm_version;

	/* V1, V2c */
	char			 sm_community[SNMPD_MAXCOMMUNITYLEN];
	int			 sm_context;

	/* V3 */
	long long		 sm_msgid;
	long long		 sm_max_msg_size;
	u_int8_t		 sm_flags;
	long long		 sm_secmodel;
	u_int32_t		 sm_engine_boots;
	u_int32_t		 sm_engine_time;
	char			 sm_ctxengineid[SNMPD_MAXENGINEIDLEN];
	size_t			 sm_ctxengineid_len;
	char			 sm_ctxname[SNMPD_MAXCONTEXNAMELEN+1];

	/* USM */
	char			 sm_username[SNMPD_MAXUSERNAMELEN+1];
	struct usmuser		*sm_user;
	size_t			 sm_digest_offs;
	char			 sm_salt[SNMP_USM_SALTLEN];
	int			 sm_usmerr;

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

	/* USM stats (RFC 3414) */
	u_int32_t		snmp_usmbadseclevel;
	u_int32_t		snmp_usmtimewindow;
	u_int32_t		snmp_usmnosuchuser;
	u_int32_t		snmp_usmnosuchengine;
	u_int32_t		snmp_usmwrongdigest;
	u_int32_t		snmp_usmdecrypterr;
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

enum usmauth {
	AUTH_NONE = 0,
	AUTH_MD5,	/* HMAC-MD5-96, RFC3414 */
	AUTH_SHA1	/* HMAC-SHA-96, RFC3414 */
};

#define AUTH_DEFAULT	AUTH_SHA1	/* Default digest */

enum usmpriv {
	PRIV_NONE = 0,
	PRIV_DES,	/* CBC-DES, RFC3414 */
	PRIV_AES	/* CFB128-AES-128, RFC3826 */
};

#define PRIV_DEFAULT	PRIV_DES	/* Default cipher */

struct usmuser {
	char			*uu_name;
	int			 uu_seclevel;

	enum usmauth		 uu_auth;
	char			*uu_authkey;
	unsigned		 uu_authkeylen;


	enum usmpriv		 uu_priv;
	char			*uu_privkey;
	unsigned long long	 uu_salt;

	SLIST_ENTRY(usmuser)	 uu_next;
};

struct snmpd {
	u_int8_t		 sc_flags;
#define SNMPD_F_VERBOSE		 0x01
#define SNMPD_F_NONAMES		 0x02

	const char		*sc_confpath;
	struct address		 sc_address;
	int			 sc_sock;
	struct event		 sc_ev;
	struct timeval		 sc_starttime;
	u_int32_t		 sc_engine_boots;

	struct control_sock	 sc_csock;
	struct control_sock	 sc_rcsock;

	char			 sc_rdcommunity[SNMPD_MAXCOMMUNITYLEN];
	char			 sc_rwcommunity[SNMPD_MAXCOMMUNITYLEN];
	char			 sc_trcommunity[SNMPD_MAXCOMMUNITYLEN];

	char			 sc_engineid[SNMPD_MAXENGINEIDLEN];
	size_t			 sc_engineid_len;

	struct snmp_stats	 sc_stats;

	struct addresslist	 sc_trapreceivers;

	int			 sc_ncpu;
	int64_t			*sc_cpustates;
	int			 sc_rtfilter;

	int			 sc_min_seclevel;
	int			 sc_readonly;
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
void		 vlog(int, const char *, va_list);
const char	*log_in6addr(const struct in6_addr *);
const char	*print_host(struct sockaddr_storage *, char *, size_t);

void		 imsg_event_add(struct imsgev *);
int		 imsg_compose_event(struct imsgev *, enum imsg_type, u_int32_t,
		    pid_t, int, void *, u_int16_t);

/* kroute.c */
void		 kr_init(void);
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
int		 mps_decodeinaddr(struct ber_oid *, struct in_addr *, int);
struct ber_oid	*mps_table(struct oid *, struct ber_oid *, struct ber_oid *);

/* pf.c */
void			 pf_init(void);
int			 pf_get_stats(struct pf_status *);
int			 pfr_get_astats(struct pfr_table *, struct pfr_astats *,
			    int *, int);
int			 pfr_get_tstats(struct pfr_table *, struct pfr_tstats *,
			    int *, int);
int			 pfr_buf_grow(struct pfr_buffer *, int);
const void		*pfr_buf_next(struct pfr_buffer *, const void *);
int			 pfi_get_ifaces(const char *, struct pfi_kif *, int *);
int			 pfi_get(struct pfr_buffer *, const char *);
int			 pfi_count(void);
int			 pfi_get_if(struct pfi_kif *, int);
int			 pft_get(struct pfr_buffer *, struct pfr_table *);
int			 pft_count(void);
int			 pft_get_table(struct pfr_tstats *, int);
int			 pfta_get(struct pfr_buffer *, struct pfr_table *);
int			 pfta_get_addr(struct pfr_astats *, int);
int			 pfta_get_nextaddr(struct pfr_astats *, int *);
int			 pfta_get_first(struct pfr_astats *);

/* smi.c */
int		 smi_init(void);
u_long		 smi_getticks(void);
void		 smi_mibtree(struct oid *);
struct oid	*smi_find(struct oid *);
struct oid	*smi_next(struct oid *);
struct oid	*smi_foreach(struct oid *, u_int);
void		 smi_oidlen(struct ber_oid *);
void		 smi_scalar_oidlen(struct ber_oid *);
char		*smi_oidstring(struct ber_oid *, char *, size_t);
void		 smi_delete(struct oid *);
void		 smi_insert(struct oid *);
int		 smi_oid_cmp(struct oid *, struct oid *);
unsigned long	 smi_application(struct ber_element *);
void		 smi_debug_elements(struct ber_element *);
char		*smi_print_element(struct ber_element *);

/* timer.c */
void		 timer_init(void);

/* snmpd.c */
int		 snmpd_socket_af(struct sockaddr_storage *, in_port_t);
u_long		 snmpd_engine_time(void);
char		*tohexstr(u_int8_t *, int);

/* usm.c */
void		 usm_generate_keys(void);
struct usmuser	*usm_newuser(char *name, const char **);
struct usmuser	*usm_finduser(char *name);
int		 usm_checkuser(struct usmuser *, const char **);
struct ber_element *usm_decode(struct snmp_message *, struct ber_element *,
		    const char **);
struct ber_element *usm_encode(struct snmp_message *, struct ber_element *);
struct ber_element *usm_encrypt(struct snmp_message *, struct ber_element *);
void		 usm_finalize_digest(struct snmp_message *, char *, ssize_t);
void		 usm_make_report(struct snmp_message *);
#endif /* _SNMPD_H */
