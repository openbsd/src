#	$OpenBSD: Makefile,v 1.7 2004/01/02 23:46:04 itojun Exp $

PROG=	rtsol
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

#MAN=	rtsold.8
NOMAN=	yes

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.PATH:	${.CURDIR}/../../usr.sbin/rtsold

.include <bsd.prog.mk>
