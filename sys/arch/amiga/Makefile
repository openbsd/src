#	$NetBSD: Makefile,v 1.10 1994/10/26 02:01:24 cgd Exp $

#	@(#)Makefile	7.3 (Berkeley) 6/9/91

S=${BSDSRCDIR}/sys
COMM=	$S/vm/*.[ch] $S/ufs/*.[ch] $S/sys/*.h $S/compat/sunos/*.[ch] \
	$S/nfs/*.[ch] $S/netns/*.[ch] $S/netiso/*.[ch] \
	$S/netiso/xebec/*.[ch] $S/netinet/*.[ch] $S/netccitt/*.[ch] \
	$S/net/*.[ch] $S/miscfs/*/*.[ch] $S/kern/*.[ch] $S/dev/*.[ch] \
	$S/scsi/*.[ch] $S/lib/libkern/m68k/*.[ch] $S/lib/libkern/*.[ch]

# Makefile for amiga tags file

all:
	@echo "make tags or links only"

TAMIGA=	$S/arch/amiga/tags
SAMIGA=	$S/arch/m68k/m68k/*.c $S/arch/m68k/include/*.h \
	$S/arch/amiga/amiga/*.[ch] $S/arch/amiga/include/*.h \
	$S/arch/amiga/dev/*.[ch]
AAMIGA=	$S/arch/m68k/m68k/*.s $S/arch/amiga/amiga/*.s

# Directories in which to place amiga tags links
DAMIGA=	amiga dev include

TAGS:
	-etags -dt ${COMM} ${SAMIGA} ${AAMIGA}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AAMIGA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TAMIGA}/tags

tags:
	-ctags -dtf ${TAMIGA} ${COMM} ${SAMIGA}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AAMIGA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TAMIGA}
	sort -o ${TAMIGA} ${TAMIGA}

links:
	-for i in ${DAMIGA}; do \
	    rm -f $$i/tags; rm -f $$i/TAGS ; \
	    ln -s ../tags $$i/tags; ln -s ../TAGS $$i/TAGS; done

.include <bsd.prog.mk>

