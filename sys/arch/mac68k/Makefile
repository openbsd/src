#	$NetBSD: Makefile,v 1.2 1994/10/26 08:45:48 cgd Exp $

#	@(#)Makefile	7.3 (Berkeley) 6/9/91
S=${BSDSRCDIR}/sys
COMM=	$S/vm/*.[ch] $S/ufs/*/*.[ch] $S/sys/*.h $S/compat/sunos/*.[ch] \
	$S/isofs/*/*.[ch] \
	$S/nfs/*.[ch] $S/netns/*.[ch] $S/netiso/*.[ch] \
	$S/netiso/xebec/*.[ch] $S/netinet/*.[ch] $S/netccitt/*.[ch] \
	$S/net/*.[ch] $S/miscfs/*/*.[ch] $S/kern/*.[ch] $S/dev/*.[ch] \
	$S/scsi/*.[ch] $S/lib/libkern/arch/m68k/*.[ch] $S/lib/libkern/*.[ch]

# Makefile for mac68k tags file

all:
	@echo "make tags or links only"

TMAC68K=	$S/arch/mac68k/tags
SMAC68K=	$S/arch/m68k/m68k/*.c $S/arch/m68k/include/*.h \
		$S/arch/mac68k/mac68k/*.[ch] $S/arch/mac68k/include/*.h \
		$S/arch/mac68k/dev/*.[ch]
AMAC68K=	$S/arch/m68k/m68k/*.s $S/arch/mac68k/mac68k/*.s

# Directories in which to place mac68k tags links
DMAC68K=	mac68k dev include

TAGS:
	-etags -dt ${COMM} ${SMAC68K} ${AMAC68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMAC68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TMAC68K}/tags

tags:
	-ctags -dtf ${TMAC68K} ${COMM} ${SMAC68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMAC68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TMAC68K}
	sort -o ${TMAC68K} ${TMAC68K}

links:
	-for i in ${DMAC68K}; do \
	    rm -f $$i/tags; rm -f $$i/TAGS ; \
	    ln -s ../tags $$i/tags; ln -s ../TAGS $$i/TAGS; done

.include <bsd.prog.mk>

