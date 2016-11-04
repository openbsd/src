# $OpenBSD: Makefile,v 1.1 2016/11/04 19:57:27 schwarze Exp $

PROG =		mlinks
CPPFLAGS +=	-I${BSDSRCDIR}/usr.bin/mandoc
MOBJS =		dbm.o dbm_map.o
LDADD +=	${MOBJS}
CLEANFILES =	${MOBJS}
NOMAN=

mlinks: ${MOBJS}

${MOBJS}:
	cd ${BSDSRCDIR}/usr.bin/mandoc && make $@
	ln -sf ${.OBJDIR}/../../../../../usr.bin/mandoc/$@ .

.include <bsd.prog.mk>
