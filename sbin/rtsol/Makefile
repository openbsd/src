#	$OpenBSD: Makefile,v 1.6 2003/05/15 14:40:53 itojun Exp $

PROG=	rtsol
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

CPPFLAGS+=-DINET6 -DHAVE_ARC4RANDOM -DHAVE_POLL_H

#MAN=	rtsold.8
NOMAN=	yes

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.PATH:	${.CURDIR}/../../usr.sbin/rtsold

.include <bsd.prog.mk>
