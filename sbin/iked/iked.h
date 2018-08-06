/*	$OpenBSD: iked.h,v 1.119 2018/08/06 06:30:06 mestre Exp $	*/

/*
 * Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/tree.h>
#include <sys/queue.h>
#include <arpa/inet.h>
#include <limits.h>
#include <imsg.h>

#include <openssl/evp.h>

#include "types.h"
#include "dh.h"

#ifndef IKED_H
#define IKED_H

/*
 * Common IKEv1/IKEv2 header
 */

struct ike_header {
	uint64_t	 ike_ispi;		/* Initiator cookie */
	uint64_t	 ike_rspi;		/* Responder cookie */
	uint8_t		 ike_nextpayload;	/* Next payload type */
	uint8_t		 ike_version;		/* Major/Minor version number */
	uint8_t		 ike_exchange;		/* Exchange type */
	uint8_t		 ike_flags;		/* Message options */
	uint32_t	 ike_msgid;		/* Message identifier */
	uint32_t	 ike_length;		/* Total message length */
} __packed;

/*
 * Common daemon infrastructure, local imsg etc.
 */

struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	struct privsep_proc	*proc;
	void			*data;
	short			 events;
	const char		*name;
};

#define IMSG_SIZE_CHECK(imsg, p) do {				\
	if (IMSG_DATA_SIZE(imsg) < sizeof(*p))			\
		fatalx("bad length imsg received");		\
} while (0)
#define IMSG_DATA_SIZE(imsg)	((imsg)->hdr.len - IMSG_HEADER_SIZE)

#define IKED_ADDR_EQ(_a, _b)						\
	((_a)->addr_mask == (_b)->addr_mask &&				\
	sockaddr_cmp((struct sockaddr *)&(_a)->addr,			\
	(struct sockaddr *)&(_b)->addr, (_a)->addr_mask) == 0)

#define IKED_ADDR_NEQ(_a, _b)						\
	((_a)->addr_mask != (_b)->addr_mask ||				\
	sockaddr_cmp((struct sockaddr *)&(_a)->addr,			\
	(struct sockaddr *)&(_b)->addr, (_a)->addr_mask) != 0)

/* initially control.h */
struct control_sock {
	const char	*cs_name;
	struct event	 cs_ev;
	struct event	 cs_evt;
	int		 cs_fd;
	int		 cs_restricted;
	void		*cs_env;

	TAILQ_ENTRY(control_sock) cs_entry;
};
TAILQ_HEAD(control_socks, control_sock);

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	uint8_t			 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgev		 iev;
};
TAILQ_HEAD(ctl_connlist, ctl_conn);
extern  struct ctl_connlist ctl_conns;

enum privsep_procid privsep_process;

/*
 * Runtime structures
 */

struct iked_timer {
	struct event	 tmr_ev;
	struct iked	*tmr_env;
	void		(*tmr_cb)(struct iked *, void *);
	void		*tmr_cbarg;
};

struct iked_spi {
	uint64_t	 spi;
	uint8_t		 spi_size;
	uint8_t		 spi_protoid;
};

struct iked_proposal {
	uint8_t				 prop_id;
	uint8_t				 prop_protoid;

	struct iked_spi			 prop_localspi;
	struct iked_spi			 prop_peerspi;

	struct iked_transform		*prop_xforms;
	unsigned int			 prop_nxforms;

	TAILQ_ENTRY(iked_proposal)	 prop_entry;
};
TAILQ_HEAD(iked_proposals, iked_proposal);

struct iked_addr {
	int				 addr_af;
	struct sockaddr_storage		 addr;
	uint8_t				 addr_mask;
	int				 addr_net;
	in_port_t			 addr_port;
};

struct iked_flow {
	struct iked_addr		 flow_src;
	struct iked_addr		 flow_dst;
	unsigned int			 flow_dir;	/* in/out */
	struct iked_addr		 flow_prenat;

	unsigned int			 flow_loaded;	/* pfkey done */

	uint8_t				 flow_saproto;
	uint8_t				 flow_ipproto;
	uint8_t				 flow_type;

	struct iked_addr		*flow_local;	/* outer source */
	struct iked_addr		*flow_peer;	/* outer dest */
	struct iked_sa			*flow_ikesa;	/* parent SA */

	RB_ENTRY(iked_flow)		 flow_node;
	TAILQ_ENTRY(iked_flow)		 flow_entry;

	int				 flow_replacing; /* cf flow_replace() */
	int				 flow_ipcomp;
};
RB_HEAD(iked_flows, iked_flow);
TAILQ_HEAD(iked_saflows, iked_flow);

struct iked_childsa {
	uint8_t				 csa_saproto;	/* IPsec protocol */
	unsigned int			 csa_dir;	/* in/out */

	uint64_t			 csa_peerspi;	/* peer relation */
	uint8_t				 csa_loaded;	/* pfkey done */
	uint8_t				 csa_rekey;	/* will be deleted */
	uint8_t				 csa_allocated;	/* from the kernel */
	uint8_t				 csa_persistent;/* do not rekey */
	uint8_t				 csa_esn;	/* use ESN */
	uint8_t				 csa_transport;	/* transport mode */
	uint8_t				 csa_acquired;	/* no rekey for me */

	struct iked_spi			 csa_spi;

	struct ibuf			*csa_encrkey;	/* encryption key */
	uint16_t			 csa_encrid;	/* encryption xform id */

