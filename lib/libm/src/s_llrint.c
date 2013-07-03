/*	$OpenBSD: s_llrint.c,v 1.5 2013/07/03 04:46:36 espie Exp $	*/
/* $NetBSD: llrint.c,v 1.2 2004/10/13 15:18:32 drochner Exp $ */

/*
 * Written by Matthias Drochner <drochner@NetBSD.org>.
 * Public domain.
 */

#define LRINTNAME llrint
#define RESTYPE long long int
#define RESTYPE_MIN LLONG_MIN
#define RESTYPE_MAX LLONG_MAX

#include "s_lrint.c"

#if	LDBL_MANT_DIG == DBL_MANT_DIG
__strong_alias(llrintl, llrint);
#endif	/* LDBL_MANT_DIG == DBL_MANT_DIG */
