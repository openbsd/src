/*	$OpenBSD: type.h,v 1.2 1997/03/12 10:41:57 downsj Exp $	*/

/*
** Various new resource record type and class values.
**
**	These might be missing in the default files on old platforms.
**	Also included are several definitions that might have been
**	omitted because they are obsolete, or are otherwise missing.
**
**	They belong in <arpa/nameser.h>
**
**	@(#)type.h              e07@nikhef.nl (Eric Wassenaar) 961010
*/

/* never used in practice */

#ifndef C_CSNET
#define C_CSNET		2
#endif

/* missing on some platforms */

#ifndef C_CHAOS
#define C_CHAOS		3
#endif

/* missing in some old versions */

#ifndef C_HS
#define C_HS		4
#endif

/* obsolete/deprecated types already missing on some platforms */

#ifndef T_MD
#define T_MD		3
#endif
#ifndef T_MF
#define T_MF		4
#endif
#ifndef T_MB
#define T_MB		7
#endif
#ifndef T_MG
#define T_MG		8
#endif
#ifndef T_MR
#define T_MR		9
#endif
#ifndef T_NULL
#define T_NULL		10
#endif
#ifndef T_MINFO
#define T_MINFO		14
#endif

/* missing in some old versions */

#ifndef T_TXT
#define T_TXT		16
#endif

/* defined per RFC 1183 */

#ifndef T_RP
#define T_RP		17
#endif
#ifndef T_AFSDB
#define T_AFSDB		18
#endif
#ifndef T_X25
#define T_X25		19
#endif
#ifndef T_ISDN
#define T_ISDN		20
#endif
#ifndef T_RT
#define T_RT		21
#endif

/* defined per RFC 1348, revised per RFC 1637 */

#ifndef T_NSAP
#define T_NSAP		22
#endif
#ifndef T_NSAPPTR
#define T_NSAPPTR	23
#endif

/* reserved per RFC 1700, defined per RFC XXXX */

#ifndef T_SIG
#define T_SIG		24
#endif
#ifndef T_KEY
#define T_KEY		25
#endif

/* defined per RFC 1664 */

#ifndef T_PX
#define T_PX		26
#endif

/* defined per RFC 1712, already withdrawn */

#ifndef T_GPOS
#define T_GPOS		27
#endif

/* reserved per RFC 1700, defined per RFC 1884 and 1886 */

#ifndef T_AAAA
#define T_AAAA		28
#endif

/* defined per RFC 1876 */

#ifndef T_LOC
#define T_LOC		29
#endif

/* defined per RFC XXXX */

#ifndef T_NXT
#define T_NXT		30
#endif

/* defined per RFC XXXX */

#ifndef T_EID
#define T_EID		31
#endif

/* defined per RFC XXXX */

#ifndef T_NIMLOC
#define T_NIMLOC	32
#endif

/* defined per RFC XXXX */

#ifndef T_SRV
#define T_SRV		33
#endif

/* defined per RFC XXXX */

#ifndef T_ATMA
#define T_ATMA		34
#endif

/* defined per RFC XXXX */

#ifndef T_NAPTR
#define T_NAPTR		35
#endif

/* nonstandard types are threatened to become extinct */

#ifndef T_UINFO
#define T_UINFO		100
#endif

#ifndef T_UID
#define T_UID		101
#endif

#ifndef T_GID
#define T_GID		102
#endif

#ifndef T_UNSPEC
#define T_UNSPEC	103
#endif

/* defined per RFC 1995 */

#ifndef T_IXFR
#define T_IXFR		251
#endif

/* really missing on some weird platforms, can you believe it */

#ifndef T_AXFR
#define T_AXFR		252
#endif

/* obsolete/deprecated types already missing on some platforms */

#ifndef T_MAILB
#define T_MAILB		253
#endif
#ifndef T_MAILA
#define T_MAILA		254
#endif