	struct ibuf			*csa_integrkey;	/* auth key */
	uint16_t			 csa_integrid;	/* auth xform id */

	struct iked_addr		*csa_local;	/* outer source */
	struct iked_addr		*csa_peer;	/* outer dest */
	struct iked_sa			*csa_ikesa;	/* parent SA */

	struct iked_childsa		*csa_peersa;	/* peer */

	struct iked_childsa		*csa_parent;	/* IPCOMP parent */
	unsigned int			 csa_children;	/* IPCOMP children */

	RB_ENTRY(iked_childsa)		 csa_node;
	TAILQ_ENTRY(iked_childsa)	 csa_entry;
};
RB_HEAD(iked_activesas, iked_childsa);
TAILQ_HEAD(iked_childsas, iked_childsa);


struct iked_static_id {
	uint8_t		id_type;
	uint8_t		id_length;
	uint8_t		id_offset;
	uint8_t		id_data[IKED_ID_SIZE];
};

struct iked_auth {
	uint8_t		auth_method;
	uint8_t		auth_eap;			/* optional EAP */
	uint8_t		auth_length;			/* zero if EAP */
	uint8_t		auth_data[IKED_PSK_SIZE];
};

struct iked_cfg {
	uint8_t				 cfg_action;
	uint16_t			 cfg_type;
	union {
		struct iked_addr	 address;
	} cfg;
};

TAILQ_HEAD(iked_sapeers, iked_sa);

struct iked_lifetime {
	uint64_t			 lt_bytes;
	uint64_t			 lt_seconds;
};

struct iked_policy {
	unsigned int			 pol_id;
	char				 pol_name[IKED_ID_SIZE];

#define IKED_SKIP_FLAGS			 0
#define IKED_SKIP_AF			 1
#define IKED_SKIP_PROTO			 2
#define IKED_SKIP_SRC_ADDR		 3
#define IKED_SKIP_DST_ADDR		 4
#define IKED_SKIP_COUNT			 5
	struct iked_policy		*pol_skip[IKED_SKIP_COUNT];

	uint8_t				 pol_flags;
#define IKED_POLICY_PASSIVE		 0x00
#define IKED_POLICY_DEFAULT		 0x01
#define IKED_POLICY_ACTIVE		 0x02
#define IKED_POLICY_REFCNT		 0x04
#define IKED_POLICY_QUICK		 0x08
#define IKED_POLICY_SKIP		 0x10
#define IKED_POLICY_IPCOMP		 0x20

	int				 pol_refcnt;

	uint8_t				 pol_certreqtype;

	int				 pol_af;
	uint8_t				 pol_saproto;
	unsigned int			 pol_ipproto;

	struct iked_addr		 pol_peer;
	struct iked_static_id		 pol_peerid;
	uint32_t			 pol_peerdh;

	struct iked_addr		 pol_local;
	struct iked_static_id		 pol_localid;

	struct iked_auth		 pol_auth;

	char				 pol_tag[IKED_TAG_SIZE];
	unsigned int			 pol_tap;

	struct iked_proposals		 pol_proposals;
	size_t				 pol_nproposals;

	struct iked_flows		 pol_flows;
	size_t				 pol_nflows;

	struct iked_cfg			 pol_cfg[IKED_CFG_MAX];
	unsigned int			 pol_ncfg;

	uint32_t			 pol_rekey;	/* ike SA lifetime */
	struct iked_lifetime		 pol_lifetime;	/* child SA lifetime */

	struct iked_sapeers		 pol_sapeers;

	TAILQ_ENTRY(iked_policy)	 pol_entry;
};
TAILQ_HEAD(iked_policies, iked_policy);

struct iked_hash {
	uint8_t		 hash_type;	/* PRF or INTEGR */
	uint16_t	 hash_id;	/* IKE PRF/INTEGR hash id */
	const void	*hash_priv;	/* Identifying the hash alg */
	void		*hash_ctx;	/* Context of the current invocation */
	int		 hash_fixedkey;	/* Requires fixed key length */
	struct ibuf	*hash_key;	/* MAC key derived from key seed */
	size_t		 hash_length;	/* Output length */
	size_t		 hash_trunc;	/* Truncate the output length */
	struct iked_hash *hash_prf;	/* PRF pointer */
};

struct iked_cipher {
	uint8_t		 encr_type;	/* ENCR */
	uint16_t	 encr_id;	/* IKE ENCR hash id */
	const void	*encr_priv;	/* Identifying the hash alg */
	void		*encr_ctx;	/* Context of the current invocation */
	int		 encr_fixedkey;	/* Requires fixed key length */
	struct ibuf	*encr_key;	/* MAC key derived from key seed */
	struct ibuf	*encr_iv;	/* Initialization Vector */
	size_t		 encr_ivlength;	/* IV length */
	size_t		 encr_length;	/* Block length */
};

struct iked_dsa {
	uint8_t		 dsa_method;	/* AUTH method */
	const void	*dsa_priv;	/* PRF or signature hash function */
	void		*dsa_ctx;	/* PRF or signature hash ctx */
	struct ibuf	*dsa_keydata;	/* public, private or shared key */
	void		*dsa_key;	/* parsed public or private key */
	void		*dsa_cert;	/* parsed certificate */
	int		 dsa_hmac;	/* HMAC or public/private key */
	int		 dsa_sign;	/* Sign or verify operation */
};

