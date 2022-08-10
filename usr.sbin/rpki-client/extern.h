/*	$OpenBSD: extern.h,v 1.147 2022/08/10 10:27:03 job Exp $ */
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
#include <openssl/x509v3.h>

/*
 * Distrusted hosts (loaded from skipfile).
 */
struct skiplistentry {
	LIST_ENTRY(skiplistentry)	 entry;
	char				*value; /* FQDN */
};
LIST_HEAD(skiplist, skiplistentry);

/*
 * Enumeration for ASN.1 explicit tags in RSC eContent
 */
enum rsc_resourceblock_tag {
	RSRCBLK_TYPE_ASID,
	RSRCBLK_TYPE_IPADDRBLK,
};

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
	int		 talid; /* cert is covered by which TAL */
	char		*repo; /* CA repository (rsync:// uri) */
	char		*mft; /* manifest (rsync:// uri) */
	char		*notify; /* RRDP notify (https:// uri) */
	char		*crl; /* CRL location (rsync:// or NULL) */
	char		*aia; /* AIA (or NULL, for trust anchor) */
	char		*aki; /* AKI (or NULL, for trust anchor) */
	char		*ski; /* SKI */
	enum cert_purpose	 purpose; /* BGPSec or CA */
	char		*pubkey; /* Subject Public Key Info */
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
	int		 id; /* ID of this TAL */
};

/*
 * Resource types specified by the RPKI profiles.
 * There might be others we don't consider.
 */
enum rtype {
	RTYPE_INVALID,
	RTYPE_TAL,
	RTYPE_MFT,
	RTYPE_ROA,
	RTYPE_CER,
	RTYPE_CRL,
	RTYPE_GBR,
	RTYPE_ASPA,
	RTYPE_REPO,
	RTYPE_FILE,
	RTYPE_RSC,
};

enum location {
	DIR_UNKNOWN,
	DIR_TEMP,
	DIR_VALID,
};

/*
 * Files specified in an MFT have their bodies hashed with SHA256.
 */
struct mftfile {
	char		*file; /* filename (CER/ROA/CRL, no path) */
	enum rtype	 type; /* file type as determined by extension */
	enum location	 location;	/* temporary or valid directory */
	unsigned char	 hash[SHA256_DIGEST_LENGTH]; /* sha256 of body */
};

/*
 * A manifest, RFC 6486.
 * This consists of a bunch of files found in the same directory as the
 * manifest file.
 */
struct mft {
	char		*path; /* relative path to directory of the MFT */
	struct mftfile	*files; /* file and hash */
	char		*seqnum; /* manifestNumber */
	char		*aia; /* AIA */
	char		*aki; /* AKI */
	char		*ski; /* SKI */
	char		*crl; /* CRL file name */
	unsigned char	 crlhash[SHA256_DIGEST_LENGTH];
	time_t		 valid_since;
	time_t		 valid_until;
	size_t		 filesz; /* number of filenames */
	unsigned int	 repoid;
	int		 stale; /* if a stale manifest */
};

/*
 * An IP address prefix for a given ROA.
 * This encodes the maximum length, AFI (v6/v4), and address.
 * FIXME: are the min/max necessary or just used in one place?
 */
struct roa_ip {
	enum afi	 afi; /* AFI value */
	struct ip_addr	 addr; /* the address prefix itself */
	unsigned char	 min[16]; /* full range minimum */
	unsigned char	 max[16]; /* full range maximum */
	unsigned char	 maxlength; /* max length or zero */
};

/*
 * An ROA, RFC 6482.
 * This consists of the concerned ASID and its IP prefixes.
 */
struct roa {
	uint32_t	 asid; /* asID of ROA (if 0, RFC 6483 sec 4) */
	struct roa_ip	*ips; /* IP prefixes */
	size_t		 ipsz; /* number of IP prefixes */
	int		 talid; /* ROAs are covered by which TAL */
	int		 valid; /* validated resources */
	char		*aia; /* AIA */
	char		*aki; /* AKI */
	char		*ski; /* SKI */
	time_t		 expires; /* do not use after */
};

