# 	$OpenBSD: Makefile,v 1.3 2009/12/25 16:08:10 miod Exp $
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for moko tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=

NOPROG=	noprog
NOMAN=	noman

TMOKO=	$S/arch/moko/tags

SMOKO=	$S/arch/moko/include/*.h \
	$S/arch/moko/moko/*.[ch] $S/arch/arm/arm/*.[ch] \
	$S/arch/arm/include/*.h $S/arch/arm/mainbus/*.[ch] \
	$S/arch/arm/s3c2xx0/*.h $S/arch/arm/xscale/*.[ch]
AMOKO=	$S/arch/moko/moko/*.S $S/arch/arm/arm/*.S \
	$S/arch/arm/mainbus/*.S

# Directories in which to place tags links
DMOKO=	include moko

tags:
	-ctags -wdtf ${TMOKO} ${SMOKO} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMOKO} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TMOKO}
	sort -o ${TMOKO} ${TMOKO}

links:
	-for i in ${DMOKO}; do \
	    (cd $$i && { rm -f tags; ln -s ${TMOKO} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
