/*	$OpenBSD: crtendS.c,v 1.7 2010/05/01 11:32:43 kettenis Exp $	*/
/*	$NetBSD: crtend.c,v 1.1 1997/04/16 19:38:24 thorpej Exp $	*/

#include <sys/cdefs.h>
#include "md_init.h"
#include "extern.h"

static init_f __CTOR_LIST__[1]
    __used __attribute__((section(".ctors"))) = { (void *)0 };	/* XXX */
static init_f __DTOR_LIST__[1]
    __used __attribute__((section(".dtors"))) = { (void *)0 };	/* XXX */

#if (__GNUC__ > 2)
static void * __JCR_END__[]
    __used __attribute__((section(".jcr"), aligned(sizeof(void*)))) = { 0 };
#endif

MD_SECTION_EPILOGUE(".init");
MD_SECTION_EPILOGUE(".fini");
