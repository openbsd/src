/*	$OpenBSD: iked.h,v 1.1 2010/06/03 16:41:12 reyk Exp $	*/
/*	$vantronix: iked.h,v 1.61 2010/06/03 07:57:33 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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
#include <imsg.h>

#include "types.h"
#include "dh.h"

#ifndef _IKED_H
#define _IKED_H

/*
 * Common daemon infrastructure, local imsg etc.
 */

struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	void			*data;
	short			 events;
	const char		*name;
};

#define IMSG_SIZE_CHECK(imsg, p) do {				\
	if (IMSG_DATA_SIZE(imsg) < sizeof(*p))			\
		fatalx("bad length imsg received");		\
} while (0)
#define IMSG_DATA_SIZE(imsg)	((imsg)->hdr.len - IMSG_HEADER_SIZE)

/* initially control.h */
struct control_sock {
	const char	*cs_name;
	struct event	 cs_ev;
	int		 cs_fd;
	int		 cs_restricted;
	void		*cs_env;
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgev		 iev;
};
TAILQ_HEAD(ctl_connlist, ctl_conn);
extern  struct ctl_connlist ctl_conns;

enum iked_procid iked_process;

/*
 * Runtime structures
 */

struct iked_spi {
	u_int64_t	 spi;
	u_int8_t	 spi_size;
	u_int8_t	 spi_protoid;
};

struct iked_proposal {
	u_int8_t			 prop_id;
	u_int8_t			 prop_protoid;

	struct iked_spi			 prop_localspi;	
	struct iked_spi			 prop_peerspi;	

	struct iked_transform		*prop_xforms;
	u_int				 prop_nxforms;

	TAILQ_ENTRY(iked_proposal)	 prop_entry;
};
TAILQ_HEAD(iked_proposals, iked_proposal);

struct iked_addr {
	int				 addr_af;
	struct sockaddr_storage		 addr;
	u_int8_t			 addr_mask;
	int				 addr_net;
	in_port_t			 addr_port;
};

struct iked_flow {
	struct iked_addr		 flow_src;
	struct iked_addr		 flow_dst;
	u_int				 flow_dir;	/* in/out */

	u_int64_t			 flow_peerspi;	/* peer relation */
	u_int				 flow_loaded;	/* pfkey done */
	u_int				 flow_rekey;	/* will be deleted */

	u_int8_t			 flow_saproto;
	u_int8_t			 flow_ipproto;

	struct iked_id			*flow_srcid;
	struct iked_id			*flow_dstid;

	struct iked_addr		*flow_local;	/* outer source */
	struct iked_addr		*flow_peer;	/* outer dest */
	struct iked_sa			*flow_ikesa;	/* parent SA */

	TAILQ_ENTRY(iked_flow)		 flow_entry;
};
TAILQ_HEAD(iked_flows, iked_flow);

struct iked_childsa {
	u_int8_t			 csa_saproto;	/* IPSec protocol */
	u_int				 csa_dir;	/* in/out */

	u_int64_t			 csa_peerspi;	/* peer relation */
	u_int				 csa_loaded;	/* pfkey done */
	u_int				 csa_rekey;	/* will be deleted */

	struct iked_spi			 csa_spi;

	struct ibuf			*csa_encrkey;	/* encryption key */	
	struct iked_transform		*csa_encrxf;	/* encryption xform */

	struct ibuf			*csa_integrkey;	/* auth key */
	struct iked_transform		*csa_integrxf;	/* auth xform */

	struct iked_id			*csa_srcid;
	struct iked_id			*csa_dstid;

	struct iked_addr		*csa_local;	/* outer source */
	struct iked_addr		*csa_peer;	/* outer dest */
	struct iked_sa			*csa_ikesa;	/* parent SA */

	TAILQ_ENTRY(iked_childsa)	 csa_entry;
};
TAILQ_HEAD(iked_childsas, iked_childsa);


