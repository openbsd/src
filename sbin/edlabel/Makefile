#	$OpenBSD: Makefile,v 1.6 2000/05/18 13:49:52 espie Exp $

.if ${MACHINE} == "sun3" || ${MACHINE} == "atari" || ${MACHINE} == "sparc"
PROG=	edlabel
CFLAGS+= -Wall
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

all: ${PROG}
.else
NOPROG=yes
.endif

NOMAN=
MANSUBDIR=sun3 sparc #atari

.include <bsd.prog.mk>
