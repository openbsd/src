#	$OpenBSD: Makefile,v 1.4 2001/12/01 23:27:23 miod Exp $

PROG=	rtsol
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

CPPFLAGS+=-DINET6 -DHAVE_ARC4RANDOM -DHAVE_GETIFADDRS

#MAN=	rtsold.8
NOMAN=	yes

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.PATH:	${.CURDIR}/../../usr.sbin/rtsold

.include <bsd.prog.mk>
