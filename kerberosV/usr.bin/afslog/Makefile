# $OpenBSD: Makefile,v 1.1 2002/06/11 15:26:49 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG	= afslog

.PATH:	${KRB5SRC}/appl/afsutil

CFLAGS	+= -I.
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBCRYPTO} ${LIBASN1}
LDADD	+= -lkafs -lkrb5 -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN} -lcom_err

.include <bsd.prog.mk>
