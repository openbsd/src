/*	$NetBSD: inet.h,v 1.3 1994/10/27 04:21:14 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * Various UDP/IP definitions (see RFC 768, and 791)
 */

/* generic IP address */
#define	IP_ANYADDR	(ipaddr_t)0	/* IP any address */
#define	IP_BCASTADDR	(ipaddr_t)0xffffffff 	/* IP broadcast address */

/* some well defined protocols/ports */
#define	IP_PROTO_UDP	17		/* user datagram protocol */
#define	IP_PORT_TFTP	69		/* trivial FTP port */

/* internet address type */
typedef u_long ipaddr_t;

/* internet address type (only used for printing address) */
typedef union {
    ipaddr_t a;
    struct { u_char a0, a1, a2, a3; } s;
} inetaddr_t;

/*
 * Structure of an internet header (without options)
 */
typedef struct {
    u_char	ip_vhl;		/* version and header length */
#define	IP_VERSION	4	/* current version number */
    u_char	ip_tos;		/* type of service */
    u_short	ip_len;		/* total length */
    u_short	ip_id;		/* identification */
    u_short	ip_off;		/* fragment offset field */
    u_char	ip_ttl;		/* time to live */
#define	IP_FRAGTTL	60	/* time to live for frags */
    u_char	ip_p;		/* protocol */
    u_short	ip_sum;		/* checksum */
    ipaddr_t	ip_src;		/* source address */
    ipaddr_t	ip_dst;		/* destination address */
} iphdr_t;

/*
 * Structure of an UDP header
 */
typedef struct {
    u_short	uh_sport;	/* source port */
    u_short	uh_dport;	/* destination port */
    u_short	uh_len;		/* udp length */
    u_short	uh_sum;		/* udp checksum */
} udphdr_t;

/* these are actually defined in arp.c */
extern ipaddr_t ip_myaddr;	/* my own IP address */
extern ipaddr_t ip_gateway;	/* gateway's IP address */

void IpPrintAddr(ipaddr_t);
