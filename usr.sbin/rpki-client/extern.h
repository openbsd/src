/*	$OpenBSD: extern.h,v 1.87 2021/11/01 09:12:18 claudio Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef EXTERN_H
#define EXTERN_H

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/time.h>

#include <openssl/x509.h>

enum cert_as_type {
	CERT_AS_ID, /* single identifier */
	CERT_AS_INHERIT, /* inherit from parent */
	CERT_AS_RANGE, /* range of identifiers */
};

/*
 * An AS identifier range.
 * The maximum AS identifier is an unsigned 32 bit integer (RFC 6793).
 */
struct cert_as_range {
	uint32_t	 min; /* minimum non-zero */
	uint32_t	 max; /* maximum */
};

/*
 * An autonomous system (AS) object.
 * AS identifiers are unsigned 32 bit integers (RFC 6793).
 */
struct cert_as {
	enum cert_as_type type; /* type of AS specification */
	union {
		uint32_t id; /* singular identifier */
		struct cert_as_range range; /* range */
	};
};

/*
 * AFI values are assigned by IANA.
 * In rpki-client, we only accept the IPV4 and IPV6 AFI values.
 */
enum afi {
	AFI_IPV4 = 1,
	AFI_IPV6 = 2
};

/*
 * An IP address as parsed from RFC 3779, section 2.2.3.8.
 * This is either in a certificate or an ROA.
 * It may either be IPv4 or IPv6.
 */
struct ip_addr {
	unsigned char	 addr[16]; /* binary address prefix */
	unsigned char	 prefixlen; /* number of valid bits in address */
};

/*
 * An IP address (IPv4 or IPv6) range starting at the minimum and making
 * its way to the maximum.
 */
struct ip_addr_range {
	struct ip_addr min; /* minimum ip */
	struct ip_addr max; /* maximum ip */
};

enum cert_ip_type {
	CERT_IP_ADDR, /* IP address range w/shared prefix */
	CERT_IP_INHERIT, /* inherited IP address */
	CERT_IP_RANGE /* range of IP addresses */
};

/*
 * A single IP address family (AFI, address or range) as defined in RFC
 * 3779, 2.2.3.2.
 * The RFC specifies multiple address or ranges per AFI; this structure
 * encodes both the AFI and a single address or range.
 */
struct cert_ip {
	enum afi		afi; /* AFI value */
	enum cert_ip_type	type; /* type of IP entry */
	unsigned char		min[16]; /* full range minimum */
	unsigned char		max[16]; /* full range maximum */
	union {
		struct ip_addr ip; /* singular address */
		struct ip_addr_range range; /* range */
	};
};

enum cert_purpose {
	CERT_PURPOSE_INVALID,
	CERT_PURPOSE_CA,
	CERT_PURPOSE_BGPSEC_ROUTER
};

/*
 * Parsed components of a validated X509 certificate stipulated by RFC
 * 6847 and further (within) by RFC 3779.
 * All AS numbers are guaranteed to be non-overlapping and properly
 * inheriting.
 */
struct cert {
	struct cert_ip	*ips; /* list of IP address ranges */
	size_t		 ipsz; /* length of "ips" */
	struct cert_as	*as; /* list of AS numbers and ranges */
	size_t		 asz; /* length of "asz" */
	char		*repo; /* CA repository (rsync:// uri) */
	char		*mft; /* manifest (rsync:// uri) */
	char		*notify; /* RRDP notify (https:// uri) */
	char		*crl; /* CRL location (rsync:// or NULL) */
	char		*aia; /* AIA (or NULL, for trust anchor) */
	char		*aki; /* AKI (or NULL, for trust anchor) */
	char		*ski; /* SKI */
	char		*tal; /* basename of TAL for this cert */
	enum cert_purpose	 purpose; /* Certificate Purpose (BGPSec or CA) */
	char		*pubkey; /* Subject Public Key Info */
	int		 valid; /* validated resources */
	X509		*x509; /* the cert */
	time_t		 expires; /* do not use after */
};

/*
 * The TAL file conforms to RFC 7730.
 * It is the top-level structure of RPKI and defines where we can find
 * certificates for TAs (trust anchors).
 * It also includes the public key for verifying those trust anchor
 * certificates.
 */
