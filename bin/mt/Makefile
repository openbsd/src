#	$OpenBSD: Makefile,v 1.5 1996/09/01 20:45:22 millert Exp $
#	$NetBSD: Makefile,v 1.5 1996/03/06 06:22:04 scottr Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	mt
SRCS=	mt.c mtrmt.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
BINOWN=	root
BINMODE=555

LINKS=	${BINDIR}/mt ${BINDIR}/eject
MLINKS=	mt.1 eject.1

.include <bsd.prog.mk>
