/*	$OpenBSD: s_llround.c,v 1.2 2011/07/06 00:02:42 martynas Exp $	*/
/* $NetBSD: llround.c,v 1.2 2004/10/13 15:18:32 drochner Exp $ */

/*
 * Written by Matthias Drochner <drochner@NetBSD.org>.
 * Public domain.
 */

/* LINTLIBRARY */

#define LROUNDNAME llround
#define RESTYPE long long int
#define RESTYPE_MIN LLONG_MIN
#define RESTYPE_MAX LLONG_MAX

#include "s_lround.c"

#if	LDBL_MANT_DIG == 53
#ifdef	lint
/* PROTOLIB1 */
long long int llroundl(long double);
#else	/* lint */
__weak_alias(llroundl, llround);
#endif	/* lint */
#endif	/* LDBL_MANT_DIG == 53 */