struct tal {
	char		**uri; /* well-formed rsync URIs */
	size_t		 urisz; /* number of URIs */
	unsigned char	*pkey; /* DER-encoded public key */
	size_t		 pkeysz; /* length of pkey */
	char		*descr; /* basename of tal file */
};

/*
 * Files specified in an MFT have their bodies hashed with SHA256.
 */
struct mftfile {
	char		*file; /* filename (CER/ROA/CRL, no path) */
	unsigned char	 hash[SHA256_DIGEST_LENGTH]; /* sha256 of body */
};

/*
 * A manifest, RFC 6486.
 * This consists of a bunch of files found in the same directory as the
 * manifest file.
 */
struct mft {
	char		*file; /* full path of MFT file */
	struct mftfile	*files; /* file and hash */
	size_t		 filesz; /* number of filenames */
	int		 stale; /* if a stale manifest */
	char		*seqnum; /* manifestNumber */
	char		*aia; /* AIA */
	char		*aki; /* AKI */
	char		*ski; /* SKI */
};

/*
 * An IP address prefix for a given ROA.
 * This encodes the maximum length, AFI (v6/v4), and address.
 * FIXME: are the min/max necessary or just used in one place?
 */
struct roa_ip {
	enum afi	 afi; /* AFI value */
	size_t		 maxlength; /* max length or zero */
	unsigned char	 min[16]; /* full range minimum */
	unsigned char	 max[16]; /* full range maximum */
	struct ip_addr	 addr; /* the address prefix itself */
};

/*
 * An ROA, RFC 6482.
 * This consists of the concerned ASID and its IP prefixes.
 */
struct roa {
	uint32_t	 asid; /* asID of ROA (if 0, RFC 6483 sec 4) */
	struct roa_ip	*ips; /* IP prefixes */
	size_t		 ipsz; /* number of IP prefixes */
	int		 valid; /* validated resources */
	char		*aia; /* AIA */
	char		*aki; /* AKI */
	char		*ski; /* SKI */
	char		*tal; /* basename of TAL for this cert */
	time_t		 expires; /* do not use after */
};

/*
 * A single Ghostbuster record
 */
struct gbr {
	char		*vcard;
	char		*aia; /* AIA */
	char		*aki; /* AKI */
	char		*ski; /* SKI */
};

/*
 * A single VRP element (including ASID)
 */
struct vrp {
	RB_ENTRY(vrp)	entry;
	struct ip_addr	addr;
	uint32_t	asid;
	char		*tal; /* basename of TAL for this cert */
	enum afi	afi;
	unsigned char	maxlength;
	time_t		expires; /* transitive expiry moment */
};
/*
 * Tree of VRP sorted by afi, addr, maxlength and asid
 */
RB_HEAD(vrp_tree, vrp);
RB_PROTOTYPE(vrp_tree, vrp, entry, vrpcmp);

/*
 * A single BGPsec Router Key (including ASID)
 */
struct brk {
	RB_ENTRY(brk)	 entry;
	uint32_t	 asid;
	char		*tal; /* basename of TAL for this key */
	char		*ski; /* Subject Key Identifier */
	char		*pubkey; /* Subject Public Key Info */
	time_t		 expires; /* transitive expiry moment */
};
/*
 * Tree of BRK sorted by asid
 */
RB_HEAD(brk_tree, brk);
RB_PROTOTYPE(brk_tree, brk, entry, brkcmp);

/*
 * A single CRL
 */
struct crl {
	RB_ENTRY(crl)	 entry;
	char		*aki;
	X509_CRL	*x509_crl;
	time_t		 expires; /* do not use after */
};
/*
 * Tree of CRLs sorted by uri
 */
RB_HEAD(crl_tree, crl);
RB_PROTOTYPE(crl_tree, crl, entry, crlcmp);

/*
 * An authentication tuple.
 * This specifies a public key and a subject key identifier used to
 * verify children nodes in the tree of entities.
 */
struct auth {
	RB_ENTRY(auth)	 entry;
	struct cert	*cert; /* owner information */
	struct auth	*parent; /* pointer to parent or NULL for TA cert */
};
/*
 * Tree of auth sorted by ski
 */
RB_HEAD(auth_tree, auth);
RB_PROTOTYPE(auth_tree, auth, entry, authcmp);

struct auth *auth_find(struct auth_tree *, const char *);

/*
 * Resource types specified by the RPKI profiles.
 * There might be others we don't consider.
 */
