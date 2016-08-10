#	$OpenBSD: Makefile,v 1.7 2016/08/10 14:27:16 deraadt Exp $

MAN+=	arm_sync_icache.2 arm_drain_writebuf.2
MANSUBDIR=zaurus

.if ${MACHINE_ARCH} == "arm"
NOPIC=
SRCS+=	arm_sync_icache.c arm_drain_writebuf.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif
