#	$OpenBSD: Makefile,v 1.6 2001/06/27 06:16:46 art Exp $

PROG =	rpc.rstatd
.PATH: ${.CURDIR}/../../usr.bin/vmstat

CFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat
SRCS =	dkstats.c rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8
MLINKS=	rpc.rstatd.8 rstatd.8

DPADD=	${LIBRPCSVC} ${LIBKVM}
LDADD=	-lrpcsvc -lkvm

.include <bsd.prog.mk>