enum rtype {
	RTYPE_EOF = 0,
	RTYPE_TAL,
	RTYPE_MFT,
	RTYPE_ROA,
	RTYPE_CER,
	RTYPE_CRL,
	RTYPE_GBR,
};

enum http_result {
	HTTP_FAILED,	/* anything else */
	HTTP_OK,	/* 200 OK */
	HTTP_NOT_MOD,	/* 304 Not Modified */
};

/*
 * Message types for communication with RRDP process.
 */
enum rrdp_msg {
	RRDP_START,
	RRDP_SESSION,
	RRDP_FILE,
	RRDP_END,
	RRDP_HTTP_REQ,
	RRDP_HTTP_INI,
	RRDP_HTTP_FIN
};

/*
 * RRDP session state, needed to pickup at the right spot on next run.
 */
struct rrdp_session {
	char			*last_mod;
	char			*session_id;
	long long		 serial;
};

/*
 * File types used in RRDP_FILE messages.
 */
enum publish_type {
	PUB_ADD,
	PUB_UPD,
	PUB_DEL,
};

/*
 * An entity (MFT, ROA, certificate, etc.) that needs to be downloaded
 * and parsed.
 */
struct entity {
	enum rtype	 type;		/* type of entity (not RTYPE_EOF) */
	char		*file;		/* local path to file */
	int		 has_data;	/* whether data blob is specified */
	unsigned char	*data;		/* optional data blob */
	size_t		 datasz; 	/* length of optional data blob */
	char		*descr;		/* tal description */
	TAILQ_ENTRY(entity) entries;
};
TAILQ_HEAD(entityq, entity);

struct repo;
struct filepath;
RB_HEAD(filepath_tree, filepath);


/*
 * Statistics collected during run-time.
 */
struct stats {
	size_t	 tals; /* total number of locators */
	size_t	 mfts; /* total number of manifests */
	size_t	 mfts_fail; /* failing syntactic parse */
	size_t	 mfts_stale; /* stale manifests */
	size_t	 certs; /* certificates */
	size_t	 certs_fail; /* failing syntactic parse */
	size_t	 certs_invalid; /* invalid resources */
	size_t	 roas; /* route origin authorizations */
	size_t	 roas_fail; /* failing syntactic parse */
	size_t	 roas_invalid; /* invalid resources */
	size_t	 repos; /* repositories */
	size_t	 rsync_repos; /* synced rsync repositories */
	size_t	 rsync_fails; /* failed rsync repositories */
	size_t	 http_repos; /* synced http repositories */
	size_t	 http_fails; /* failed http repositories */
	size_t	 rrdp_repos; /* synced rrdp repositories */
	size_t	 rrdp_fails; /* failed rrdp repositories */
	size_t	 crls; /* revocation lists */
	size_t	 gbrs; /* ghostbuster records */
	size_t	 vrps; /* total number of vrps */
	size_t	 uniqs; /* number of unique vrps */
	size_t	 del_files; /* number of files removed in cleanup */
	size_t	 del_dirs; /* number of directories removed in cleanup */
	size_t	 brks; /* number of BGPsec Router Key (BRK) certificates */
	size_t	 brks_invalids; /* invalid BGPsec certs */
	char	*talnames;
	struct timeval	elapsed_time;
	struct timeval	user_time;
	struct timeval	system_time;
};

struct ibuf;
struct msgbuf;

/* global variables */
extern int verbose;

/* Routines for RPKI entities. */

void		 tal_buffer(struct ibuf *, const struct tal *);
void		 tal_free(struct tal *);
struct tal	*tal_parse(const char *, char *, size_t);
struct tal	*tal_read(struct ibuf *);

void		 cert_buffer(struct ibuf *, const struct cert *);
void		 cert_free(struct cert *);
struct cert	*cert_parse(X509 **, const char *, const unsigned char *,
		    size_t);
struct cert	*ta_parse(X509 **, const char *, const unsigned char *, size_t,
		    const unsigned char *, size_t);
struct cert	*cert_read(struct ibuf *);
void		 cert_insert_brks(struct brk_tree *, struct cert *);

void		 mft_buffer(struct ibuf *, const struct mft *);
void		 mft_free(struct mft *);
struct mft	*mft_parse(X509 **, const char *, const unsigned char *,
		    size_t);
