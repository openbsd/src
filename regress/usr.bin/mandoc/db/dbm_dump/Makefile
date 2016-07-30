# $OpenBSD: Makefile,v 1.1 2016/07/30 10:56:13 schwarze Exp $

PROG =		dbm_dump
CPPFLAGS +=	-I${BSDSRCDIR}/usr.bin/mandoc
LDADD +=	${BSDOBJDIR}/usr.bin/mandoc/dbm.o
LDADD +=	${BSDOBJDIR}/usr.bin/mandoc/dbm_map.o

.include <bsd.prog.mk>
