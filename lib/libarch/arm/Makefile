#	$OpenBSD: Makefile,v 1.2 2005/03/15 20:12:20 drahn Exp $

MAN+=	arm_sync_icache.2 arm_drain_writebuf.2
MANSUBDIR=cats zaurus

.if ${MACHINE} == "cats" || ${MACHINE} == "zaurus"
NOPIC=
SRCS+=	arm_sync_icache.c arm_drain_writebuf.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif
