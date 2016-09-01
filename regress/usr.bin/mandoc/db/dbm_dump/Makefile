# $OpenBSD: Makefile,v 1.2 2016/09/01 17:42:08 schwarze Exp $

PROG =		dbm_dump
CPPFLAGS +=	-I${BSDSRCDIR}/usr.bin/mandoc
MOBJS =		dbm.o dbm_map.o
LDADD +=	${MOBJS}
CLEANFILES =	${MOBJS}

regress: dbm_dump

dbm_dump: ${MOBJS}

${MOBJS}:
	@cp ${BSDOBJDIR}/usr.bin/mandoc/$@ . || \
		cp ${BSDSRCDIR}/usr.bin/mandoc/$@ .

.include <bsd.prog.mk>
