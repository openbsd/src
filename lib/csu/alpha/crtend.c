/*	$OpenBSD: crtend.c,v 1.1 1996/11/13 21:28:04 niklas Exp $	*/
/*	$NetBSD: crtend.c,v 1.1 1996/09/12 16:59:04 cgd Exp $	*/

#ifndef ECOFF_COMPAT

#include <sys/cdefs.h>

static void (*__CTOR_LIST__[1]) __P((void))
    __attribute__((section(".ctors"))) = { (void *)0 };		/* XXX */
static void (*__DTOR_LIST__[1]) __P((void))
    __attribute__((section(".dtors"))) = { (void *)0 };		/* XXX */

#endif /* !ECOFF_COMPAT */