struct iked_static_id {
	u_int8_t	id_type;
	u_int8_t	id_length;
	u_int8_t	id_data[IKED_ID_SIZE];
};

struct iked_auth {
	u_int8_t	auth_method;
	u_int8_t	auth_eap;			/* optional EAP */
	u_int8_t	auth_length;			/* zero if EAP */
	u_int8_t	auth_data[IKED_PSK_SIZE];
};

struct iked_cfg {
	u_int8_t			 cfg_action;
	u_int16_t			 cfg_type;
	union {
		struct iked_addr	 address;
	} cfg;
};

struct iked_policy {
	u_int				 pol_id;
	char				 pol_name[IKED_ID_SIZE];

	u_int8_t			 pol_flags;
#define IKED_POLICY_PASSIVE		 0x00
#define IKED_POLICY_DEFAULT		 0x01
#define IKED_POLICY_ACTIVE		 0x02
#define IKED_POLICY_REFCNT		 0x04

	int				 pol_refcnt;

	u_int8_t			 pol_saproto;
	u_int				 pol_ipproto;

	struct sockaddr_storage		 pol_peer;
	u_int8_t			 pol_peermask;
	int				 pol_peernet;

	struct sockaddr_storage		 pol_local;
	u_int8_t			 pol_localmask;
	int				 pol_localnet;

	struct iked_static_id		 pol_peerid;
	struct iked_static_id		 pol_localid;

	struct iked_auth		 pol_auth;

	char				 pol_tag[IKED_TAG_SIZE];

	struct iked_proposals		 pol_proposals;
	size_t				 pol_nproposals;

	struct iked_flows		 pol_flows;
	size_t				 pol_nflows;

	struct iked_cfg			 pol_cfg[IKED_CFG_MAX];
	u_int				 pol_ncfg;

	RB_ENTRY(iked_policy)		 pol_entry;
};
RB_HEAD(iked_policies, iked_policy);

struct iked_hash {
	u_int8_t	 hash_type;	/* PRF or INTEGR */
	u_int16_t	 hash_id;	/* IKE PRF/INTEGR hash id */
	const void	*hash_priv;	/* Identifying the hash alg */
	void		*hash_ctx;	/* Context of the current invocation */
	int		 hash_fixedkey;	/* Requires fixed key length */
	struct ibuf	*hash_key;	/* MAC key derived from key seed */
	size_t		 hash_length;	/* Output length */
	size_t		 hash_trunc;	/* Truncate the output length */
	struct iked_hash *hash_prf;	/* PRF pointer */
};

struct iked_cipher {
	u_int8_t	 encr_type;	/* ENCR */
	u_int16_t	 encr_id;	/* IKE ENCR hash id */
	const void	*encr_priv;	/* Identifying the hash alg */
	void		*encr_ctx;	/* Context of the current invocation */
	int		 encr_fixedkey;	/* Requires fixed key length */
	struct ibuf	*encr_key;	/* MAC key derived from key seed */
	struct ibuf	*encr_iv;	/* Initialization Vector */
	size_t		 encr_ivlength;	/* IV length */
	size_t		 encr_length;	/* Block length */
};

struct iked_dsa {
	u_int8_t	 dsa_method;	/* AUTH method */
	const void	*dsa_priv;	/* PRF or signature hash function */
	void		*dsa_ctx;	/* PRF or signature hash ctx */
	struct ibuf	*dsa_keydata;	/* public, private or shared key */
	void		*dsa_key;	/* parsed public or private key */
	void		*dsa_cert;	/* parsed certificate */
	int		 dsa_hmac;	/* HMAC or public/private key */
	int		 dsa_sign;	/* Sign or verify operation */
};

struct iked_id {
	u_int8_t	 id_type;
	struct ibuf	*id_buf;
};

