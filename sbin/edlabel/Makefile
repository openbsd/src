# $OpenBSD: Makefile,v 1.4 1997/03/26 00:34:44 deraadt Exp $
# $NetBSD: Makefile,v 1.1.1.1.6.1 1996/06/28 17:42:49 jtc Exp $
# edlabel (Edit Disk LABEL)

PROG=	edlabel
NOMAN=
CFLAGS+= -Wall
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

all: ${PROG}

.include <bsd.prog.mk>
