#	$OpenBSD: Makefile,v 1.2 2007/10/14 20:29:27 otto Exp $
#
# Makefile for the ccom part of pcc.
#

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= x86
.else
SUBDIR+= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
