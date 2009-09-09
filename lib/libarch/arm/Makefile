#	$OpenBSD: Makefile,v 1.5 2009/09/09 12:10:34 marex Exp $

MAN+=	arm_sync_icache.2 arm_drain_writebuf.2
MANSUBDIR=armish zaurus

.if ${MACHINE} == "armish" || ${MACHINE} == "zaurus" || ${MACHINE} == "palm"
NOPIC=
SRCS+=	arm_sync_icache.c arm_drain_writebuf.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif
