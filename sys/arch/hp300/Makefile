#	$OpenBSD: Makefile,v 1.7 2005/05/06 03:13:53 marco Exp $
#	$NetBSD: Makefile,v 1.5 1995/08/04 07:40:31 thorpej Exp $
#	@(#)Makefile	7.3 (Berkeley) 6/9/91

# Makefile for hp300 tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

SUBDIR= stand ../m68k

THP300=	$S/arch/hp300/tags
SHP300=	$S/arch/m68k/m68k/*.c $S/arch/m68k/include/*.h \
	$S/arch/hp300/hp300/*.[ch] $S/arch/hp300/include/*.h \
	$S/arch/hp300/dev/*.[ch]
AHP300=	$S/arch/m68k/m68k/*.[Ss] $S/arch/hp300/hp300/*.[Ss] \
	$S/lib/libkern/arch/m68k/*.[sS]

# Directories in which to place hp300 tags links
DHP300=	conf dev hp300 include

tags:
	-ctags -wdtf ${THP300} ${COMM} ${SHP300}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AHP300} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${THP300}
	sort -o ${THP300} ${THP300}

links:
	-for i in ${DHP300}; do \
	    (cd $$i && rm -f tags; ln -s ${THP300} tags); done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
