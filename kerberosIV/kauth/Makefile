#	$OpenBSD: Makefile,v 1.2 1998/01/23 08:29:58 art Exp $

.include <bsd.obj.mk>

SRCS= encdata.c rkinit.c kauth.c marshall.c
PROG= kauth
LDADD+= -lkrb -lkafs -ldes
DPADD+=	${LIBKRB} ${LIBKAFS}
BINDIR=/usr/bin
MAN= kauth.1

.include <bsd.prog.mk>
