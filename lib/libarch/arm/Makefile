#	$OpenBSD: Makefile,v 1.6 2009/09/24 19:53:47 miod Exp $

MAN+=	arm_sync_icache.2 arm_drain_writebuf.2
MANSUBDIR=armish zaurus

.if ${MACHINE_ARCH} == "arm"
NOPIC=
SRCS+=	arm_sync_icache.c arm_drain_writebuf.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif
