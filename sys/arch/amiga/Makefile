#	$OpenBSD: Makefile,v 1.5 2002/07/15 21:29:16 mickey Exp $
#	$NetBSD: Makefile,v 1.11 1997/01/06 12:33:09 is Exp $

#	@(#)Makefile	7.3 (Berkeley) 6/9/91

S=${.CURDIR}/../..
COMM=	$S/uvm/*.[ch] $S/ufs/*/*.[ch] $S/sys/*.h $S/compat/sunos/*.[ch] \
	$S/nfs/*.[ch] $S/netns/*.[ch] $S/netiso/*.[ch] \
	$S/netinet/*.[ch] $S/netinet6/*.[ch] $S/netccitt/*.[ch] \
	$S/net/*.[ch] $S/miscfs/*/*.[ch] $S/kern/*.[ch] $S/dev/*.[ch] \
	$S/scsi/*.[ch] $S/lib/libkern/arch/m68k/*.[sS] $S/lib/libkern/*.[ch]

# Makefile for amiga tags file

SUBDIR=stand ../m68k

TAMIGA=	$S/arch/amiga/tags
SAMIGA=	$S/arch/m68k/m68k/*.c $S/arch/m68k/include/*.h \
	$S/arch/amiga/amiga/*.[ch] $S/arch/amiga/include/*.h \
	$S/arch/amiga/dev/*.[ch]
AAMIGA=	$S/arch/m68k/m68k/*.s $S/arch/amiga/amiga/*.s

# Directories in which to place amiga tags links
DAMIGA=	amiga dev include

tags:
	-ctags -wdtf ${TAMIGA} ${COMM} ${SAMIGA}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AAMIGA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TAMIGA}
	sort -o ${TAMIGA} ${TAMIGA}

links:
	-for i in ${DAMIGA}; do \
	    rm -f $$i/tags; rm -f $$i/TAGS ; \
	    ln -s ${TAMIGA} $$i/tags; done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

