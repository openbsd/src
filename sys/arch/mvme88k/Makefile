#	$OpenBSD: Makefile,v 1.5 2002/07/15 21:29:16 mickey Exp $

# from: @(#)Makefile	8.1 (Berkeley) 6/11/93
#
# Makefile for mvme88k links, tags file

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman
NOOBJ=	noobj

TMVME88K=${.CURDIR}/tags
SUBDIR=	stand 
DIRS=	conf dev include mvme88k

links::
	-for i in ${DIRS}; do \
		echo $$i ; \
		cd ${.CURDIR} ; \
	    (cd $$i && { rm -f tags; ln -s ${TMVME88K} tags; }) done

SMVME88K=$S/arch/mvme88k/dev/*.[ch] $S/arch/mvme88k/include/*.[ch] \
	$S/arch/mvme88k/mvme88k/*.[ch]
AMVME88K=$S/arch/mvme88k/mvme88k/*.[sS]

tags:
	-ctags -wdtf ${TMVME88K} ${COMM} ${SMVME88K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMVME88K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TMVME88K}
	sort -o ${TMVME88K} ${TMVME88K}

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
