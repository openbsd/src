# 	$OpenBSD: Makefile,v 1.1 2010/02/14 22:39:31 miod Exp $

# Makefile for loongson tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=	stand

NOPROG=	noprog
NOMAN=	noman

TLOONGSON=	$S/arch/loongson/tags

SLOONGSON=	$S/arch/loongson/dev/*.[ch] $S/arch/loongson/include/*.h \
		$S/arch/loongson/loongson/*.[ch] \
		$S/arch/mips64/dev/*.[ch] $S/arch/mips64/include/*.h \
		$S/arch/mips64/mips64/*.[ch]
ALOONGSON=	$S/arch/loongson/loongson/*.S $S/arch/mips64/mips64/*.S

# Directories in which to place tags links
DLOONGSON=	dev include loongson

tags:
	-ctags -wdtf ${TLOONGSON} ${SLOONGSON} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ALOONGSON} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TLOONGSON}
	sort -o ${TLOONGSON} ${TLOONGSON}

links:
	-for i in ${DLOONGSON}; do \
	    (cd $$i && { rm -f tags; ln -s ${TLOONGSON} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
