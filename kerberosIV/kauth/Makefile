#	$OpenBSD: Makefile,v 1.3 1998/08/12 23:55:56 art Exp $

.include <bsd.obj.mk>

SRCS=	encdata.c rkinit.c kauth.c marshall.c
.PATH:	${.CURDIR}/../roken
SRCS+=	simple_exec.c
CFLAGS+= -I${.CURDIR}/../roken
PROG=	kauth
LDADD+=	-lkrb -lkafs -ldes
DPADD+=	${LIBKRB} ${LIBKAFS}
BINDIR=	/usr/bin
MAN=	kauth.1

.include <bsd.prog.mk>
