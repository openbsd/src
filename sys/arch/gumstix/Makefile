# 	$OpenBSD: Makefile,v 1.2 2008/11/27 20:50:34 miod Exp $
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for gumstix tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=	stand

NOPROG=	noprog
NOMAN=	noman

TGUMSTIX=	$S/arch/gumstix/tags

SGUMSTIX=	$S/arch/gumstix/include/*.h \
		$S/arch/gumstix/gumstix/*.[ch] $S/arch/arm/arm/*.[ch] \
		$S/arch/arm/include/*.h $S/arch/arm/mainbus/*.[ch] \
		$S/arch/arm/sa11x0/*.[ch] $S/arch/arm/xscale/*.[ch] \
		$S/arch/arm/s3c2xx0/*.[ch]
AGUMSTIX=	$S/arch/gumstix/gumstix/*.S $S/arch/arm/arm/*.S \
		$S/arch/arm/mainbus/*.S $S/arch/arm/xscale/*.S

# Directories in which to place tags links
DGUMSTIX=	include gumstix

tags:
	-ctags -wdtf ${TGUMSTIX} ${SGUMSTIX} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AGUMSTIX} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TGUMSTIX}
	sort -o ${TGUMSTIX} ${TGUMSTIX}

links:
	-for i in ${DGUMSTIX}; do \
	    (cd $$i && { rm -f tags; ln -s ${TGUMSTIX} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
