#	$OpenBSD: Makefile,v 1.5 2002/10/26 20:23:20 itojun Exp $

PROG=	rtsol
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

CPPFLAGS+=-DINET6 -DHAVE_ARC4RANDOM -DHAVE_GETIFADDRS -DHAVE_POLL_H

#MAN=	rtsold.8
NOMAN=	yes

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.PATH:	${.CURDIR}/../../usr.sbin/rtsold

.include <bsd.prog.mk>
