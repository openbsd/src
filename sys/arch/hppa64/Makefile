#	$OpenBSD: Makefile,v 1.1 2005/04/01 10:40:46 mickey Exp $

# Makefile for hppa64 tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	stand

THPPA=	$S/arch/hppa64/tags
SHPPA=	$S/arch/hppa64/hppa64/*.[ch] $S/arch/hppa64/include/*.h \
	$S/arch/hppa64/dev/*.[ch]
AHPPA=	$S/arch/hppa64/hppa64/*.S $S/lib/libkern/arch/hppa64/*.[sS]

# Directories in which to place hppa64 tags links
DHPPA=	conf dev hppa64 include

tags:
	-ctags -wdtf ${THPPA} ${COMM} ${SHPPA}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AHPPA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${THPPA}
	sort -o ${THPPA} ${THPPA}

links:
	-for i in ${DHPPA}; do \
	    cd $$i && rm -f tags; ln -s ${THPPA} tags; done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
