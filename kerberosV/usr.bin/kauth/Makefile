# $OpenBSD: Makefile,v 1.2 2001/06/22 16:12:17 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kauth
NOMAN=

SRCS=	kinit.c kauth_options.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb ${LIBROKEN} -lasn1 -lcrypto

.include <bsd.prog.mk>