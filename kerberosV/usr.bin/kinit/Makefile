# $OpenBSD: Makefile,v 1.2 2001/06/22 16:12:18 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit

SRCS=	kinit.c kinit_options.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb -lkafs ${LIBROKEN} -lasn1 -lcrypto

.include <bsd.prog.mk>