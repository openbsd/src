#	$OpenBSD: Makefile.inc,v 1.3 2006/11/07 17:38:49 drahn Exp $

CPPFLAGS += -DSOFTFLOAT

SOFTFLOAT_BITS=32
.include <arch/sh/softfloat/Makefile.inc>
