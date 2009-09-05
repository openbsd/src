# 	$OpenBSD: Makefile,v 1.1 2009/09/05 01:22:11 marex Exp $
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for palm tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

TPALM=		$S/arch/palm/tags

SPALM=		$S/arch/palm/include/*.h \
		$S/arch/palm/palm/*.[ch] $S/arch/arm/arm/*.[ch] \
		$S/arch/arm/include/*.h $S/arch/arm/mainbus/*.[ch] \
		$S/arch/arm/sa11x0/*.[ch] $S/arch/arm/xscale/*.[ch]
APALM=		$S/arch/palm/palm/*.S $S/arch/arm/arm/*.S \
		$S/arch/arm/mainbus/*.S $S/arch/arm/xscale/*.S

# Directories in which to place tags links
DPALM=	include palm

tags:
	-ctags -wdtf ${TPALM} ${SPALM} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${APALM} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TPALM}
	sort -o ${TPALM} ${TPALM}

links:
	-for i in ${DPALM}; do \
	    (cd $$i && { rm -f tags; ln -s ${TPALM} tags; }) done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
