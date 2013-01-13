/*	$OpenBSD: s_llrint.c,v 1.3 2013/01/13 03:45:00 martynas Exp $	*/
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

#if	LDBL_MANT_DIG == 53
__weak_alias(llrintl, llrint);
#endif	/* LDBL_MANT_DIG == 53 */
