#	$OpenBSD: Makefile,v 1.1 1996/02/25 19:20:26 mickey Exp $

LKM=	ipl
SRCS+=	stub.c ip_fil.c fil.c ip_nat.c ip_frag.c ip_state.c
CFLAGS+= -DIPFILTER_LKM -DIPFILTER_LOG
NOMAN=	noman

.PATH: ${.CURDIR}/../../sys/netinet

.include <bsd.lkm.mk>
