#	$OpenBSD: Makefile,v 1.1 1999/12/09 15:10:49 itojun Exp $

PROG=	rtsol
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

CPPFLAGS+=-DINET6
LDADD+=	-lkvm
DPADD+=	${LIBKVM}

#MAN=	rtsold.8
NOMAN=	yes

.PATH:	${.CURDIR}/../../usr.sbin/rtsold

.include <bsd.prog.mk>
