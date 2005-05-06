# 	$OpenBSD: Makefile,v 1.3 2005/05/06 02:20:24 uwe Exp $
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for cats tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

TCATS=	$S/arch/cats/tags
SCATS=	$S/arch/cats/cats/*.[ch] $S/arch/cats/include/*.h \
	$S/arch/cats/isa/*.[ch] $S/arch/cats/pci/*.[ch] \
	$S/arch/arm/arm/*.[ch] $S/arch/arm/footbridge/*.[ch] \
	$S/arch/arm/footbridge/isa/*.[ch]
ACATS=	$S/arch/arm/arm/*.S \
	$S/arch/arm/footbridge/isa/*.S

# Directories in which to place tags links
DCATS=	cats include isa pci

tags:
	-ctags -wdtf ${TCATS} ${SCATS} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ACATS} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TCATS}
	sort -o ${TCATS} ${TCATS}

links:
	-for i in ${DCATS}; do \
	    (cd $$i && { rm -f tags; ln -s ${TCATS} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
