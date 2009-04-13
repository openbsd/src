/*	$OpenBSD: crtend.c,v 1.7 2009/04/13 20:15:24 kurt Exp $	*/
/*	$NetBSD: crtend.c,v 1.1 1996/09/12 16:59:04 cgd Exp $	*/

#include <sys/cdefs.h>
#include "md_init.h"
#include "extern.h"

static init_f __CTOR_LIST__[1]
    __attribute__((section(".ctors"))) = { (void *)0 };		/* XXX */
static init_f __DTOR_LIST__[1]
    __attribute__((section(".dtors"))) = { (void *)0 };		/* XXX */

static const int __EH_FRAME_END__[]
__attribute__((unused, mode(SI), section(".eh_frame"), aligned(4))) = { 0 };

#if (__GNUC__ > 2)
static void * __JCR_END__[]
__attribute__((unused, section(".jcr"), aligned(sizeof(void*)))) = { 0 };
#endif

MD_SECTION_EPILOGUE(".init");
MD_SECTION_EPILOGUE(".fini");
