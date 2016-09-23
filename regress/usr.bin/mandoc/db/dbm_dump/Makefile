# $OpenBSD: Makefile,v 1.4 2016/09/23 15:08:10 bluhm Exp $

PROG =		dbm_dump
CPPFLAGS +=	-I${BSDSRCDIR}/usr.bin/mandoc
MOBJS =		dbm.o dbm_map.o
LDADD +=	${MOBJS}
CLEANFILES =	${MOBJS}

dbm_dump: ${MOBJS}

${MOBJS}:
	cd ${BSDSRCDIR}/usr.bin/mandoc && make $@
	ln -sf ${.OBJDIR}/../../../../../usr.bin/mandoc/$@ .

.include <bsd.prog.mk>
