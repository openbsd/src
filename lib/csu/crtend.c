/*	$OpenBSD: crtend.c,v 1.1 2001/02/03 22:47:01 art Exp $	*/
/*	$NetBSD: crtend.c,v 1.1 1996/09/12 16:59:04 cgd Exp $	*/

#ifndef ECOFF_COMPAT

#include <sys/cdefs.h>

static void (*__CTOR_LIST__[1]) __P((void))
    __attribute__((section(".ctors"))) = { (void *)0 };		/* XXX */
static void (*__DTOR_LIST__[1]) __P((void))
    __attribute__((section(".dtors"))) = { (void *)0 };		/* XXX */

#endif /* !ECOFF_COMPAT */
