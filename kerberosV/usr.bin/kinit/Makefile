# $OpenBSD: Makefile,v 1.6 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit
MLINKS=	kinit.1 kauth.1

.PATH:	${KRB5SRC}/kuser

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIB45} ${LIBKRB} ${LIBKAFS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb -lkafs ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err

.include <bsd.prog.mk>
