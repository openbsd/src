/*	$id$	*/

/*
 * The author of this code is Angelos D. Keromytis, angelos@openbsd.org
 * 	(except when noted otherwise).
 *
 * Copyright (C) 1997, 1998, 1999 by John Ioannidis and Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */


/*
 * PF_KEYv2 definitions
 */

#define	PF_KEY_V2	0

struct sadb_msg
{
    u_int8_t  sadb_msg_version;   /* Must be PF_KEY_V2 */
    u_int8_t  sadb_msg_type;      
    u_int8_t  sadb_msg_errno;     /* Should be zero for messages to kernel */
    u_int8_t  sadb_msg_sa_type;   
    u_int16_t sadb_msg_len;       /* In 32-bit words, inclusive */
    u_int16_t sadb_msg_reserved;  /* Set to zero */
    u_int32_t sadb_msg_seq;       
    u_int32_t sadb_msg_pid;       /* PID of originating process, 0 if kernel */
};


struct sadb_hdr
{
    u_int16_t sadb_hdr_len;       /* In 32-bit words, inclusive */
    u_int16_t sadb_hdr_hdrtype;   /* 0 is reserved */
};

struct sadb_sa
{
    u_int16_t sadb_sa_len;                 /* In 32-bit words, inclusive */
    u_int16_t sadb_sa_hdrtype;             /* ASSOCIATION */
    u_int16_t sadb_sa_spi;                 /* Network byte order */
    u_int8_t  sadb_sa_replay_window_len;   /* Set to 0 if not in use */
    u_int8_t  sadb_sa_state;               /* Set to zero by sending process */
    u_int8_t  sadb_sa_encrypt;             /* Encryption algorithm */
    u_int8_t  sadb_sa_auth;                /* Authentication algorithm */
    u_int32_t sadb_sa_flags;               /* Bitmask */
};

struct sadb_lifetime 
{
    u_int16_t sadb_lifetime_len;          /* In 32-bit words, inclusive */
    u_int16_t sadb_lifetime_hdrtype;      /* LIFETIME */
    u_int8_t  sadb_lifetime_which;        /* Bitmask */
    u_int8_t  sadb_lifetime_reserved[3];  /* Padding */
};

struct sadb_lifetime_val
{
    u_int8_t  sadb_lifetime_val_which;      /* Corresponds to lifetime_which */
    u_int8_t  sadb_lifetime_val_reserved;
    u_int16_t sadb_lifetime_val_allocations; /* How many "flows" to use for */
    u_int32_t sadb_lifetime_val_bytes;       /* Number of bytes before expr */
    time_t    sadb_lifetime_val_absolute;
    time_t    sadb_lifetime_val_updatetime;
    time_t    sadb_lifetime_val_usetime;
};

struct sadb_address
{
    u_int16_t sadb_address_len;         /* In 32-bit words, inclusive */
    u_int16_t sadb_address_hdrtype;     /* ADDRESS */
    u_int8_t  sadb_address_which;       /* Bitmask */
    u_int8_t  sadb_address_reserved[3]; /* Padding */
    /* Followed by one or more sockaddr structures */
};

struct sadb_keyblk
{
    u_int16_t sadb_keyblk_len;          /* In 32-bit words, inclusive */
    u_int16_t sadb_keyblk_hdrtype;      /* KEY */
    u_int8_t  sadb_keyblk_which;        /* Bitmask */
    u_int8_t  sadb_keyblk_reserved[3];  /* Padding */
    /* Followed by sadb_key */
};

struct sadb_key
{
    u_int16_t sadb_key_len;             /* Length of key in bits */
    u_int16_t sadb_key_which;           /* Corresponds to keyblk_which */
    u_int8_t  sadb_key_type;            /* 3DES, DES, HMAC-MD5, etc. */
    /* Actual key follows */
};

struct sadb_id
{
    u_int16_t sadb_id_len;         /* In 32-bit words, inclusive */
    u_int16_t sadb_id_hdrtype;     /* IDENTITY */
    u_int8_t  sadb_id_which;       /* Bitmask */
    u_int8_t  sadb_id_reserved[3]; /* Padding */
    /* Followed by one or more sadb_certids */
};

struct sadb_certid
{
    u_int16_t sadb_certid_len;    /* In 32-bit words, inclusive */
    u_int16_t sadb_certid_type;
    /* Cert id. follows */
};

struct sadb_sens
{
    u_int16_t sadb_sens_len;              /* In 32-bit words, inclusive */
    u_int16_t sadb_sens_hdrtype;          /* SENSITIVITY */
    u_int32_t sadb_sens_dpd;              /* Protection Domain */
    u_int8_t  sadb_sens_level;
    u_int8_t  sadb_sens_sens_bitmap_len;  /* In 32-bit words */
    u_int8_t  sadb_sens_integ_level;
    u_int8_t  sadb_sens_integ_bitmap_len; /* In 32-bit words */
    /*
     * Followed by 2 u_int32_t arrays
     * u_int32_t sadb_sens_bitmap[sens_bitmap_len];
     * u_int32_t integ_bitmap[integ_bitmap_len];
     */
};

struct sadb_prop
{
    u_int16_t sadb_prop_len;        /* In 32-bit words, inclusive */
    u_int16_t sadb_prop_hdrtype;    /* PROPOSAL */
    u_int8_t  sadb_prop_num;
    u_int8_t  sadb_prop_replay;     /* Replay window size */
    u_int16_t sadb_prop_reserved;
};

struct sadb_comb
{
    u_int8_t  sadb_comb_auth;
    u_int8_t  sadb_comb_encr;
    u_int16_t sadb_comb_flags;
    u_int16_t sadb_comb_auth_keylen_min;
    u_int16_t sadb_comb_auth_keylen_max;
    u_int16_t sadb_comb_encr_keylen_min;
    u_int16_t sadb_comb_encr_keylen_max;
};