int		 mft_check(const char *, struct mft *);
struct mft	*mft_read(struct ibuf *);

void		 roa_buffer(struct ibuf *, const struct roa *);
void		 roa_free(struct roa *);
struct roa	*roa_parse(X509 **, const char *, const unsigned char *,
		    size_t);
struct roa	*roa_read(struct ibuf *);
void		 roa_insert_vrps(struct vrp_tree *, struct roa *, size_t *,
		    size_t *);

void		 gbr_free(struct gbr *);
struct gbr	*gbr_parse(X509 **, const char *, const unsigned char *,
		    size_t);

/* crl.c */
X509_CRL	*crl_parse(const char *, const unsigned char *, size_t);
void		 free_crl(struct crl *);

/* Validation of our objects. */

struct auth	*valid_ski_aki(const char *, struct auth_tree *,
		    const char *, const char *);
int		 valid_ta(const char *, struct auth_tree *,
		    const struct cert *);
int		 valid_cert(const char *, struct auth_tree *,
		    const struct cert *);
int		 valid_roa(const char *, struct auth_tree *, struct roa *);
int		 valid_filename(const char *);
int		 valid_filehash(const char *, const char *, size_t);
int		 valid_uri(const char *, size_t, const char *);
int		 valid_origin(const char *, const char *);

/* Working with CMS. */
unsigned char	*cms_parse_validate(X509 **, const char *,
		    const unsigned char *, size_t,
		    const ASN1_OBJECT *, size_t *);
int		 cms_econtent_version(const char *, const unsigned char **,
		    size_t, long *);
/* Helper for ASN1 parsing */
int		 ASN1_frame(const char *, size_t,
			const unsigned char **, long *, int *);

/* Work with RFC 3779 IP addresses, prefixes, ranges. */

int		 ip_addr_afi_parse(const char *, const ASN1_OCTET_STRING *,
			enum afi *);
int		 ip_addr_parse(const ASN1_BIT_STRING *,
			enum afi, const char *, struct ip_addr *);
void		 ip_addr_print(const struct ip_addr *, enum afi, char *,
			size_t);
void		 ip_addr_buffer(struct ibuf *, const struct ip_addr *);
void		 ip_addr_range_buffer(struct ibuf *,
			const struct ip_addr_range *);
void		 ip_addr_read(struct ibuf *, struct ip_addr *);
void		 ip_addr_range_read(struct ibuf *, struct ip_addr_range *);
int		 ip_addr_cmp(const struct ip_addr *, const struct ip_addr *);
int		 ip_addr_check_overlap(const struct cert_ip *,
			const char *, const struct cert_ip *, size_t);
int		 ip_addr_check_covered(enum afi, const unsigned char *,
			const unsigned char *, const struct cert_ip *, size_t);
int		 ip_cert_compose_ranges(struct cert_ip *);
void		 ip_roa_compose_ranges(struct roa_ip *);

/* Work with RFC 3779 AS numbers, ranges. */

int		 as_id_parse(const ASN1_INTEGER *, uint32_t *);
int		 as_check_overlap(const struct cert_as *, const char *,
			const struct cert_as *, size_t);
int		 as_check_covered(uint32_t, uint32_t,
			const struct cert_as *, size_t);

/* Parser-specific */
void		 entity_free(struct entity *);
void		 entity_read_req(struct ibuf *, struct entity *);
void		 entityq_flush(struct entityq *, struct repo *);
void		 proc_parser(int) __attribute__((noreturn));

/* Rsync-specific. */

char		*rsync_base_uri(const char *);
void		 proc_rsync(char *, char *, int) __attribute__((noreturn));

/* HTTP and RRDP processes. */

void		 proc_http(char *, int);
void		 proc_rrdp(int);

/* Repository handling */
int		 filepath_add(struct filepath_tree *, char *);
void		 rrdp_save_state(size_t, struct rrdp_session *);
int		 rrdp_handle_file(size_t, enum publish_type, char *,
		    char *, size_t, char *, size_t);
char		*repo_filename(const struct repo *, const char *);
struct repo	*ta_lookup(struct tal *);
struct repo	*repo_lookup(const char *, const char *);
int		 repo_queued(struct repo *, struct entity *);
void		 repo_cleanup(struct filepath_tree *);
void		 repo_free(void);