struct iked_id {
	uint8_t		 id_type;
	uint8_t		 id_offset;
	struct ibuf	*id_buf;
};

#define IKED_REQ_CERT		0x0001	/* get local certificate (if required) */
#define IKED_REQ_CERTVALID	0x0002	/* validated the peer cert */
#define IKED_REQ_CERTREQ	0x0004	/* CERTREQ has been received */
#define IKED_REQ_AUTH		0x0008	/* AUTH payload */
#define IKED_REQ_AUTHVALID	0x0010	/* AUTH payload has been verified */
#define IKED_REQ_SA		0x0020	/* SA available */
#define IKED_REQ_EAPVALID	0x0040	/* EAP payload has been verified */
#define IKED_REQ_CHILDSA	0x0080	/* Child SA initiated */
#define IKED_REQ_INF		0x0100	/* Informational exchange initiated */

#define IKED_REQ_BITS	\
    "\20\01CERT\02CERTVALID\03CERTREQ\04AUTH\05AUTHVALID\06SA\07EAPVALID" \
    "\10CHILDSA\11INF"

TAILQ_HEAD(iked_msgqueue, iked_message);

struct iked_sahdr {
	uint64_t			 sh_ispi;	/* Initiator SPI */
	uint64_t			 sh_rspi;	/* Responder SPI */
	unsigned int			 sh_initiator;	/* Is initiator? */
} __packed;

struct iked_kex {
	struct ibuf			*kex_inonce;	/* Ni */
	struct ibuf			*kex_rnonce;	/* Nr */

	struct group			*kex_dhgroup;	/* DH group */
	struct ibuf			*kex_dhiexchange;
	struct ibuf			*kex_dhrexchange;
	struct ibuf			*kex_dhpeer;	/* pointer to i or r */
};

struct iked_sa {
	struct iked_sahdr		 sa_hdr;
	uint32_t			 sa_msgid;	/* Last request rcvd */
	int				 sa_msgid_set;	/* msgid initialized */
	uint32_t			 sa_reqid;	/* Next request sent */

	int				 sa_type;
#define IKED_SATYPE_LOOKUP		 0		/* Used for lookup */
#define IKED_SATYPE_LOCAL		 1		/* Local SA */

	struct iked_addr		 sa_peer;
	struct iked_addr		 sa_peer_loaded;/* MOBIKE */
	struct iked_addr		 sa_local;
	int				 sa_fd;

	int				 sa_natt;	/* for IKE messages */
	int				 sa_udpencap;	/* for pfkey */
	int				 sa_usekeepalive;/* NAT-T keepalive */

	int				 sa_state;
	unsigned int			 sa_stateflags;
	unsigned int			 sa_stateinit;	/* SA_INIT */
	unsigned int			 sa_statevalid;	/* IKE_AUTH */

	int				 sa_cp;		/* XXX */

	struct iked_policy		*sa_policy;
	struct timeval			 sa_timecreated;
	struct timeval			 sa_timeused;

	char				*sa_tag;

	struct iked_kex			 sa_kex;
/* XXX compat defines until everything is converted */
#define sa_inonce		sa_kex.kex_inonce
#define sa_rnonce		sa_kex.kex_rnonce
#define sa_dhgroup		sa_kex.kex_dhgroup
#define sa_dhiexchange		sa_kex.kex_dhiexchange
#define sa_dhrexchange		sa_kex.kex_dhrexchange
#define sa_dhpeer		sa_kex.kex_dhpeer

	struct iked_hash		*sa_prf;	/* PRF alg */
	struct iked_hash		*sa_integr;	/* integrity alg */
	struct iked_cipher		*sa_encr;	/* encryption alg */

	struct ibuf			*sa_key_d;	/* SK_d */
	struct ibuf			*sa_key_iauth;	/* SK_ai */
	struct ibuf			*sa_key_rauth;	/* SK_ar */
	struct ibuf			*sa_key_iencr;	/* SK_ei */
	struct ibuf			*sa_key_rencr;	/* SK_er */
	struct ibuf			*sa_key_iprf;	/* SK_pi */
	struct ibuf			*sa_key_rprf;	/* SK_pr */

	struct ibuf			*sa_1stmsg;	/* for initiator AUTH */
	struct ibuf			*sa_2ndmsg;	/* for responder AUTH */
	struct iked_id			 sa_localauth;	/* local AUTH message */
	int				 sa_sigsha2;	/* use SHA2 for signatures */

	struct iked_id			 sa_iid;	/* initiator id */
	struct iked_id			 sa_rid;	/* responder id */
	struct iked_id			 sa_icert;	/* initiator cert */
	struct iked_id			 sa_rcert;	/* responder cert */
#define IKESA_SRCID(x) ((x)->sa_hdr.sh_initiator ? &(x)->sa_iid : &(x)->sa_rid)
#define IKESA_DSTID(x) ((x)->sa_hdr.sh_initiator ? &(x)->sa_rid : &(x)->sa_iid)

	char				*sa_eapid;	/* EAP identity */
	struct iked_id			 sa_eap;	/* EAP challenge */
	struct ibuf			*sa_eapmsk;	/* EAK session key */

	struct iked_proposals		 sa_proposals;	/* SA proposals */
	struct iked_childsas		 sa_childsas;	/* IPsec Child SAs */
	struct iked_saflows		 sa_flows;	/* IPsec flows */

