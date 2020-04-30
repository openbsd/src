/*	$OpenBSD: extern.h,v 1.29 2020/04/30 13:46:39 deraadt Exp $ */
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

#include <sys/tree.h>
#include <sys/time.h>

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
	char		*mft; /* manifest (rsync:// uri) */
	char		*crl; /* CRL location (rsync:// or NULL) */
	char		*aki; /* AKI (or NULL, for trust anchor) */
	char		*ski; /* SKI */
	int		 valid; /* validated resources */
	X509		*x509; /* the cert */
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
	char		*ski; /* SKI */
	char		*aki; /* AKI */
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
	char		*ski; /* SKI */
	char		*aki; /* AKI */
	char		*tal; /* basename of TAL for this cert */
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
};
/*
 * Tree of VRP sorted by afi, addr, maxlength and asid
 */
RB_HEAD(vrp_tree, vrp);
RB_PROTOTYPE(vrp_tree, vrp, entry, vrpcmp);

/*
 * A single CRL
 */
struct crl {
	RB_ENTRY(crl)	 entry;
	char		*aki;
	X509_CRL	*x509_crl;
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
	char		*tal; /* basename of TAL for this cert */
	char		*fn; /* FIXME: debugging */
};
/*
 * Tree of auth sorted by ski
 */
RB_HEAD(auth_tree, auth);
RB_PROTOTYPE(auth_tree, auth, entry, authcmp);

struct auth *auth_find(struct auth_tree *, const char *);

/*
 * Resource types specified by the RPKI profiles.
 * There are others (e.g., gbr) that we don't consider.
 */
enum rtype {
	RTYPE_EOF = 0,
	RTYPE_TAL,
	RTYPE_MFT,
	RTYPE_ROA,
	RTYPE_CER,
	RTYPE_CRL
};

/*
 * Statistics collected during run-time.
 */
struct	stats {
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
	size_t	 crls; /* revocation lists */
	size_t	 vrps; /* total number of vrps */
	size_t	 uniqs; /* number of unique vrps */
	char	*talnames;
	struct timeval	elapsed_time;
	struct timeval	user_time;
	struct timeval	system_time;
};

/* global variables */
extern int verbose;

/* Routines for RPKI entities. */

void		 tal_buffer(char **, size_t *, size_t *, const struct tal *);
void		 tal_free(struct tal *);
struct tal	*tal_parse(const char *, char *);
char		*tal_read_file(const char *);
struct tal	*tal_read(int);

void		 cert_buffer(char **, size_t *, size_t *, const struct cert *);
void		 cert_free(struct cert *);
struct cert	*cert_parse(X509 **, const char *, const unsigned char *);
struct cert	*ta_parse(X509 **, const char *, const unsigned char *, size_t);
struct cert	*cert_read(int);

void		 mft_buffer(char **, size_t *, size_t *, const struct mft *);
void		 mft_free(struct mft *);
struct mft	*mft_parse(X509 **, const char *, int);
int		 mft_check(const char *, struct mft *);
struct mft	*mft_read(int);

void		 roa_buffer(char **, size_t *, size_t *, const struct roa *);
void		 roa_free(struct roa *);
struct roa	*roa_parse(X509 **, const char *, const unsigned char *);
struct roa	*roa_read(int);
void		 roa_insert_vrps(struct vrp_tree *, struct roa *, size_t *,
		    size_t *);

/* crl.c */
X509_CRL	*crl_parse(const char *, const unsigned char *);
void		 free_crl(struct crl *);

/* Validation of our objects. */

struct auth	*valid_ski_aki(const char *, struct auth_tree *,
		    const char *, const char *);
int		 valid_ta(const char *, struct auth_tree *,
		    const struct cert *);
int		 valid_cert(const char *, struct auth_tree *,
		    const struct cert *);
int		 valid_roa(const char *, struct auth_tree *, struct roa *);

/* Working with CMS files. */

unsigned char	*cms_parse_validate(X509 **, const char *,
			const char *, const unsigned char *, size_t *);

/* Work with RFC 3779 IP addresses, prefixes, ranges. */

int		 ip_addr_afi_parse(const char *, const ASN1_OCTET_STRING *,
			enum afi *);
int		 ip_addr_parse(const ASN1_BIT_STRING *,
			enum afi, const char *, struct ip_addr *);
void		 ip_addr_print(const struct ip_addr *, enum afi, char *,
			size_t);
void		 ip_addr_buffer(char **, size_t *, size_t *,
			const struct ip_addr *);
void		 ip_addr_range_buffer(char **, size_t *, size_t *,
			const struct ip_addr_range *);
void		 ip_addr_read(int, struct ip_addr *);
void		 ip_addr_range_read(int, struct ip_addr_range *);
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

/* Rsync-specific. */

int		 rsync_uri_parse(const char **, size_t *,
			const char **, size_t *, const char **, size_t *,
			enum rtype *, const char *);

/* Logging (though really used for OpenSSL errors). */

void		 cryptowarnx(const char *, ...)
			__attribute__((format(printf, 1, 2)));
void		 cryptoerrx(const char *, ...)
			__attribute__((format(printf, 1, 2)))
			__attribute__((noreturn));

/* Functions for moving data between processes. */

void		 io_socket_blocking(int);
void		 io_socket_nonblocking(int);
void		 io_simple_buffer(char **, size_t *, size_t *, const void *,
			size_t);
void		 io_simple_read(int, void *, size_t);
void		 io_simple_write(int, const void *, size_t);
void		 io_buf_buffer(char **, size_t *, size_t *, const void *,
			size_t);
void		 io_buf_read_alloc(int, void **, size_t *);
void		 io_buf_write(int, const void *, size_t);
void		 io_str_buffer(char **, size_t *, size_t *, const char *);
void		 io_str_read(int, char **);
void		 io_str_write(int, const char *);

/* X509 helpers. */

char		*x509_get_aki_ext(X509_EXTENSION *, const char *);
char		*x509_get_ski_ext(X509_EXTENSION *, const char *);
int		 x509_get_ski_aki(X509 *, const char *, char **, char **);
char		*x509_get_crl(X509 *, const char *);
char		*x509_crl_get_aki(X509_CRL *);

/* Output! */

extern int	 outformats;
#define FORMAT_OPENBGPD	0x01
#define FORMAT_BIRD	0x02
#define FORMAT_CSV	0x04
#define FORMAT_JSON	0x08
extern char*	 outputdir;

int		 outputfiles(struct vrp_tree *v, struct stats *);
int		 outputheader(FILE *, struct stats *);
int		 output_bgpd(FILE *, struct vrp_tree *, struct stats *);
int		 output_bird1v4(FILE *, struct vrp_tree *, struct stats *);
int		 output_bird1v6(FILE *, struct vrp_tree *, struct stats *);
int		 output_bird2(FILE *, struct vrp_tree *, struct stats *);
int		 output_csv(FILE *, struct vrp_tree *, struct stats *);
int		 output_json(FILE *, struct vrp_tree *, struct stats *);

void	logx(const char *fmt, ...)
		    __attribute__((format(printf, 1, 2)));

#define		RPKI_PATH_OUT_DIR	"/var/db/rpki-client"
#define		RPKI_PATH_BASE_DIR	"/var/cache/rpki-client"

#endif /* ! EXTERN_H */
