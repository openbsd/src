#	$OpenBSD: Makefile,v 1.2 1996/06/02 16:50:01 mickey Exp $

LKM=	ipl
SRCS+=	stub.c ip_fil.c fil.c ip_nat.c ip_frag.c ip_state.c
CFLAGS+= -DIPFILTER_LKM
NOMAN=	noman

.PATH: ${.CURDIR}/../../sys/netinet

.include <bsd.lkm.mk>