	struct iked_sa			*sa_nexti;	/* initiated IKE SA */
	struct iked_sa			*sa_nextr;	/* simultaneous rekey */
	uint64_t			 sa_rekeyspi;	/* peerspi CSA rekey*/
	struct ibuf			*sa_simult;	/* simultaneous rekey */

	uint8_t				 sa_ipcomp;	/* IPcomp transform */
	uint16_t			 sa_cpi_out;	/* IPcomp outgoing */
	uint16_t			 sa_cpi_in;	/* IPcomp incoming*/

	int				 sa_mobike;	/* MOBIKE */

	struct iked_timer		 sa_timer;	/* SA timeouts */
#define IKED_IKE_SA_EXCHANGE_TIMEOUT	 300		/* 5 minutes */
#define IKED_IKE_SA_REKEY_TIMEOUT	 120		/* 2 minutes */
#define IKED_IKE_SA_DELETE_TIMEOUT	 120		/* 2 minutes */
#define IKED_IKE_SA_ALIVE_TIMEOUT	 60		/* 1 minute */

	struct iked_timer		 sa_keepalive;	/* keepalive timer */
#define IKED_IKE_SA_KEEPALIVE_TIMEOUT	 20

	struct iked_timer		 sa_rekey;	/* rekey timeout */

	struct iked_msgqueue		 sa_requests;	/* request queue */
#define IKED_RETRANSMIT_TIMEOUT		 2		/* 2 seconds */

	struct iked_msgqueue		 sa_responses;	/* response queue */
#define IKED_RESPONSE_TIMEOUT		 120		/* 2 minutes */

	TAILQ_ENTRY(iked_sa)		 sa_peer_entry;
	RB_ENTRY(iked_sa)		 sa_entry;

	struct iked_addr		*sa_addrpool;	/* address from pool */
	RB_ENTRY(iked_sa)		 sa_addrpool_entry;	/* pool entries */

	struct iked_addr		*sa_addrpool6;	/* address from pool */
	RB_ENTRY(iked_sa)		 sa_addrpool6_entry;	/* pool entries */
};
RB_HEAD(iked_sas, iked_sa);
RB_HEAD(iked_addrpool, iked_sa);
RB_HEAD(iked_addrpool6, iked_sa);

struct iked_message {
	struct ibuf		*msg_data;
	size_t			 msg_offset;

	struct sockaddr_storage	 msg_local;
	socklen_t		 msg_locallen;

	struct sockaddr_storage	 msg_peer;
	socklen_t		 msg_peerlen;

	struct iked_socket	*msg_sock;

	int			 msg_fd;
	int			 msg_response;
	int			 msg_responded;
	int			 msg_natt;
	int			 msg_natt_rcvd;
	int			 msg_error;
	int			 msg_e;
	struct iked_message	*msg_parent;

	/* Associated policy and SA */
	struct iked_policy	*msg_policy;
	struct iked_sa		*msg_sa;

	uint32_t		 msg_msgid;
	uint8_t			 msg_exchange;

	/* Parsed information */
	struct iked_proposals	 msg_proposals;
	struct iked_spi		 msg_rekey;
	struct ibuf		*msg_nonce;	/* dh NONCE */
	uint16_t		 msg_dhgroup;	/* dh group */
	struct ibuf		*msg_ke;	/* dh key exchange */
	struct iked_id		 msg_auth;	/* AUTH payload */
	struct iked_id		 msg_id;
	struct iked_id		 msg_cert;
	struct ibuf		*msg_cookie;

	/* MOBIKE */
	int			 msg_update_sa_addresses;
	struct ibuf		*msg_cookie2;

	/* Parse stack */
	struct iked_proposal	*msg_prop;
	uint16_t		 msg_attrlength;

	/* Retransmit queue */
	struct iked_timer	 msg_timer;
	TAILQ_ENTRY(iked_message)
				 msg_entry;
	int			 msg_tries;	/* retransmits sent */
#define IKED_RETRANSMIT_TRIES	 5		/* try 5 times */
};

struct iked_user {
	char			 usr_name[LOGIN_NAME_MAX];
	char			 usr_pass[IKED_PASSWORD_SIZE];
	RB_ENTRY(iked_user)	 usr_entry;
};
RB_HEAD(iked_users, iked_user);

struct privsep_pipes {
	int				*pp_pipes[PROC_MAX];
};

struct privsep {
	struct privsep_pipes		*ps_pipes[PROC_MAX];
	struct privsep_pipes		*ps_pp;

	struct imsgev			*ps_ievs[PROC_MAX];
	const char			*ps_title[PROC_MAX];
	pid_t				 ps_pid[PROC_MAX];
	struct passwd			*ps_pw;
	int				 ps_noaction;

	struct control_sock		 ps_csock;
	struct control_socks		 ps_rcsocks;

	unsigned int			 ps_instances[PROC_MAX];
	unsigned int			 ps_ninstances;
	unsigned int			 ps_instance;

	/* Event and signal handlers */
	struct event			 ps_evsigint;
	struct event			 ps_evsigterm;
	struct event			 ps_evsigchld;
	struct event			 ps_evsighup;
	struct event			 ps_evsigpipe;
	struct event			 ps_evsigusr1;

	struct iked			*ps_env;
};

