/*
%%% copyright-nrl-98
This software is Copyright 1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/
#ifndef _NET_PFKEY_V2_H
#define _NET_PFKEY_V2_H 1

#define PF_KEY_V2 2

#define SADB_RESERVED      0
#define SADB_GETSPI        1
#define SADB_UPDATE        2
#define SADB_ADD           3
#define SADB_DELETE        4
#define SADB_GET           5
#define SADB_ACQUIRE       6
#define SADB_REGISTER      7
#define SADB_EXPIRE        8
#define SADB_FLUSH         9
#define SADB_DUMP          10
#define SADB_X_PROMISC     11
#define SADB_X_ADDFLOW     12
#define SADB_X_DELFLOW     13
#define SADB_X_GRPSPIS     14
#define SADB_X_BINDSA      15
#define SADB_MAX           15

struct sadb_msg {
  uint8_t sadb_msg_version;
  uint8_t sadb_msg_type;
  uint8_t sadb_msg_errno;
  uint8_t sadb_msg_satype;
  uint16_t sadb_msg_len;
  uint16_t sadb_msg_reserved;
  uint32_t sadb_msg_seq;
  uint32_t sadb_msg_pid;
};

struct sadb_ext {
  uint16_t sadb_ext_len;
  uint16_t sadb_ext_type;
};

struct sadb_sa {
  uint16_t sadb_sa_len;
  uint16_t sadb_sa_exttype;
  uint32_t sadb_sa_spi;
  uint8_t sadb_sa_replay;
  uint8_t sadb_sa_state;
  uint8_t sadb_sa_auth;
  uint8_t sadb_sa_encrypt;
  uint32_t sadb_sa_flags;
};

struct sadb_lifetime {
  uint16_t sadb_lifetime_len;
  uint16_t sadb_lifetime_exttype;
  uint32_t sadb_lifetime_allocations;
  uint64_t sadb_lifetime_bytes;
  uint64_t sadb_lifetime_addtime;
  uint64_t sadb_lifetime_usetime;
};

struct sadb_address {
  uint16_t sadb_address_len;
  uint16_t sadb_address_exttype;
  uint32_t sadb_address_reserved;
};

struct sadb_key {
  uint16_t sadb_key_len;
  uint16_t sadb_key_exttype;
  uint16_t sadb_key_bits;
  uint16_t sadb_key_reserved;
};

struct sadb_ident {
  uint16_t sadb_ident_len;
  uint16_t sadb_ident_exttype;
  uint16_t sadb_ident_type;
  uint16_t sadb_ident_reserved;
  uint64_t sadb_ident_id;
};

struct sadb_sens {
  uint16_t sadb_sens_len;
  uint16_t sadb_sens_exttype;
  uint32_t sadb_sens_dpd;
  uint8_t sadb_sens_sens_level;
  uint8_t sadb_sens_sens_len;
  uint8_t sadb_sens_integ_level;
  uint8_t sadb_sens_integ_len;
  uint32_t sadb_sens_reserved;
};

struct sadb_prop {
  uint16_t sadb_prop_len;
  uint16_t sadb_prop_exttype;
  uint8_t sadb_prop_num;
  uint8_t sadb_prop_replay;
  uint16_t sadb_prop_reserved;
};

struct sadb_comb {
  uint8_t sadb_comb_auth;
  uint8_t sadb_comb_encrypt;
  uint16_t sadb_comb_flags;
  uint16_t sadb_comb_auth_minbits;
  uint16_t sadb_comb_auth_maxbits;
  uint16_t sadb_comb_encrypt_minbits;
  uint16_t sadb_comb_encrypt_maxbits;
  uint32_t sadb_comb_reserved;
  uint32_t sadb_comb_soft_allocations;
  uint32_t sadb_comb_hard_allocations;
  uint64_t sadb_comb_soft_bytes;
  uint64_t sadb_comb_hard_bytes;
  uint64_t sadb_comb_soft_addtime;
  uint64_t sadb_comb_hard_addtime;
  uint64_t sadb_comb_soft_usetime;
  uint64_t sadb_comb_hard_usetime;
};

struct sadb_supported {
  uint16_t sadb_supported_len;
  uint16_t sadb_supported_exttype;
  uint8_t sadb_supported_nauth;
  uint8_t sadb_supported_nencrypt;
  uint16_t sadb_supported_reserved;
};

struct sadb_alg {
  uint8_t sadb_alg_type;
  uint8_t sadb_alg_ivlen;
  uint16_t sadb_alg_minbits;
  uint16_t sadb_alg_maxbits;
  uint16_t sadb_alg_reserved;
};

struct sadb_spirange {
  uint16_t sadb_spirange_len;
  uint16_t sadb_spirange_exttype;
  uint32_t sadb_spirange_min;
  uint32_t sadb_spirange_max;
  uint32_t sadb_spirange_reserved;
};

struct sadb_protocol {
  uint16_t sadb_protocol_len;
  uint16_t sadb_protocol_exttype;
  uint8_t  sadb_protocol_proto;
  uint8_t  sadb_protocol_reserved1;
  uint16_t sadb_protocol_reserved2;
};
    
#define SADB_GETSPROTO(x) ( (x) == SADB_SATYPE_AH ? IPPROTO_AH :\
                              (x) == SADB_X_SATYPE_AH_OLD ? IPPROTO_AH :\
                                (x) == SADB_SATYPE_ESP ? IPPROTO_ESP :\
                                  (x) == SADB_X_SATYPE_ESP_OLD ? IPPROTO_ESP :\
                                    (x) == SADB_X_SATYPE_BYPASS ? IPPROTO_IP :\
                                      IPPROTO_IPIP )

#define SADB_EXT_RESERVED             0
#define SADB_EXT_SA                   1
#define SADB_EXT_LIFETIME_CURRENT     2
#define SADB_EXT_LIFETIME_HARD        3
#define SADB_EXT_LIFETIME_SOFT        4
#define SADB_EXT_ADDRESS_SRC          5
#define SADB_EXT_ADDRESS_DST          6
#define SADB_EXT_ADDRESS_PROXY        7
#define SADB_EXT_KEY_AUTH             8
#define SADB_EXT_KEY_ENCRYPT          9
#define SADB_EXT_IDENTITY_SRC         10
#define SADB_EXT_IDENTITY_DST         11
#define SADB_EXT_SENSITIVITY          12
#define SADB_EXT_PROPOSAL             13
#define SADB_EXT_SUPPORTED            14
#define SADB_EXT_SPIRANGE             15
#define SADB_X_EXT_SRC_MASK           16
#define SADB_X_EXT_DST_MASK           17
#define SADB_X_EXT_PROTOCOL           18
#define SADB_X_EXT_SA2                19
#define SADB_X_EXT_SRC_FLOW           20
#define SADB_X_EXT_DST_FLOW           21
#define SADB_X_EXT_DST2               22
#define SADB_EXT_MAX                  22

/* Fix pfkeyv2.c struct pfkeyv2_socket if SATYPE_MAX > 31 */
#define SADB_SATYPE_UNSPEC		 0
#define SADB_SATYPE_AH			 1
#define SADB_SATYPE_ESP			 2
#define SADB_SATYPE_RSVP		 3
#define SADB_SATYPE_OSPFV2		 4
#define SADB_SATYPE_RIPV2		 5
#define SADB_SATYPE_MIP			 6
#define SADB_X_SATYPE_AH_OLD		 7
#define SADB_X_SATYPE_ESP_OLD		 8
#define SADB_X_SATYPE_IPIP		 9
#define SADB_X_SATYPE_TCPSIGNATURE	10
#define SADB_X_SATYPE_BYPASS		11
#define SADB_SATYPE_MAX			11

