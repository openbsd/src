/*	$OpenBSD: s_llround.c,v 1.5 2013/07/03 04:46:36 espie Exp $	*/
/* $NetBSD: llround.c,v 1.2 2004/10/13 15:18:32 drochner Exp $ */

/*
 * Written by Matthias Drochner <drochner@NetBSD.org>.
 * Public domain.
 */

#define LROUNDNAME llround
#define RESTYPE long long int
#define RESTYPE_MIN LLONG_MIN
#define RESTYPE_MAX LLONG_MAX

#include "s_lround.c"

#if	LDBL_MANT_DIG == DBL_MANT_DIG
__strong_alias(llroundl, llround);
#endif	/* LDBL_MANT_DIG == DBL_MANT_DIG */
