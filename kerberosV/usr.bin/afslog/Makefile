# $OpenBSD: Makefile,v 1.2 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG	= afslog

.PATH:	${KRB5SRC}/appl/afsutil

CFLAGS	+= -I.
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBCRYPTO} ${LIBASN1}
LDADD	+= -lkrb5 -ldes -lcrypto -lasn1 ${LIBROKEN} -lcom_err

.include <bsd.prog.mk>
