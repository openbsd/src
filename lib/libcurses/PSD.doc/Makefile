#	@(#)Makefile	8.2 (Berkeley) 5/23/94

DIR=	psd/19.curses
SRCS=	Master
MACROS=	-me
EXTRA=	appen.A appen.B appen.C c_macros doc.I doc.II doc.III \
	doc.IV ex1.c ex2.c fns.doc intro.0 intro.1 intro.2 intro.3 \
	intro.4 intro.5 intro.6 life.c macros twinkle1.c twinkle2.c \
	win_st.c

CLEANFILES+=win_st.gr twinkle1.gr twinkle2.gr life.gr intro.2.tbl appen.A.tbl \
	ex1.gr ex2.gr

.SUFFIXES:
.SUFFIXES: .c .gr

#
# this section formats C input source into nice troffable (or nroffable)
# versions.  It uses the capabilites of "vgrind", which sets keywords in
# bold font, and comments in italics.
#

# Don't re-run vgrind unless you want to patch the output files.
VFONT=	/usr/libexec/vfontedpr
.c.gr:
	${VFONT} $*.c | grep -v "^'wh" > $*.gr

paper.ps: ${SRCS}
	soelim ${SRCS} | ${ROFF} > ${.TARGET}

Master: twinkle1.gr ex1.gr ex2.gr fns.doc intro.5 intro.2.tbl intro.0 intro.1 \
	intro.3 intro.4 intro.6 macros c_macros

intro.2.tbl: intro.2
	${TBL} intro.2 > intro.2.tbl

.include <bsd.doc.mk>


