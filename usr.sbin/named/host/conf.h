/*
** Various configuration definitions.
**
**	@(#)conf.h              e07@nikhef.nl (Eric Wassenaar) 951230
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
 * Miscellaneous constants.
 */

#define MAXADDRS	35	/* max address count from gethostnamadr.c */
#define MAXNSNAME	16	/* maximum count of nameservers per zone */
#define MAXIPADDR	10	/* maximum count of addresses per nameserver */
#define MAXHOSTS	65536	/* maximum count of hostnames per zone */

/*
 * Version number of T_LOC resource record.
 */

#define T_LOC_VERSION	0	/* must be zero */

/*
 * Prefix for messages on stdout in debug mode.
 */

#if defined(BIND_49)
#define DBPREFIX	";; "
#else
#define DBPREFIX	""
#endif
