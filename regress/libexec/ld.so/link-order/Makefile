# $OpenBSD: Makefile,v 1.2 2003/07/01 07:08:31 niklas Exp $

.include <bsd.own.mk>

.if (${ELF_TOOLCHAIN} == "yes")
SUBDIR+= libnover
.endif
SUBDIR+= lib10 lib11 lib20 prog test

install:

.include <bsd.subdir.mk>