struct rscfile {
	char		*filename; /* an optional filename on the checklist */
	unsigned char	 hash[SHA256_DIGEST_LENGTH]; /* the digest */
};

/*
 * A Signed Checklist (RSC)
 */
struct rsc {
	int		 talid; /* RSC covered by what TAL */
	int		 valid; /* eContent resources covered by EE's 3779? */
	struct cert_ip	*ips; /* IP prefixes */
	size_t		 ipsz; /* number of IP prefixes */
	struct cert_as	*as; /* AS resources */
	size_t		 asz; /* number of AS resources */
	struct rscfile	*files; /* FileAndHashes in the RSC */
	size_t		 filesz; /* number of FileAndHashes */
	char		*aia; /* AIA */
	char		*aki; /* AKI */
	char		*ski; /* SKI */
	time_t		 expires; /* Not After of the RSC EE */
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
	int		talid; /* covered by which TAL */
	uint32_t	asid;
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
	int		 talid; /* covered by which TAL */
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
	time_t		 issued;	/* do not use before */
	time_t		 expires;	/* do not use after */
};
/*
 * Tree of CRLs sorted by uri
 */
RB_HEAD(crl_tree, crl);

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

struct auth	*auth_find(struct auth_tree *, const char *);
struct auth	*auth_insert(struct auth_tree *, struct cert *, struct auth *);

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
	RRDP_CLEAR,
	RRDP_END,
	RRDP_HTTP_REQ,
	RRDP_HTTP_INI,
	RRDP_HTTP_FIN,
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
	TAILQ_ENTRY(entity) entries;
	char		*path;		/* path relative to repository */
	char		*file;		/* filename or valid repo path */
	unsigned char	*data;		/* optional data blob */
	size_t		 datasz;	/* length of optional data blob */
	unsigned int	 repoid;	/* repository identifier */
	int		 talid;		/* tal identifier */
	enum rtype	 type;		/* type of entity (not RTYPE_EOF) */
	enum location	 location;	/* which directroy the file lives in */
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
	size_t	 certs_fail; /* invalid certificate */
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
	size_t	 extra_files; /* number of superfluous files */
	size_t	 del_dirs; /* number of directories removed in cleanup */
	size_t	 brks; /* number of BGPsec Router Key (BRK) certificates */
	size_t	 skiplistentries; /* number of skiplist entries */
	struct timeval	elapsed_time;
	struct timeval	user_time;
	struct timeval	system_time;
};

struct ibuf;
struct msgbuf;

/* global variables */
extern int verbose;
extern int filemode;
extern const char *tals[];
extern const char *taldescs[];
extern unsigned int talrepocnt[];
extern int talsz;

/* Routines for RPKI entities. */

void		 tal_buffer(struct ibuf *, const struct tal *);
void		 tal_free(struct tal *);
struct tal	*tal_parse(const char *, char *, size_t);
struct tal	*tal_read(struct ibuf *);

void		 cert_buffer(struct ibuf *, const struct cert *);
void		 cert_free(struct cert *);
struct cert	*cert_parse_pre(const char *, const unsigned char *, size_t);
struct cert	*cert_parse(const char *, struct cert *);
struct cert	*ta_parse(const char *, struct cert *, const unsigned char *,
		    size_t);
struct cert	*cert_read(struct ibuf *);
void		 cert_insert_brks(struct brk_tree *, struct cert *);

enum rtype	 rtype_from_file_extension(const char *);
void		 mft_buffer(struct ibuf *, const struct mft *);
void		 mft_free(struct mft *);
struct mft	*mft_parse(X509 **, const char *, const unsigned char *,
		    size_t);
struct mft	*mft_read(struct ibuf *);
int		 mft_compare(const struct mft *, const struct mft *);

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

void		 rsc_free(struct rsc *);
struct rsc	*rsc_parse(X509 **, const char *, const unsigned char *,
		    size_t);

