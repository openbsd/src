#	$OpenBSD: Makefile,v 1.1 1997/12/18 22:28:02 art Exp $

.include <bsd.obj.mk>

SRCS= encdata.c rkinit.c kauth.c marshall.c
PROG= kauth
LDADD+= -lkrb -lkafs -ldes
DPADD+=	${LIBKRB} ${LIBKAFS}
MAN=

.include <bsd.prog.mk>
