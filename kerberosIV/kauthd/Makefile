#	$OpenBSD: Makefile,v 1.2 1998/02/26 00:16:47 art Exp $

.include <bsd.obj.mk>

SRCS= kauthd.c inaddr2str.c mini_inetd.c
.PATH: ${.CURDIR}/../kauth
SRCS+= encdata.c marshall.c
PROG= kauthd
LDADD+= -lkrb -lkafs -ldes
DPADD+=	${LIBKRB} ${LIBKAFS}
CFLAGS+= -I${.CURDIR}/../kauth
BINDIR=/usr/libexec
MAN= kauthd.8

.include <bsd.prog.mk>
