#	$OpenBSD: Makefile,v 1.17 2007/10/22 01:26:31 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1996/02/01 22:31:28 mycroft Exp $
#	@(#)Makefile	7.3 (Berkeley) 6/9/91

# Makefile for i386 tags file and boot blocks

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=	stand

TI386=	$S/arch/i386/tags
SI386=	$S/arch/i386/eisa/*.[ch] $S/arch/i386/i386/*.[ch] \
	$S/arch/i386/include/*.h $S/arch/i386/include/pc/*.h \
	$S/arch/i386/isa/*.[ch] $S/arch/i386/pci/*.[ch]
AI386=	$S/arch/i386/i386/*.[sS] $S/arch/i386/isa/*.[sS] \
	$S/lib/libkern/arch/i386/*.[sS]

# Directories in which to place i386 tags links
DI386=	conf eisa i386 include isa pci

tags:
	-ctags -wdtf ${TI386} ${COMM} ${SI386}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AI386} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TI386}
	sort -o ${TI386} ${TI386}

links:
	-for i in ${DI386}; do \
	    (cd $$i && rm -f tags; ln -s ${TI386} tags); done

obj:	_SUBDIRUSE

.include <bsd.subdir.mk>
