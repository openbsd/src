/*	$NetBSD: bsd.h,v 1.2 1998/01/09 08:03:40 perry Exp $	*/
/*	$OpenBSD: bsd.h,v 1.2 1999/01/21 05:47:39 d Exp $	*/

/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

# if defined(BSD_RELEASE) && BSD_RELEASE >= 43
# define	BROADCAST
# define	SYSLOG_43
# define	TALK_43
# endif
# if defined(BSD_RELEASE) && BSD_RELEASE == 42
# define	SYSLOG_42
# define	TALK_42
# endif
