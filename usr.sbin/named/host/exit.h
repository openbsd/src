/*	$OpenBSD: exit.h,v 1.2 1997/03/12 10:41:52 downsj Exp $	*/

/*
** Various exit codes.
**
**	They come from <sysexits.h>
**	Defined here to avoid including /usr/ucbinclude on solaris 2.x
**
**	@(#)exit.h              e07@nikhef.nl (Eric Wassenaar) 961010
*/

#undef  EX_OK			/* defined in <unistd.h> on SVR4 */
#define EX_SUCCESS	0	/* successful termination */
#define EX_USAGE	64	/* command line usage error */
#define EX_DATAERR	65	/* data format error */
#define EX_NOINPUT	66	/* cannot open input */
#define EX_NOUSER	67	/* addressee unknown */
#define EX_NOHOST	68	/* host name unknown */
#define EX_UNAVAILABLE	69	/* service unavailable */
#define EX_SOFTWARE	70	/* internal software error */
#define EX_OSERR	71	/* system error (e.g., can't fork) */
#define EX_OSFILE	72	/* critical OS file missing */
#define EX_CANTCREAT	73	/* can't create (user) output file */
#define EX_IOERR	74	/* input/output error */
#define EX_TEMPFAIL	75	/* temp failure; user is invited to retry */
#define EX_PROTOCOL	76	/* remote error in protocol */
#define EX_NOPERM	77	/* permission denied */
#define EX_CONFIG	78	/* local configuration error */
