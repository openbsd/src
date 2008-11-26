# 	$OpenBSD: Makefile,v 1.1 2008/11/26 14:18:11 drahn Exp $
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for zaurus tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=	stand

NOPROG=	noprog
NOMAN=	noman

TZAURUS=	$S/arch/zaurus/tags

SZAURUS=	$S/arch/zaurus/dev/*.[ch] $S/arch/zaurus/include/*.h \
		$S/arch/zaurus/zaurus/*.[ch] $S/arch/arm/arm/*.[ch] \
		$S/arch/arm/include/*.h $S/arch/arm/mainbus/*.[ch] \
		$S/arch/arm/sa11x0/*.[ch] $S/arch/arm/xscale/*.[ch]
AZAURUS=	$S/arch/zaurus/zaurus/*.S $S/arch/arm/arm/*.S \
		$S/arch/arm/mainbus/*.S $S/arch/arm/xscale/*.S

# Directories in which to place tags links
DZAURUS=	dev include zaurus

tags:
	-ctags -wdtf ${TZAURUS} ${SZAURUS} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AZAURUS} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TZAURUS}
	sort -o ${TZAURUS} ${TZAURUS}

links:
	-for i in ${DZAURUS}; do \
	    (cd $$i && { rm -f tags; ln -s ${TZAURUS} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
