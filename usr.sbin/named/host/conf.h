/*	$OpenBSD: conf.h,v 1.2 1997/03/12 10:41:51 downsj Exp $	*/

/*
** Various configuration definitions.
**
**	@(#)conf.h              e07@nikhef.nl (Eric Wassenaar) 961013
*/

/*
 * A special version of res_send() is included, which returns additional
 * errno statuses, and which corrects some flaws in the BIND 4.8 version.
 */

#if !defined(HOST_RES_SEND) && !defined(BIND_RES_SEND)

#if defined(BIND_49)
#define BIND_RES_SEND		/* use the default BIND res_send() */
#else
#define HOST_RES_SEND		/* use the special host res_send() */
#endif

#endif

/*
 * The root domain for the internet reversed mapping zones.
 */

#define ARPA_ROOT	"in-addr.arpa"

/*
 * The root domain for the IP v6 reversed mapping zones as per RFC 1886.
 */

#ifndef IPNG_ROOT
#define IPNG_ROOT	"ip6.int"
#endif

/*
 * The root domain for the NSAP reversed mapping zones as per RFC 1637.
 */

#ifndef NSAP_ROOT
#define NSAP_ROOT	"nsap.int"
#endif

/*
 * An encoded NSAP address is 7 to 20 octets as per RFC 1629.
 */

#define MAXNSAP		20	/* maximum size of encoded NSAP address */

/*
 * Version number of T_LOC resource record.
 */

#define T_LOC_VERSION	0	/* must be zero */

/*
 * Various constants related to MD5 keys and signatures.
 */

#define	MAXMD5BITS	2552
#define	MAXMD5SIZE	(2*((MAXMD5BITS+7)/8)+3)
#define	MAXB64SIZE	(4*((MAXMD5SIZE+2)/3))

/*
 * The standard nameserver port.
 */

#ifndef NAMESERVER_PORT
#define NAMESERVER_PORT	53
#endif

/*
 * Miscellaneous constants.
 */

#define MAXCHAIN	10	/* maximum count of recursive chain lookups */
#define MAXALIAS	35	/* maximum aliases count from gethnamaddr.c */
#define MAXADDRS	35	/* maximum address count from gethnamaddr.c */
#define MAXNSNAME	16	/* maximum count of nameservers per zone */
#define MAXIPADDR	10	/* maximum count of addresses per nameserver */

/*
 * Default timeout values.
 */

#define DEF_RETRIES	2	/* number of datagram retries per nameserver */
#define DEF_RETRANS	5	/* timeout (seconds) between datagram retries */
#define CONNTIMEOUT	5	/* connect timeout (value _res.retrans used) */
#define READTIMEOUT	60	/* read timeout (seconds) during stream I/O */

/*
 * Prefix for messages on stdout in debug mode.
 */

#if defined(BIND_49)
#define DBPREFIX	";; "
#else
#define DBPREFIX	""
#endif