#define SADB_SASTATE_LARVAL   0
#define SADB_SASTATE_MATURE   1
#define SADB_SASTATE_DYING    2
#define SADB_SASTATE_DEAD     3
#define SADB_SASTATE_MAX      3

#define SADB_AALG_NONE               0
#define SADB_AALG_MD5HMAC            1
#define SADB_AALG_SHA1HMAC           2
#define SADB_AALG_MD5HMAC96          3
#define SADB_AALG_SHA1HMAC96         4
#define SADB_X_AALG_RIPEMD160HMAC96  5
#define SADB_X_AALG_MD5              6
#define SADB_X_AALG_SHA1             7
#define SADB_AALG_MAX                7

#define SADB_EALG_NONE        0
#define SADB_EALG_DESCBC      1
#define SADB_EALG_3DESCBC     2
#define SADB_X_EALG_BLF       3
#define SADB_X_EALG_CAST      4
#define SADB_X_EALG_SKIPJACK  5
#define SADB_EALG_MAX         5

#define SADB_SAFLAGS_PFS         	0x01    /* perfect forward secrecy */
#define SADB_X_SAFLAGS_HALFIV    	0x02    /* Used for ESP-old */
#define SADB_X_SAFLAGS_TUNNEL	 	0x04    /* Force tunneling */
#define SADB_X_SAFLAGS_CHAINDEL  	0x08    /* Delete whole SA chain */
#define SADB_X_SAFLAGS_REPLACEFLOW	0x20    /* Replace existing flow */
#define SADB_X_SAFLAGS_INGRESS_FLOW     0x40    /* Ingress ACL entry */