struct privsep_proc {
	const char		*p_title;
	enum privsep_procid	 p_id;
	int			(*p_cb)(int, struct privsep_proc *,
				    struct imsg *);
	pid_t			(*p_init)(struct privsep *,
				    struct privsep_proc *);
	const char		*p_chroot;
	struct privsep		*p_ps;
	struct iked		*p_env;
	void			(*p_shutdown)(void);
	unsigned int		 p_instance;
};

struct iked_ocsp_entry {
	TAILQ_ENTRY(iked_ocsp_entry) ioe_entry;	/* next request */
	void			*ioe_ocsp;	/* private ocsp request data */
};
TAILQ_HEAD(iked_ocsp_requests, iked_ocsp_entry);

/*
 * Daemon configuration
 */

struct iked {
	char				 sc_conffile[PATH_MAX];

	uint32_t			 sc_opts;
	uint8_t				 sc_passive;
	uint8_t				 sc_decoupled;

	uint8_t				 sc_mobike;	/* MOBIKE */

	struct iked_policies		 sc_policies;
	struct iked_policy		*sc_defaultcon;

	struct iked_sas			 sc_sas;
	struct iked_activesas		 sc_activesas;
	struct iked_flows		 sc_activeflows;
	struct iked_users		 sc_users;

	void				*sc_priv;	/* per-process */

	int				 sc_pfkey;	/* ike process */
	struct event			 sc_pfkeyev;
	uint8_t				 sc_certreqtype;
	struct ibuf			*sc_certreq;

	struct iked_socket		*sc_sock4[2];
	struct iked_socket		*sc_sock6[2];

	struct iked_timer		 sc_inittmr;
#define IKED_INITIATOR_INITIAL		 2
#define IKED_INITIATOR_INTERVAL		 60

	struct privsep			 sc_ps;

	struct iked_ocsp_requests	 sc_ocsp;
	char				*sc_ocsp_url;

	struct iked_addrpool		 sc_addrpool;
	struct iked_addrpool6		 sc_addrpool6;
};

struct iked_socket {
	int			 sock_fd;
	struct event		 sock_ev;
	struct iked		*sock_env;
	struct sockaddr_storage	 sock_addr;
};

/* iked.c */
void	 parent_reload(struct iked *, int, const char *);

/* control.c */
pid_t	 control(struct privsep *, struct privsep_proc *);
int	 control_init(struct privsep *, struct control_sock *);
int	 control_listen(struct control_sock *);

/* config.c */
struct iked_policy *
	 config_new_policy(struct iked *);
void	 config_free_kex(struct iked_kex *);
void	 config_free_sa(struct iked *, struct iked_sa *);
struct iked_sa *
	 config_new_sa(struct iked *, int);
struct iked_user *
	 config_new_user(struct iked *, struct iked_user *);
uint64_t
	 config_getspi(void);
struct iked_transform *
	 config_findtransform(struct iked_proposals *, uint8_t, unsigned int);
void	 config_free_policy(struct iked *, struct iked_policy *);
struct iked_proposal *
	 config_add_proposal(struct iked_proposals *, unsigned int,
	    unsigned int);
void	 config_free_proposals(struct iked_proposals *, unsigned int);
void	 config_free_flows(struct iked *, struct iked_flows *);
void	 config_free_childsas(struct iked *, struct iked_childsas *,
	    struct iked_spi *, struct iked_spi *);
struct iked_transform *
	 config_add_transform(struct iked_proposal *,
	    unsigned int, unsigned int, unsigned int, unsigned int);
int	 config_setcoupled(struct iked *, unsigned int);
int	 config_getcoupled(struct iked *, unsigned int);
int	 config_setmode(struct iked *, unsigned int);
int	 config_getmode(struct iked *, unsigned int);
int	 config_setreset(struct iked *, unsigned int, enum privsep_procid);
int	 config_getreset(struct iked *, struct imsg *);
int	 config_setpolicy(struct iked *, struct iked_policy *,
	    enum privsep_procid);
int	 config_getpolicy(struct iked *, struct imsg *);
int	 config_setflow(struct iked *, struct iked_policy *,
	    enum privsep_procid);
int	 config_getflow(struct iked *, struct imsg *);
int	 config_setsocket(struct iked *, struct sockaddr_storage *, in_port_t,
	    enum privsep_procid);
int	 config_getsocket(struct iked *env, struct imsg *,
	    void (*cb)(int, short, void *));
int	 config_setpfkey(struct iked *, enum privsep_procid);
int	 config_getpfkey(struct iked *, struct imsg *);
int	 config_setuser(struct iked *, struct iked_user *, enum privsep_procid);
int	 config_getuser(struct iked *, struct imsg *);
int	 config_setcompile(struct iked *, enum privsep_procid);
int	 config_getcompile(struct iked *, struct imsg *);
int	 config_setocsp(struct iked *);
int	 config_getocsp(struct iked *, struct imsg *);
int	 config_setkeys(struct iked *);
int	 config_getkey(struct iked *, struct imsg *);
int	 config_setmobike(struct iked *);
int	 config_getmobike(struct iked *, struct imsg *);

/* policy.c */
void	 policy_init(struct iked *);
int	 policy_lookup(struct iked *, struct iked_message *);
struct iked_policy *
	 policy_test(struct iked *, struct iked_policy *);
void	 policy_calc_skip_steps(struct iked_policies *);
void	 policy_ref(struct iked *, struct iked_policy *);
void	 policy_unref(struct iked *, struct iked_policy *);
void	 sa_state(struct iked *, struct iked_sa *, int);
void	 sa_stateflags(struct iked_sa *, unsigned int);
int	 sa_stateok(struct iked_sa *, int);
struct iked_sa *
	 sa_new(struct iked *, uint64_t, uint64_t, unsigned int,
	    struct iked_policy *);
