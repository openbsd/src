#	$OpenBSD: Makefile,v 1.3 2006/04/15 17:35:11 miod Exp $
#	$NetBSD: Makefile,v 1.4 1996/03/25 22:48:09 mrg Exp $

#
# Makefile for solbourne links, tags file

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=

NOPROG=	noprog
NOMAN=	noman

TSOLBOURNE=	$S/arch/solbourne/tags
DIRS=	conf include dev solourne

links::
	-for i in ${DIRS}; do \
	    (cd $$i && { rm -f tags; ln -s ${TSOLBOURNE} tags; }) done

SOLBOURNE=	$S/arch/solbourne/dev/*.[ch] \
	$S/arch/solbourne/include/*.[ch] $S/arch/solbourne/solbourne/*.[ch]
ASOLBOURNE=	$S/arch/solbourne/solbourne/*.[sS]

tags:
	-ctags -wdtf ${TSOLBOURNE} ${COMM} ${SOLBOURNE}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ASOLBOURNE} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TSOLBOURNE}
	sort -o ${TSOLBOURNE} ${TSOLBOURNE}

obj:   _SUBDIRUSE

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
