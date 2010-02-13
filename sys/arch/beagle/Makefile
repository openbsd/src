# 	$OpenBSD: Makefile,v 1.1 2010/02/13 18:23:42 drahn Exp $
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for beagle tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

#SUBDIR=	stand

NOPROG=	noprog
NOMAN=	noman

TBEAGLE=	$S/arch/beagle/tags

SBEAGLE=	$S/arch/beagle/dev/*.[ch] $S/arch/beagle/include/*.h \
		$S/arch/beagle/beagle/*.[ch] $S/arch/arm/arm/*.[ch] \
		$S/arch/arm/include/*.h $S/arch/arm/mainbus/*.[ch]
ABEAGLE=	$S/arch/beagle/beagle/*.S $S/arch/arm/arm/*.S \
		$S/arch/arm/mainbus/*.S 

# Directories in which to place tags links
DBEAGLE=	dev include beagle

tags:
	-ctags -wdtf ${TBEAGLE} ${SBEAGLE} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ABEAGLE} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TBEAGLE}
	sort -o ${TBEAGLE} ${TBEAGLE}

links:
	-for i in ${DBEAGLE}; do \
	    (cd $$i && { rm -f tags; ln -s ${TBEAGLE} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