/* crl.c */
struct crl	*crl_parse(const char *, const unsigned char *, size_t);
struct crl	*crl_get(struct crl_tree *, const struct auth *);
int		 crl_insert(struct crl_tree *, struct crl *);
void		 crl_free(struct crl *);

/* Validation of our objects. */

struct auth	*valid_ski_aki(const char *, struct auth_tree *,
		    const char *, const char *);
int		 valid_ta(const char *, struct auth_tree *,
		    const struct cert *);
int		 valid_cert(const char *, struct auth *, const struct cert *);
int		 valid_roa(const char *, struct auth *, struct roa *);
int		 valid_filehash(int, const char *, size_t);
int		 valid_hash(unsigned char *, size_t, const char *, size_t);
int		 valid_filename(const char *, size_t);
int		 valid_uri(const char *, size_t, const char *);
int		 valid_origin(const char *, const char *);
int		 valid_x509(char *, X509_STORE_CTX *, X509 *, struct auth *,
		    struct crl *, int);
int		 valid_rsc(const char *, struct auth *, struct rsc *);
int		 valid_econtent_version(const char *, const ASN1_INTEGER *);

/* Working with CMS. */
unsigned char	*cms_parse_validate(X509 **, const char *,
		    const unsigned char *, size_t,
		    const ASN1_OBJECT *, size_t *);

/* Work with RFC 3779 IP addresses, prefixes, ranges. */

int		 ip_addr_afi_parse(const char *, const ASN1_OCTET_STRING *,
			enum afi *);
int		 ip_addr_parse(const ASN1_BIT_STRING *,
			enum afi, const char *, struct ip_addr *);
void		 ip_addr_print(const struct ip_addr *, enum afi, char *,
			size_t);
int		 ip_addr_cmp(const struct ip_addr *, const struct ip_addr *);
int		 ip_addr_check_overlap(const struct cert_ip *,
			const char *, const struct cert_ip *, size_t);
int		 ip_addr_check_covered(enum afi, const unsigned char *,
			const unsigned char *, const struct cert_ip *, size_t);
int		 ip_cert_compose_ranges(struct cert_ip *);
void		 ip_roa_compose_ranges(struct roa_ip *);

int		 sbgp_addr(const char *, struct cert_ip *, size_t *,
		    enum afi, const ASN1_BIT_STRING *);
int		 sbgp_addr_range(const char *, struct cert_ip *, size_t *,
		    enum afi, const IPAddressRange *);

/* Work with RFC 3779 AS numbers, ranges. */

int		 as_id_parse(const ASN1_INTEGER *, uint32_t *);
int		 as_check_overlap(const struct cert_as *, const char *,
			const struct cert_as *, size_t);
int		 as_check_covered(uint32_t, uint32_t,
			const struct cert_as *, size_t);

int		 sbgp_as_id(const char *, struct cert_as *, size_t *,
		    const ASN1_INTEGER *);
int		 sbgp_as_range(const char *, struct cert_as *, size_t *,
		    const ASRange *);

/* Parser-specific */
void		 entity_free(struct entity *);
void		 entity_read_req(struct ibuf *, struct entity *);
void		 entityq_flush(struct entityq *, struct repo *);
void		 proc_parser(int) __attribute__((noreturn));
void		 proc_filemode(int) __attribute__((noreturn));

/* Rsync-specific. */

char		*rsync_base_uri(const char *);
void		 proc_rsync(char *, char *, int) __attribute__((noreturn));

/* HTTP and RRDP processes. */

void		 proc_http(char *, int) __attribute__((noreturn));
void		 proc_rrdp(int) __attribute__((noreturn));

/* Repository handling */
int		 filepath_add(struct filepath_tree *, char *);
void		 rrdp_clear(unsigned int);
void		 rrdp_save_state(unsigned int, struct rrdp_session *);
int		 rrdp_handle_file(unsigned int, enum publish_type, char *,
		    char *, size_t, char *, size_t);
