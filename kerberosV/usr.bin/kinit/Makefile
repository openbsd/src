# $OpenBSD: Makefile,v 1.4 2001/08/30 10:33:59 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit
MLINKS=	kinit.1 kauth.1

SRCS=	kinit.c kinit_options.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb -lkafs ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err

.include <bsd.prog.mk>