struct sadb_alg
{
    u_int16_t sadb_alg_len;        /* In 32-bit words, inclusive */
    u_int16_t sadb_alg_hdrtype;    /* SUPPORTED */
    u_int8_t  sadb_alg_num_auth;   /* Number of auth algorithms */
    u_int8_t  sadb_alg_num_encrypt;
    /* Followed by one or more sadb_algd */
};

struct sadb_algd
{
    u_int8_t  sadb_algd_type;        /* Algorithm type */
    u_int8_t  sadb_algd_ivlen;       /* IV len, in bits */
    u_int16_t sadb_algd_minlen;      /* Minimum key length, in bits */
    u_int16_t sadb_algd_maxlen;      /* Maximum key length, in bits */
    u_int16_t sadb_algd_reserved;
};

struct sadb_spirange
{
    u_int16_t sadb_spirage_len;      /* In 32-bit words, inclusive */
    u_int16_t sadb_spirage_hdrtype;  /* SPI_RANGE */
    u_int32_t sadb_spirange_low;
    u_int32_t sadb_spirange_hi;
};

/* Message types */

#define SADB_GETSPI     1
#define SADB_UPDATE     2
#define SADB_ADD        3
#define SADB_DELETE     4
#define SADB_GET        5
#define SADB_ACQUIRE    6
#define SADB_REGISTER   7
#define SADB_EXPIRE     8
#define SADB_FLUSH      9

#define SADB_DUMP       10   /* Not used normally */

#define SADB_MAX        10

/* Security association flags */

#define SA_USED         0x1   /* SA used/not used */
#define SA_UNIQUE       0x2   /* SA unique/reusable */
#define SA_INBOUND      0x4   /* SA for packets destined here */
#define SA_OUTBOUND     0x8   /* SA for packets sourced here */
#define SA_FORWARD      0x10  /* SA for packets forwarded through */
#define SA_PFS          0x20  /* Perfect Forward Secrecy ? */
#define SA_REPLAY       0x40  /* Replay protection ? */

/* Security association state */

#define SA_STATE_LARVAL  0
#define SA_STATE_MATURE  1
#define SA_STATE_DYING   2
#define SA_STATE_DEAD    3

#define SA_STATE_MAX     3

/* Security association type */

#define SADB_SATYPE_NONE   0
#define SADB_SATYPE_AH     1  /* RFC-1826 */
#define SADB_SATYPE_ESP    2  /* RFC-1827 */
#define SADB_SATYPE_RSVP   3  /* RVSP Authentication */
#define SADB_SATYPE_OSPFV2 4  /* OSPFv2 Authentication */
#define SADB_SATYPE_RIPV2  5  /* RIPv2 Authentication */
#define SADB_SATYPE_MIPV4  6  /* Mobile IPv4 Authentication */

#define SADB_SATYPE_MAX    6

/* Algorithm types */

/* Authentication algorithms */

#define SADB_AALG_NONE      0
#define SADB_AALG_MD5_HMAC  1
#define SADB_AALG_SHA1_HMAC 2

#define SADB_AALG_MAX       2

/* Encryption algorithms */

#define SADB_EALG_NONE      0
#define SADB_EALG_DES_CBC   1
#define SADB_EALG_3DES      2
#define SADB_EALG_RC5       3

#define SADB_EALG_MAX       3

/* Extension header values */

#define SA_EXT_ASSOCIATION  1
#define SA_EXT_LIFETIME     2
#define SA_EXT_ADDRESS      3
#define SA_EXT_KEY          4
#define SA_EXT_IDENTITY     5
#define SA_EXT_SENSITIVITY  6
#define SA_EXT_PROPOSAL     7
#define SA_EXT_SUPPORTED    8
#define SA_EXT_SPI_RANGE    9

#define SA_EXT_MAX          9

/* Address extension values */

#define SADB_ADDR_SRC       0x1  /* Source */
#define SADB_ADDR_DST       0x2  /* Destination */
#define SADB_ADDR_INNER_SRC 0x4  /* Inner-packet src */
#define SADB_ADDR_INNER_DST 0x8  /* Inner-packet dst */
#define SADB_ADDR_PROXY     0x10 /* Proxy address */

/* Lifetime extension values */

#define SADB_LIFETIME_HARD      0x1   /* Hard lifetime */
#define SADB_LIFETIME_SOFT      0x2   /* Soft lifetime */
#define SADB_LIFETIME_CURRENT   0x4   /* Current lifetime left */

/* Key extension values */

#define SADB_KEYBLK_AUTH      0x1     /* Authentication key */
#define SADB_KEYBLK_ENCRYPT   0x2     /* Encryption key */

/* Identity extension values */

#define SADB_ID_SRC    0x1
#define SADB_ID_DST    0x2

/* Identity type */

#define SADB_IDT_IPV4_ADDR    1
#define SADB_IDT_IPV6_ADDR    2
#define SADB_IDT_IPV4_RANGE   3
#define SADB_IDT_IPV6_RANGE   4
#define SADB_IDT_FQDN         5
#define SADB_IDT_USER_FQDN    6
#define SADB_IDT_IPV4_CONNID  7
#define SADB_IDT_IPV6_CONNID  8

#define SADB_IDT_MAX          8

/* Sensitivity extension values */

#define SADB_DPD_NONE        0
#define SADB_DPD_DOD_GENSER  1
#define SADB_DPD_DOD_SCI     2
#define SADB_DPD_DOE         3
#define SADB_DPD_NATO        4

#define SADB_DPD_MAX         4
