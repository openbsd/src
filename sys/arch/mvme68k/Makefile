#	$OpenBSD: Makefile,v 1.9 2002/07/15 21:29:16 mickey Exp $

# from: @(#)Makefile	8.1 (Berkeley) 6/11/93
#
# Makefile for mvme68k links, tags file

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman
NOOBJ=	noobj

TMVEM68K= $S/arch/mvme68k/tags
SUBDIR=	stand ../m68k
DIRS=	conf dev include mvme68k

links::
	-for i in ${DIRS}; do \
		echo $$i ; \
		cd ${.CURDIR} ; \
	    (cd $$i && { rm -f tags; ln -s ${SYSTAGS} tags; }) done

SMVME68K=$S/arch/mvme68k/dev/*.[ch] $S/arch/mvme68k/include/*.[ch] \
	$S/arch/mvme68k/mvme68k/*.[ch]
AMVME68K=$S/arch/mvme68k/mvme68k/*.s $S/lib/libkern/arch/m68k/*.[sS]

tags:
	-ctags -wdtf ${TMVEM68K} ${COMM} ${SMVME68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMVME68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> tags
	sort -o ${TMVEM68K} ${TMVEM68K}

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
