# $OpenBSD: Makefile,v 1.3 2000/03/01 22:10:11 todd Exp $
# $NetBSD: Makefile,v 1.4 1996/03/27 04:48:40 scottr Exp $
# Build a "small init" (i.e. for boot media)

PROG=	init
NOMAN=	

# Need -lutil for: logout, logwtmp, login_tty
DPADD=	${LIBUTIL}
LDADD=	-lutil

SRCDIR= ${.CURDIR}/../../../sbin/init
CFLAGS+= -DLETS_GET_SMALL -I${SRCDIR}

all: ${PROG}

.include <bsd.prog.mk>

.PATH:  ${SRCDIR}
