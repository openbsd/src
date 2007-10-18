#	$OpenBSD: Makefile,v 1.10 2007/10/18 18:08:02 deraadt Exp $

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR+= 060sp fpsp

TM68K=	$S/arch/m68k/tags
SM68K=	$S/arch/m68k/m68k/*.[ch] \
	$S/arch/m68k/include/*.h
AM68K=	$S/arch/m68k/m68k/*.[sS]  \
	$S/lib/libkern/arch/m68k/*.[sS] \
	$S/arch/m68k/fpsp/*.sa    \
	$S/arch/m68k/060sp/*.[sS] \
	$S/arch/m68k/060sp/*.sa   \

# Directories in which to place m68k tags links
DM68K=	conf m68k include pci

tags:
	-ctags -wdtf ${TM68K} ${COMM} ${SM68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AM68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TM68K}
	sort -o ${TM68K} ${TM68K}

links:
	-for i in ${DM68K}; do \
	    cd $$i && rm -f tags; ln -s ${TM68K} tags; done


.include <bsd.subdir.mk>