void	 sa_free(struct iked *, struct iked_sa *);
void	 sa_free_flows(struct iked *, struct iked_saflows *);
int	 sa_address(struct iked_sa *, struct iked_addr *,
	    struct sockaddr_storage *);
void	 childsa_free(struct iked_childsa *);
struct iked_childsa *
	 childsa_lookup(struct iked_sa *, uint64_t, uint8_t);
void	 flow_free(struct iked_flow *);
int	 flow_equal(struct iked_flow *, struct iked_flow *);
int	 flow_replace(struct iked *, struct iked_flow *);
struct iked_sa *
	 sa_lookup(struct iked *, uint64_t, uint64_t, unsigned int);
struct iked_user *
	 user_lookup(struct iked *, const char *);
RB_PROTOTYPE(iked_sas, iked_sa, sa_entry, sa_cmp);
RB_PROTOTYPE(iked_addrpool, iked_sa, sa_addrpool_entry, sa_addrpool_cmp);
RB_PROTOTYPE(iked_addrpool6, iked_sa, sa_addrpool6_entry, sa_addrpool6_cmp);
RB_PROTOTYPE(iked_users, iked_user, user_entry, user_cmp);
RB_PROTOTYPE(iked_activesas, iked_childsa, csa_node, childsa_cmp);
RB_PROTOTYPE(iked_flows, iked_flow, flow_node, flow_cmp);

/* crypto.c */
struct iked_hash *
	 hash_new(uint8_t, uint16_t);
struct ibuf *
	 hash_setkey(struct iked_hash *, void *, size_t);
void	 hash_free(struct iked_hash *);
void	 hash_init(struct iked_hash *);
void	 hash_update(struct iked_hash *, void *, size_t);
void	 hash_final(struct iked_hash *, void *, size_t *);
size_t	 hash_keylength(struct iked_hash *);
size_t	 hash_length(struct iked_hash *);

struct iked_cipher *
	 cipher_new(uint8_t, uint16_t, uint16_t);
struct ibuf *
	 cipher_setkey(struct iked_cipher *, void *, size_t);
struct ibuf *
	 cipher_setiv(struct iked_cipher *, void *, size_t);
void	 cipher_free(struct iked_cipher *);
void	 cipher_init(struct iked_cipher *, int);
void	 cipher_init_encrypt(struct iked_cipher *);
void	 cipher_init_decrypt(struct iked_cipher *);
void	 cipher_update(struct iked_cipher *, void *, size_t, void *, size_t *);
void	 cipher_final(struct iked_cipher *, void *, size_t *);
size_t	 cipher_length(struct iked_cipher *);
size_t	 cipher_keylength(struct iked_cipher *);
size_t	 cipher_ivlength(struct iked_cipher *);
size_t	 cipher_outlength(struct iked_cipher *, size_t);

struct iked_dsa *
	 dsa_new(uint16_t, struct iked_hash *, int);
struct iked_dsa *
	 dsa_sign_new(uint16_t, struct iked_hash *);
struct iked_dsa *
	 dsa_verify_new(uint16_t, struct iked_hash *);
struct ibuf *
	 dsa_setkey(struct iked_dsa *, void *, size_t, uint8_t);
void	 dsa_free(struct iked_dsa *);
int	 dsa_init(struct iked_dsa *, const void *, size_t);
size_t	 dsa_prefix(struct iked_dsa *);
size_t	 dsa_length(struct iked_dsa *);
int	 dsa_update(struct iked_dsa *, const void *, size_t);
ssize_t	 dsa_sign_final(struct iked_dsa *, void *, size_t);
ssize_t	 dsa_verify_final(struct iked_dsa *, void *, size_t);

/* ikev2.c */
pid_t	 ikev2(struct privsep *, struct privsep_proc *);
void	 ikev2_recv(struct iked *, struct iked_message *);
void	 ikev2_init_ike_sa(struct iked *, void *);
int	 ikev2_sa_negotiate(struct iked_proposals *, struct iked_proposals *,
	    struct iked_proposals *, int);
int	 ikev2_policy2id(struct iked_static_id *, struct iked_id *, int);
int	 ikev2_childsa_enable(struct iked *, struct iked_sa *);
int	 ikev2_childsa_delete(struct iked *, struct iked_sa *,
	    uint8_t, uint64_t, uint64_t *, int);
void	 ikev2_ikesa_recv_delete(struct iked *, struct iked_sa *);
void	 ikev2_ike_sa_timeout(struct iked *env, void *);

struct ibuf *
	 ikev2_prfplus(struct iked_hash *, struct ibuf *, struct ibuf *,
	    size_t);
ssize_t	 ikev2_psk(struct iked_sa *, uint8_t *, size_t, uint8_t **);
ssize_t	 ikev2_nat_detection(struct iked *, struct iked_message *,
	    void *, size_t, unsigned int);
int	 ikev2_send_informational(struct iked *, struct iked_message *);
int	 ikev2_send_ike_e(struct iked *, struct iked_sa *, struct ibuf *,
	    uint8_t, uint8_t, int);
struct ike_header *
	 ikev2_add_header(struct ibuf *, struct iked_sa *,
	    uint32_t, uint8_t, uint8_t, uint8_t);
