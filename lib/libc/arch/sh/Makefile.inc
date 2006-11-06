#	$OpenBSD: Makefile.inc,v 1.2 2006/11/06 15:14:50 drahn Exp $

CPPFLAGS += -DSOFTFLOAT

SOFTFLOAT_BITS=32
.include <arch/arm/softfloat/Makefile.inc>
