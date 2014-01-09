#	$OpenBSD: Makefile,v 1.1 2014/01/09 15:02:52 deraadt Exp $

.PATH: ${.CURDIR}/../../../usr.sbin/ztsscale

.if ${MACHINE} == "zaurus"
SRCS=	ztsscale.c

PROG=	ztsscale
.else
NOPROG=yes
.endif

.include <bsd.prog.mk>