int	 ikev2_set_header(struct ike_header *, size_t);
struct ikev2_payload *
	 ikev2_add_payload(struct ibuf *);
int	 ikev2_next_payload(struct ikev2_payload *, size_t,
	    uint8_t);
int	 ikev2_acquire_sa(struct iked *, struct iked_flow *);
void	 ikev2_disable_rekeying(struct iked *, struct iked_sa *);
int	 ikev2_rekey_sa(struct iked *, struct iked_spi *);
int	 ikev2_drop_sa(struct iked *, struct iked_spi *);
int	 ikev2_print_id(struct iked_id *, char *, size_t);

/* ikev2_msg.c */
void	 ikev2_msg_cb(int, short, void *);
struct ibuf *
	 ikev2_msg_init(struct iked *, struct iked_message *,
	    struct sockaddr_storage *, socklen_t,
	    struct sockaddr_storage *, socklen_t, int);
struct iked_message *
	 ikev2_msg_copy(struct iked *, struct iked_message *);
void	 ikev2_msg_cleanup(struct iked *, struct iked_message *);
uint32_t
	 ikev2_msg_id(struct iked *, struct iked_sa *);
struct ibuf
	*ikev2_msg_auth(struct iked *, struct iked_sa *, int);
int	 ikev2_msg_authsign(struct iked *, struct iked_sa *,
	    struct iked_auth *, struct ibuf *);
int	 ikev2_msg_authverify(struct iked *, struct iked_sa *,
	    struct iked_auth *, uint8_t *, size_t, struct ibuf *);
int	 ikev2_msg_valid_ike_sa(struct iked *, struct ike_header *,
	    struct iked_message *);
int	 ikev2_msg_send(struct iked *, struct iked_message *);
int	 ikev2_msg_send_encrypt(struct iked *, struct iked_sa *,
	    struct ibuf **, uint8_t, uint8_t, int);
struct ibuf
	*ikev2_msg_encrypt(struct iked *, struct iked_sa *, struct ibuf *);
struct ibuf *
	 ikev2_msg_decrypt(struct iked *, struct iked_sa *,
	    struct ibuf *, struct ibuf *);
int	 ikev2_msg_integr(struct iked *, struct iked_sa *, struct ibuf *);
int	 ikev2_msg_frompeer(struct iked_message *);
struct iked_socket *
	 ikev2_msg_getsocket(struct iked *, int, int);
int	 ikev2_msg_retransmit_response(struct iked *, struct iked_sa *,
	    struct iked_message *);
void	 ikev2_msg_prevail(struct iked *, struct iked_msgqueue *,
	    struct iked_message *);
void	 ikev2_msg_dispose(struct iked *, struct iked_msgqueue *,
	    struct iked_message *);
void	 ikev2_msg_flushqueue(struct iked *, struct iked_msgqueue *);
struct iked_message *
	 ikev2_msg_lookup(struct iked *, struct iked_msgqueue *,
	    struct iked_message *, struct ike_header *);

/* ikev2_pld.c */
int	 ikev2_pld_parse(struct iked *, struct ike_header *,
	    struct iked_message *, size_t);

/* eap.c */
ssize_t	 eap_identity_request(struct ibuf *);
int	 eap_parse(struct iked *, struct iked_sa *, void *, int);

/* pfkey.c */
int	 pfkey_couple(int, struct iked_sas *, int);
int	 pfkey_flow_add(int fd, struct iked_flow *);
int	 pfkey_flow_delete(int fd, struct iked_flow *);
int	 pfkey_block(int, int, unsigned int);
int	 pfkey_sa_init(int, struct iked_childsa *, uint32_t *);
int	 pfkey_sa_add(int, struct iked_childsa *, struct iked_childsa *);
int	 pfkey_sa_update_addresses(int, struct iked_childsa *);
int	 pfkey_sa_delete(int, struct iked_childsa *);
int	 pfkey_sa_last_used(int, struct iked_childsa *, uint64_t *);
int	 pfkey_flush(int);
int	 pfkey_socket(void);
void	 pfkey_init(struct iked *, int fd);

/* ca.c */
pid_t	 caproc(struct privsep *, struct privsep_proc *);
int	 ca_setreq(struct iked *, struct iked_sa *, struct iked_static_id *,
	    uint8_t, uint8_t *, size_t, enum privsep_procid);
int	 ca_setcert(struct iked *, struct iked_sahdr *, struct iked_id *,
	    uint8_t, uint8_t *, size_t, enum privsep_procid);
int	 ca_setauth(struct iked *, struct iked_sa *,
	    struct ibuf *, enum privsep_procid);
void	 ca_getkey(struct privsep *, struct iked_id *, enum imsg_type);
int	 ca_privkey_serialize(EVP_PKEY *, struct iked_id *);
int	 ca_pubkey_serialize(EVP_PKEY *, struct iked_id *);
void	 ca_sslinit(void);
void	 ca_sslerror(const char *);
char	*ca_asn1_name(uint8_t *, size_t);
char	*ca_x509_name(void *);
void	*ca_x509_name_parse(char *);

/* timer.c */
void	 timer_set(struct iked *, struct iked_timer *,
	    void (*)(struct iked *, void *), void *);
void	 timer_add(struct iked *, struct iked_timer *, int);
void	 timer_del(struct iked *, struct iked_timer *);