char		*repo_basedir(const struct repo *, int);
unsigned int	 repo_id(const struct repo *);
const char	*repo_uri(const struct repo *);
struct repo	*ta_lookup(int, struct tal *);
struct repo	*repo_lookup(int, const char *, const char *);
struct repo	*repo_byid(unsigned int);
int		 repo_queued(struct repo *, struct entity *);
void		 repo_cleanup(struct filepath_tree *, int);
void		 repo_free(void);

void		 rsync_finish(unsigned int, int);
void		 http_finish(unsigned int, enum http_result, const char *);
void		 rrdp_finish(unsigned int, int);

void		 rsync_fetch(unsigned int, const char *, const char *,
		    const char *);
void		 http_fetch(unsigned int, const char *, const char *, int);
void		 rrdp_fetch(unsigned int, const char *, const char *,
		    struct rrdp_session *);
void		 rrdp_http_done(unsigned int, enum http_result, const char *);
int		 repo_check_timeout(int);

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
int		 hex_decode(const char *, char *, size_t);


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

void		 x509_init_oid(void);
int		 x509_get_aia(X509 *, const char *, char **);
int		 x509_get_aki(X509 *, const char *, char **);
int		 x509_get_ski(X509 *, const char *, char **);
int		 x509_get_expire(X509 *, const char *, time_t *);
int		 x509_get_crl(X509 *, const char *, char **);
char		*x509_crl_get_aki(X509_CRL *, const char *);
char		*x509_get_pubkey(X509 *, const char *);
enum cert_purpose	 x509_get_purpose(X509 *, const char *);
int		 x509_get_time(const ASN1_TIME *, time_t *);
char		*x509_convert_seqnum(const char *, const ASN1_INTEGER *);
int		 x509_location(const char *, const char *, const char *,
		    GENERAL_NAME *, char **);
int		 x509_inherits(X509 *);

/* printers */
char		*time2str(time_t);
void		 x509_print(const X509 *);
void		 tal_print(const struct tal *);
void		 cert_print(const struct cert *);
void		 crl_print(const struct crl *);
void		 mft_print(const X509 *, const struct mft *);
void		 roa_print(const X509 *, const struct roa *);
void		 gbr_print(const X509 *, const struct gbr *);
void		 rsc_print(const X509 *, const struct rsc *);

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
time_t		getmonotime(void);

int	mkpath(const char *);
int	mkpathat(int, const char *);

#define RPKI_PATH_OUT_DIR	"/var/db/rpki-client"
#define RPKI_PATH_BASE_DIR	"/var/cache/rpki-client"

#define DEFAULT_SKIPLIST_FILE	"/etc/rpki/skiplist"

/* Maximum number of TAL files we'll load. */
#define	TALSZ_MAX	8

/* Maximum number of IP and AS ranges accepted in any single file */
#define MAX_IP_SIZE		200000
#define MAX_AS_SIZE		200000

/* Maximum acceptable URI length */
#define MAX_URI_LENGTH		2048

/* Maximum acceptable file size */
#define MAX_FILE_SIZE		4000000

/* Maximum number of FileNameAndHash entries per RSC checklist. */
#define MAX_CHECKLIST_ENTRIES	100000

/* Maximum number of FileAndHash entries per manifest. */
#define MAX_MANIFEST_ENTRIES	100000

/* Maximum depth of the RPKI tree. */
#define MAX_CERT_DEPTH		12

/* Maximum number of concurrent http and rsync requests. */
#define MAX_HTTP_REQUESTS	64
#define MAX_RSYNC_REQUESTS	16

/* How many seconds to wait for a connection to succeed. */
#define MAX_CONN_TIMEOUT	15

/* How long to wait for IO from a remote server. */
#define MAX_IO_TIMEOUT		30

/* Maximum allowd repositories per tal */
#define MAX_REPO_PER_TAL	1000

/* Maximum number of delta files per RRDP notification file. */
#define MAX_RRDP_DELTAS		300

#endif /* ! EXTERN_H */
