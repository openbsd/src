# $OpenBSD: Makefile,v 1.4 2010/10/13 08:14:22 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kf

SRCS=	kf.c

.PATH:	${KRB5SRC}/appl/kf

DPADD+= ${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