/* proc.c */
void	 proc_init(struct privsep *, struct privsep_proc *, unsigned int);
void	 proc_kill(struct privsep *);
void	 proc_listen(struct privsep *, struct privsep_proc *, size_t);
void	 proc_dispatch(int, short event, void *);
pid_t	 proc_run(struct privsep *, struct privsep_proc *,
	    struct privsep_proc *, unsigned int,
	    void (*)(struct privsep *, struct privsep_proc *, void *), void *);
void	 imsg_event_add(struct imsgev *);
int	 imsg_compose_event(struct imsgev *, uint16_t, uint32_t,
	    pid_t, int, void *, uint16_t);
int	 imsg_composev_event(struct imsgev *, uint16_t, uint32_t,
	    pid_t, int, const struct iovec *, int);
int	 proc_compose_imsg(struct privsep *, enum privsep_procid, int,
	    u_int16_t, u_int32_t, int, void *, u_int16_t);
int	 proc_compose(struct privsep *, enum privsep_procid,
	    uint16_t, void *, uint16_t);
int	 proc_composev_imsg(struct privsep *, enum privsep_procid, int,
	    u_int16_t, u_int32_t, int, const struct iovec *, int);
int	 proc_composev(struct privsep *, enum privsep_procid,
	    uint16_t, const struct iovec *, int);
int	 proc_forward_imsg(struct privsep *, struct imsg *,
	    enum privsep_procid, int);
struct imsgbuf *
	 proc_ibuf(struct privsep *, enum privsep_procid, int);
struct imsgev *
	 proc_iev(struct privsep *, enum privsep_procid, int);

/* util.c */
int	 socket_af(struct sockaddr *, in_port_t);
in_port_t
	 socket_getport(struct sockaddr *);
int	 socket_setport(struct sockaddr *, in_port_t);
int	 socket_getaddr(int, struct sockaddr_storage *);
int	 socket_bypass(int, struct sockaddr *);
int	 udp_bind(struct sockaddr *, in_port_t);
ssize_t	 sendtofrom(int, void *, size_t, int, struct sockaddr *,
	    socklen_t, struct sockaddr *, socklen_t);
ssize_t	 recvfromto(int, void *, size_t, int, struct sockaddr *,
	    socklen_t *, struct sockaddr *, socklen_t *);
const char *
	 print_spi(uint64_t, int);
const char *
	 print_map(unsigned int, struct iked_constmap *);
void	 lc_string(char *);
void	 print_hex(uint8_t *, off_t, size_t);
void	 print_hexval(uint8_t *, off_t, size_t);
const char *
	 print_bits(unsigned short, unsigned char *);
int	 sockaddr_cmp(struct sockaddr *, struct sockaddr *, int);
uint8_t mask2prefixlen(struct sockaddr *);
uint8_t mask2prefixlen6(struct sockaddr *);
struct in6_addr *
	 prefixlen2mask6(uint8_t, uint32_t *);
uint32_t
	 prefixlen2mask(uint8_t);
const char *
	 print_host(struct sockaddr *, char *, size_t);
char	*get_string(uint8_t *, size_t);
const char *
	 print_proto(uint8_t);
int	 expand_string(char *, size_t, const char *, const char *);
uint8_t *string2unicode(const char *, size_t *);
void	 print_debug(const char *, ...)
	    __attribute__((format(printf, 1, 2)));
void	 print_verbose(const char *, ...)
	    __attribute__((format(printf, 1, 2)));

/* imsg_util.c */
struct ibuf *
	 ibuf_new(const void *, size_t);
struct ibuf *
	 ibuf_static(void);
int	 ibuf_cat(struct ibuf *, struct ibuf *);
void	 ibuf_release(struct ibuf *);
size_t	 ibuf_length(struct ibuf *);
int	 ibuf_setsize(struct ibuf *, size_t);
uint8_t *
	 ibuf_data(struct ibuf *);
void	*ibuf_getdata(struct ibuf *, size_t);
struct ibuf *
	 ibuf_get(struct ibuf *, size_t);
struct ibuf *
	 ibuf_dup(struct ibuf *);
struct ibuf *
	 ibuf_random(size_t);
int	 ibuf_prepend(struct ibuf *, void *, size_t);
void	*ibuf_advance(struct ibuf *, size_t);
void	 ibuf_zero(struct ibuf *);

/* log.c */
void	log_init(int, int);
void	log_procinit(const char *);
void	log_setverbose(int);
int	log_getverbose(void);
void	log_warn(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void	log_warnx(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void	log_info(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void	log_debug(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void	logit(int, const char *, ...)
	    __attribute__((__format__ (printf, 2, 3)));
void	vlog(int, const char *, va_list)
	    __attribute__((__format__ (printf, 2, 0)));
__dead void fatal(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
__dead void fatalx(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));

/* ocsp.c */
int	 ocsp_connect(struct iked *env);
int	 ocsp_receive_fd(struct iked *, struct imsg *);
int	 ocsp_validate_cert(struct iked *, struct iked_static_id *,
    void *, size_t, struct iked_sahdr, uint8_t);

/* parse.y */
int	 parse_config(const char *, struct iked *);
void	 print_user(struct iked_user *);
void	 print_policy(struct iked_policy *);
size_t	 keylength_xf(unsigned int, unsigned int, unsigned int);
size_t	 noncelength_xf(unsigned int, unsigned int);
int	 cmdline_symset(char *);

#endif /* IKED_H */
