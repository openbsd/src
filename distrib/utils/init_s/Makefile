# $NetBSD: Makefile,v 1.3 1995/10/13 18:35:17 gwr Exp $
# Build a "small init" (i.e. for boot media)

PROG=	init
NOMAN=	

# Need -lutil for: logout, logwtmp, login_tty
DPADD=	${LIBUTIL}
LDADD=	-lutil

SRCDIR= ${BSDSRCDIR}/sbin/init
CFLAGS+= -DLETS_GET_SMALL -I${SRCDIR}

all: ${PROG}

.include <bsd.prog.mk>

.PATH:  ${SRCDIR}
