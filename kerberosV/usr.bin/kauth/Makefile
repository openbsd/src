# $OpenBSD: Makefile,v 1.1 2001/05/25 08:20:33 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kauth
NOMAN=

SRCS=	kinit.c kauth_options.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	${LIBROKEN} -lkrb5 -ldes -lkafs -l45 -lkrb -lkafs5

.include <bsd.prog.mk>