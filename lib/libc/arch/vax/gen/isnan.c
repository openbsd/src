/*	$OpenBSD: isnan.c,v 1.6 2011/07/02 19:27:34 martynas Exp $	*/
/*
 * Copyright (c) Martynas Venckus <martynas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* LINTLIBRARY */

#include <sys/cdefs.h>

/* ARGSUSED */
int
__isnan(double d)
{
	return(0);
}

/* ARGSUSED */
int
__isnanf(float f)
{
	return(0);
}

#ifdef	lint
/* PROTOLIB1 */
int __isnanl(long double);
#else	/* lint */
__weak_alias(__isnanl, __isnan);
#endif	/* lint */

/*
 * 3BSD compatibility aliases.
 */
__weak_alias(isnan, __isnan);
__weak_alias(isnanf, __isnanf);
