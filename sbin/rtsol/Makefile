#	$OpenBSD: Makefile,v 1.8 2004/01/02 23:52:17 itojun Exp $

PROG=	rtsol
SRCS=	rtsold.c rtsol.c if.c probe.c

CPPFLAGS+=-DSMALL

#MAN=	rtsold.8
NOMAN=	yes

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.PATH:	${.CURDIR}/../../usr.sbin/rtsold

.include <bsd.prog.mk>
