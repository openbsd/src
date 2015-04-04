/*	$OpenBSD: crtendS.c,v 1.9 2015/04/04 18:05:05 guenther Exp $	*/
/*	$NetBSD: crtend.c,v 1.1 1997/04/16 19:38:24 thorpej Exp $	*/

#include <sys/types.h>
#include "md_init.h"
#include "extern.h"

static init_f __CTOR_LIST__[1]
    __used __attribute__((section(".ctors"))) = { (void *)0 };	/* XXX */
static init_f __DTOR_LIST__[1]
    __used __attribute__((section(".dtors"))) = { (void *)0 };	/* XXX */

static void * __JCR_END__[]
    __used __attribute__((section(".jcr"), aligned(sizeof(void*)))) = { 0 };

MD_SECTION_EPILOGUE(".init");
MD_SECTION_EPILOGUE(".fini");