#define IKED_REQ_CERT		0x01	/* get local certificate (if required) */
#define IKED_REQ_VALID		0x02	/* validate the peer cert */
#define IKED_REQ_AUTH		0x04	/* AUTH payload */
#define IKED_REQ_SA		0x08	/* SA available */
#define IKED_REQ_CHILDSA	0x10	/* Child SA initiated */

#define IKED_REQ_BITS	\
    "\10\01CERT\02VALID\03AUTH\04SA\05CHILDSA"

struct iked_sahdr {
	u_int64_t			 sh_ispi;	/* Initiator SPI */
	u_int64_t			 sh_rspi;	/* Responder SPI */
	u_int				 sh_initiator;	/* Is initiator? */
} __packed;

struct iked_sa {
	struct iked_sahdr		 sa_hdr;
	u_int32_t			 sa_msgid;

	struct iked_addr		 sa_peer;
	struct iked_addr		 sa_local;
	int				 sa_fd;

	int				 sa_natt;	/* for IKE messages */
	int				 sa_udpencap;	/* for pfkey */

	int				 sa_state;
	u_int				 sa_stateflags;
	u_int				 sa_staterequire;

	int				 sa_cp;		/* XXX */

	struct iked_policy		*sa_policy;
	struct timeval			 sa_timecreated;
	struct timeval			 sa_timeused;

	char				*sa_tag;

	struct ibuf			*sa_inonce;	/* Ni */
	struct ibuf			*sa_rnonce;	/* Nr */

	struct group			*sa_dhgroup;	/* DH group */
	struct ibuf			*sa_dhiexchange;
	struct ibuf			*sa_dhrexchange;

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

	struct iked_id			 sa_iid;	/* initiator id */
	struct iked_id			 sa_rid;	/* responder id */
	struct iked_id			 sa_icert;	/* initiator cert */
	struct iked_id			 sa_rcert;	/* responder cert */

	char				*sa_eapid;	/* EAP identity */
	struct iked_id			 sa_eap;	/* EAP challenge */
	struct ibuf			*sa_eapmsk;	/* EAK session key */

	struct iked_proposals		 sa_proposals;	/* SA proposals */
	struct iked_childsas		 sa_childsas;	/* IPSec Child SAs */
	struct iked_flows		 sa_flows;	/* IPSec flows */
	u_int8_t			 sa_flowhash[20]; /* SHA1 */

	RB_ENTRY(iked_sa)		 sa_entry;
};
RB_HEAD(iked_sas, iked_sa);

struct iked_message {
	struct ibuf		*msg_data;
	off_t			 msg_offset;

	struct sockaddr_storage	 msg_local;
	socklen_t		 msg_locallen;

	struct sockaddr_storage	 msg_peer;
	socklen_t		 msg_peerlen;

	int			 msg_fd;
	int			 msg_response;
	int			 msg_natt;
	struct iked_message	*msg_decrypted;
	int			 msg_error;

	/* Associated policy and SA */
	struct iked_policy	*msg_policy;
	struct iked_sa		*msg_sa;

	/* Parsed information */
	struct iked_proposals	 msg_proposals;
	struct iked_spi		 msg_rekey;

	/* Parse stack */
	struct iked_proposal	*msg_prop;
	u_int16_t		 msg_attrlength;
};

struct iked_user {
	char			 usr_name[MAXLOGNAME];
	char			 usr_pass[IKED_PASSWORD_SIZE];
	RB_ENTRY(iked_user)	 usr_entry;
};
RB_HEAD(iked_users, iked_user);

/*
 * Daemon configuration
 */

struct iked {
	char				 sc_conffile[MAXPATHLEN];

	u_int32_t			 sc_opts;

	int				 sc_pipes[PROC_MAX][PROC_MAX];
	struct imsgev			 sc_ievs[PROC_MAX];
	const char			*sc_title[PROC_MAX];
	pid_t				 sc_pid[PROC_MAX];
	struct passwd			*sc_pw;