void		 rsync_finish(size_t, int);
void		 http_finish(size_t, enum http_result, const char *);
void		 rrdp_finish(size_t, int);

void		 rsync_fetch(size_t, const char *, const char *);
void		 http_fetch(size_t, const char *, const char *, int);
void		 rrdp_fetch(size_t, const char *, const char *,
		    struct rrdp_session *);
void		 rrdp_http_done(size_t, enum http_result, const char *);


/* Logging (though really used for OpenSSL errors). */

void		 cryptowarnx(const char *, ...)
			__attribute__((format(printf, 1, 2)));
void		 cryptoerrx(const char *, ...)
			__attribute__((format(printf, 1, 2)))
			__attribute__((noreturn));

/* Encoding functions for hex and base64. */

unsigned char	*load_file(const char *, size_t *);
int		 base64_decode_len(size_t, size_t *);
int		 base64_decode(const unsigned char *, size_t,
		    unsigned char **, size_t *);
int		 base64_encode_len(size_t, size_t *);
int		 base64_encode(const unsigned char *, size_t, char **);
char		*hex_encode(const unsigned char *, size_t);


/* Functions for moving data between processes. */

struct ibuf	*io_new_buffer(void);
void		 io_simple_buffer(struct ibuf *, const void *, size_t);
void		 io_buf_buffer(struct ibuf *, const void *, size_t);
void		 io_str_buffer(struct ibuf *, const char *);
void		 io_close_buffer(struct msgbuf *, struct ibuf *);
void		 io_read_buf(struct ibuf *, void *, size_t);
void		 io_read_str(struct ibuf *, char **);
void		 io_read_buf_alloc(struct ibuf *, void **, size_t *);
struct ibuf	*io_buf_read(int, struct ibuf **);
struct ibuf	*io_buf_recvfd(int, struct ibuf **);

/* X509 helpers. */

char		*x509_get_aia(X509 *, const char *);
char		*x509_get_aki(X509 *, int, const char *);
char		*x509_get_ski(X509 *, const char *);
int		 x509_get_expire(X509 *, const char *, time_t *);
char		*x509_get_crl(X509 *, const char *);
char		*x509_crl_get_aki(X509_CRL *, const char *);
char		*x509_get_pubkey(X509 *, const char *);
enum cert_purpose	 x509_get_purpose(X509 *, const char *);

/* printers */
void		tal_print(const struct tal *);
void		cert_print(const struct cert *);
void		mft_print(const struct mft *);
void		roa_print(const struct roa *);
void		gbr_print(const struct gbr *);

/* Output! */

extern int	 outformats;
#define FORMAT_OPENBGPD	0x01
#define FORMAT_BIRD	0x02
#define FORMAT_CSV	0x04
#define FORMAT_JSON	0x08

int		 outputfiles(struct vrp_tree *v, struct brk_tree *b,
		    struct stats *);
int		 outputheader(FILE *, struct stats *);
int		 output_bgpd(FILE *, struct vrp_tree *, struct brk_tree *,
		    struct stats *);
int		 output_bird1v4(FILE *, struct vrp_tree *, struct brk_tree *,
		    struct stats *);
int		 output_bird1v6(FILE *, struct vrp_tree *, struct brk_tree *,
		    struct stats *);
int		 output_bird2(FILE *, struct vrp_tree *, struct brk_tree *,
		    struct stats *);
int		 output_csv(FILE *, struct vrp_tree *, struct brk_tree *,
		    struct stats *);
int		 output_json(FILE *, struct vrp_tree *, struct brk_tree *,
		    struct stats *);

void		logx(const char *fmt, ...)
		    __attribute__((format(printf, 1, 2)));

int	mkpath(const char *);

#define		RPKI_PATH_OUT_DIR	"/var/db/rpki-client"
#define		RPKI_PATH_BASE_DIR	"/var/cache/rpki-client"

/*
 * Maximum number of ip ranges and AS ranges we will accept in
 * any single file
 */
#define MAX_IP_SIZE	200000
#define MAX_AS_SIZE	200000

/*
 * Maximum URI length we will accept
 */
#define MAX_URI_LENGTH 2048

/*
 * Maximum File Size we will accept
 */
#define MAX_FILE_SIZE 2000000

/*
 * Maximum number of FileAndHash entries per Manifest.
 */
#define	MAX_MANIFEST_ENTRIES	100000

#endif /* ! EXTERN_H */