#define SADB_IDENTTYPE_RESERVED   0
#define SADB_IDENTTYPE_PREFIX     1
#define SADB_IDENTTYPE_FQDN       2
#define SADB_IDENTTYPE_MBOX       3
#define SADB_IDENTTYPE_CONNECTION 4
#define SADB_IDENTTYPE_MAX        4

#define SADB_KEY_FLAGS_MAX 0

#define PFKEYV2_LIFETIME_HARD      0
#define PFKEYV2_LIFETIME_SOFT      1
#define PFKEYV2_LIFETIME_CURRENT   2

#define PFKEYV2_IDENTITY_SRC       0
#define PFKEYV2_IDENTITY_DST       1

#define PFKEYV2_ENCRYPTION_KEY     0
#define PFKEYV2_AUTHENTICATION_KEY 1

#define PFKEYV2_SOCKETFLAGS_REGISTERED 1
#define PFKEYV2_SOCKETFLAGS_PROMISC    2

#define PFKEYV2_SENDMESSAGE_UNICAST    1
#define PFKEYV2_SENDMESSAGE_REGISTERED 2
#define PFKEYV2_SENDMESSAGE_BROADCAST  3

#ifdef _KERNEL
struct tdb;
struct socket;
struct mbuf;

struct pfkey_version
{
    int protocol;
    int (*create)(struct socket *socket);
    int (*release)(struct socket *socket);
    int (*send)(struct socket *socket, void *message, int len);
};

struct pfkeyv2_socket
{
    struct pfkeyv2_socket *next;
    struct socket *socket;
    int flags;
    uint32_t pid;
    uint32_t registration;    /* Increase size if SATYPE_MAX > 31 */
};

struct dump_state
{
    struct sadb_msg *sadb_msg;
    struct socket *socket;
};

int pfkeyv2_init(void);
int pfkeyv2_cleanup(void);
int pfkeyv2_parsemessage(void *, int, void **);
int pfkeyv2_expire(struct tdb *, u_int16_t);
int pfkeyv2_acquire(struct tdb *, int);

int pfkey_register(struct pfkey_version *version);
int pfkey_unregister(struct pfkey_version *version);
int pfkey_sendup(struct socket *socket, struct mbuf *packet, int more);
#endif /* _KERNEL */
#endif /* _NET_PFKEY_V2_H */