	struct iked_policies		 sc_policies;
	struct iked_policy		*sc_defaultcon;

	struct iked_sas			 sc_sas;
	struct iked_users		 sc_users;

	void				*sc_priv;	/* per-process */

	int				 sc_pfkey;	/* ike process */
	u_int8_t			 sc_certreqtype;
	struct ibuf			*sc_certreq;

	struct control_sock		 sc_csock;

	/* Event and signal handlers */
	struct event			 sc_evsigint;
	struct event			 sc_evsigterm;
	struct event			 sc_evsigchld;
	struct event			 sc_evsighup;
	struct event			 sc_evsigpipe;
};

struct iked_proc {
	const char		*title;
	enum iked_procid	 id;
	int			(*cb)(int, struct iked_proc *, struct imsg *);
	pid_t			(*init)(struct iked *, struct iked_proc *);
	const char		*chroot;
	struct iked		*env;
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
int	 control_init(struct iked *, struct control_sock *);
int	 control_listen(struct control_sock *);
void	 control_cleanup(struct control_sock *);

/* config.c */
struct iked_policy *
	 config_new_policy(struct iked *);
void	 config_free_sa(struct iked *, struct iked_sa *);
struct iked_sa *
	 config_new_sa(struct iked *, int);
struct iked_user *
	 config_new_user(struct iked *, struct iked_user *);
u_int64_t
	 config_getspi(void);
struct iked_transform *
	 config_findtransform(struct iked_proposals *, u_int8_t);
void	 config_free_policy(struct iked *, struct iked_policy *);
struct iked_proposal *
	 config_add_proposal(struct iked_proposals *, u_int, u_int);
void	 config_free_proposals(struct iked_proposals *, u_int);
void	 config_free_flows(struct iked *, struct iked_flows *,
	    struct iked_spi *);
void	 config_free_childsas(struct iked *, struct iked_childsas *,
	    struct iked_spi *, struct iked_spi *);
struct iked_transform *
	 config_add_transform(struct iked_proposal *,
	    u_int, u_int, u_int, u_int);
int	 config_setreset(struct iked *, u_int, enum iked_procid);
int	 config_getreset(struct iked *, struct imsg *);
int	 config_setpolicy(struct iked *, struct iked_policy *,
	    enum iked_procid);
int	 config_getpolicy(struct iked *, struct imsg *);
int	 config_setsocket(struct iked *, struct sockaddr_storage *, in_port_t,
	    enum iked_procid);
int	 config_getsocket(struct iked *env, struct imsg *,
	    void (*cb)(int, short, void *));
int	 config_setpfkey(struct iked *, enum iked_procid);
int	 config_getpfkey(struct iked *, struct imsg *);
int	 config_setuser(struct iked *, struct iked_user *, enum iked_procid);
int	 config_getuser(struct iked *, struct imsg *);

/* policy.c */
void	 policy_init(struct iked *);
int	 policy_lookup(struct iked *, struct iked_message *);
void	 policy_ref(struct iked *, struct iked_policy *);
void	 policy_unref(struct iked *, struct iked_policy *);
void	 sa_state(struct iked *, struct iked_sa *, int);
void	 sa_stateflags(struct iked_sa *, u_int);
int	 sa_stateok(struct iked_sa *, int);
struct iked_sa *
	 sa_new(struct iked *, u_int64_t, u_int64_t, u_int,
	    struct iked_policy *);
void	 sa_free(struct iked *, struct iked_sa *);
void	 childsa_free(struct iked_childsa *);
void	 flow_free(struct iked_flow *);
struct iked_sa *
	 sa_lookup(struct iked *, u_int64_t, u_int64_t, u_int);
struct iked_user *
	 user_lookup(struct iked *, const char *);
RB_PROTOTYPE(iked_policies, iked_policy, pol_entry, policy_cmp);
RB_PROTOTYPE(iked_sas, iked_sa, sa_entry, sa_cmp);
RB_PROTOTYPE(iked_users, iked_user, user_entry, user_cmp);

/* crypto.c */
struct iked_hash *
	 hash_new(u_int8_t, u_int16_t);
struct ibuf *
	 hash_setkey(struct iked_hash *, void *, size_t);
void	 hash_free(struct iked_hash *);
void	 hash_init(struct iked_hash *);
void	 hash_update(struct iked_hash *, void *, size_t);
void	 hash_final(struct iked_hash *, void *, size_t *);
size_t	 hash_keylength(struct iked_hash *);
size_t	 hash_length(struct iked_hash *);

struct iked_cipher *
	 cipher_new(u_int8_t, u_int16_t, u_int16_t);
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
	 dsa_new(u_int16_t, struct iked_hash *, int);
struct iked_dsa *
	 dsa_sign_new(u_int16_t, struct iked_hash *);
struct iked_dsa *
	 dsa_verify_new(u_int16_t, struct iked_hash *);
struct ibuf *
	 dsa_setkey(struct iked_dsa *, void *, size_t, u_int8_t);
void	 dsa_free(struct iked_dsa *);
int	 dsa_init(struct iked_dsa *);
size_t	 dsa_length(struct iked_dsa *);
int	 dsa_update(struct iked_dsa *, const void *, size_t);
ssize_t	 dsa_sign_final(struct iked_dsa *, void *, size_t);
ssize_t	 dsa_verify_final(struct iked_dsa *, void *, size_t);

/* ikev1.c */
pid_t	 ikev1(struct iked *, struct iked_proc *);

/* ikev2.c */
pid_t	 ikev2(struct iked *, struct iked_proc *);
int	 ikev2_send_ike_e(struct iked *, struct iked_sa *, struct ibuf *,
	    u_int8_t, u_int8_t, int);
int	 ikev2_message_authsign(struct iked *, struct iked_sa *,
	    struct iked_auth *, struct ibuf *);
struct ibuf *
	 ikev2_prfplus(struct iked_hash *, struct ibuf *, struct ibuf *,
	    size_t);
ssize_t	 ikev2_psk(struct iked_sa *, u_int8_t *, size_t, u_int8_t **);

/* eap.c */
ssize_t	 eap_identity_request(struct ibuf *);
int	 eap_parse(struct iked *, struct iked_sa *, void *, int);

/* pfkey.c */
int	 pfkey_flow_add(int fd, struct iked_flow *);
int	 pfkey_flow_delete(int fd, struct iked_flow *);
int	 pfkey_sa_init(int, struct iked_childsa *, u_int32_t *);
int	 pfkey_sa_add(int, struct iked_childsa *, struct iked_childsa *);
int	 pfkey_sa_delete(int, struct iked_childsa *);
int	 pfkey_flush(int);
int	 pfkey_init(void);

/* ca.c */
pid_t	 caproc(struct iked *, struct iked_proc *);
int	 ca_setreq(struct iked *, struct iked_sahdr *, u_int8_t,
	    u_int8_t *, size_t, enum iked_procid);
int	 ca_setcert(struct iked *, struct iked_sahdr *, struct iked_id *,
	    u_int8_t, u_int8_t *, size_t, enum iked_procid);
int	 ca_setauth(struct iked *, struct iked_sa *,
	    struct ibuf *, enum iked_procid);
void	 ca_sslinit(void);
void	 ca_sslerror(void);
char	*ca_asn1_name(u_int8_t *, size_t);
char	*ca_x509_name(void *);

/* proc.c */
void	 init_procs(struct iked *, struct iked_proc *, u_int);
void	 kill_procs(struct iked *);
void	 init_pipes(struct iked *);
void	 config_pipes(struct iked *, struct iked_proc *, u_int);
void	 config_procs(struct iked *, struct iked_proc *, u_int);
void	 purge_config(struct iked *, u_int8_t);
void	 dispatch_proc(int, short event, void *);
pid_t	 run_proc(struct iked *, struct iked_proc *, struct iked_proc *,
	    u_int, void (*)(struct iked *, void *), void *);

/* util.c */
void	 socket_set_blockmode(int, enum blockmodes);
void	 imsg_event_add(struct imsgev *);
int	 imsg_compose_event(struct imsgev *, u_int16_t, u_int32_t,
	    pid_t, int, void *, u_int16_t);
int	 imsg_composev_event(struct imsgev *, u_int16_t, u_int32_t,
	    pid_t, int, const struct iovec *, int);
int	 imsg_compose_proc(struct iked *, enum iked_procid,
	    u_int16_t, int, void *, u_int16_t);
int	 imsg_composev_proc(struct iked *, enum iked_procid,
	    u_int16_t, int, const struct iovec *, int);
int	 imsg_forward_proc(struct iked *, struct imsg *,
	    enum iked_procid);
void	 imsg_flush_proc(struct iked *, enum iked_procid);
int	 socket_af(struct sockaddr *, in_port_t);
in_port_t
	 socket_getport(struct sockaddr_storage *);
int	 socket_bypass(int, struct sockaddr *);
int	 udp_bind(struct sockaddr *, in_port_t);
ssize_t	 recvfromto(int, void *, size_t, int, struct sockaddr *,
	    socklen_t *, struct sockaddr *, socklen_t *);
const char *
	 print_spi(u_int64_t, int);
const char *
	 print_map(u_int, struct iked_constmap *);
void	 print_hex(u_int8_t *, off_t, size_t);
void	 print_hexval(u_int8_t *, off_t, size_t);
const char *
	 print_bits(u_short, char *);
int	 sockaddr_cmp(struct sockaddr *, struct sockaddr *, int);
struct in6_addr *
	 prefixlen2mask6(u_int8_t, u_int32_t *);
u_int32_t
	 prefixlen2mask(u_int8_t);
const char *
	 print_host(struct sockaddr_storage *, char *, size_t);
char	*get_string(u_int8_t *, size_t);
int	 print_id(struct iked_id *, off_t, char *, size_t);
const char *
	 print_proto(u_int8_t);
void	 message_cleanup(struct iked *, struct iked_message *);
int	 expand_string(char *, size_t, const char *, const char *);
u_int8_t *string2unicode(const char *, size_t *);

struct ibuf *
	 ibuf_new(void *, size_t);
struct ibuf *
	 ibuf_static(void);
int	 ibuf_cat(struct ibuf *, struct ibuf *);
void	 ibuf_release(struct ibuf *);
size_t	 ibuf_length(struct ibuf *);
int	 ibuf_setsize(struct ibuf *, size_t);
u_int8_t *
	 ibuf_data(struct ibuf *);
void	*ibuf_get(struct ibuf *, size_t);
struct ibuf *
	 ibuf_copy(struct ibuf *, size_t);
struct ibuf *
	 ibuf_dup(struct ibuf *);
struct ibuf *
	 ibuf_random(size_t);
int	 ibuf_prepend(struct ibuf *, void *, size_t);
void	*ibuf_advance(struct ibuf *, size_t);
void	 ibuf_zero(struct ibuf *);

/* log.c */
void	 log_init(int);
void	 log_verbose(int);
void	 log_warn(const char *, ...);
void	 log_warnx(const char *, ...);
void	 log_info(const char *, ...);
void	 log_debug(const char *, ...);
void	 print_debug(const char *, ...);
void	 print_verbose(const char *, ...);
__dead void fatal(const char *);
__dead void fatalx(const char *);

/* parse.y */
int	 parse_config(const char *, struct iked *);
void	 print_user(struct iked_user *);
void	 print_policy(struct iked_policy *);
size_t	 keylength_xf(u_int, u_int, u_int);

#endif /* _IKED_H */
