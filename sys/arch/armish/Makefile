# 	$OpenBSD: Makefile,v 1.1 2006/05/29 17:22:48 drahn Exp $
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for armish tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=	stand

NOPROG=	noprog
NOMAN=	noman

TARMISH=	$S/arch/armish/tags

SARMISH=	$S/arch/armish/dev/*.[ch] $S/arch/armish/include/*.h \
		$S/arch/armish/armish/*.[ch] $S/arch/arm/arm/*.[ch] \
		$S/arch/arm/include/*.h $S/arch/arm/mainbus/*.[ch] \
		$S/arch/arm/sa11x0/*.[ch] $S/arch/arm/xscale/*.[ch]
AARMISH=	$S/arch/armish/armish/*.S $S/arch/arm/arm/*.S \
		$S/arch/arm/mainbus/*.S $S/arch/arm/xscale/*.S

# Directories in which to place tags links
DARMISH=	dev include armish

tags:
	-ctags -wdtf ${TARMISH} ${SARMISH} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AARMISH} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TARMISH}
	sort -o ${TARMISH} ${TARMISH}

links:
	-for i in ${DARMISH}; do \
	    (cd $$i && { rm -f tags; ln -s ${TARMISH} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
