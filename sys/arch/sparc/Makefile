#	$OpenBSD: Makefile,v 1.8 2002/07/15 21:29:16 mickey Exp $
#	$NetBSD: Makefile,v 1.4 1996/03/25 22:48:09 mrg Exp $

#
# Makefile for sparc links, tags file

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR= stand

NOPROG=	noprog
NOMAN=	noman

TSPARC=	$S/arch/sparc/tags
DIRS=	conf dev fpu include rcons sbus sparc

links::
	-for i in ${DIRS}; do \
	    (cd $$i && { rm -f tags; ln -s ${TSPARC} tags; }) done

SPARC=	$S/arch/sparc/dev/*.[ch] $S/arch/sparc/fpu/*.[ch] \
	$S/arch/sparc/include/*.[ch] $S/arch/sparc/sparc/*.[ch]
ASPARC=	$S/arch/sparc/sparc/*.[sS] $S/lib/libkern/arch/sparc/*.[sS]

tags:
	-ctags -wdtf ${TSPARC} ${COMM} ${SPARC}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ASPARC} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TSPARC}
	sort -o ${TSPARC} ${TSPARC}

obj:   _SUBDIRUSE

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
