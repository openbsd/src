#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:19:42 deraadt Exp $
#	$NetBSD: Makefile,v 1.7 1995/03/21 09:03:47 cgd Exp $

PROG=	date
SRCS=	date.c netdate.c
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.prog.mk>
