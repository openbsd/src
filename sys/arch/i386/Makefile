#	$NetBSD: Makefile,v 1.6 1996/02/01 22:31:28 mycroft Exp $
#	@(#)Makefile	7.3 (Berkeley) 6/9/91

# Makefile for i386 tags file and boot blocks

.include "../../kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	boot boot.dos stand isa/pcvt/Util

TI386=	../i386/tags
SI386=	../i386/eisa/*.[ch] ../i386/i386/*.[ch] ../i386/include/*.h \
	../i386/include/pc/*.h ../i386/isa/*.[ch] ../i386/isa/pcvt/*.[ch] \
	../i386/pci/*.[ch]
AI386=	../i386/i386/*.s ../i386/isa/*.s

# Directories in which to place i386 tags links
DI386=	conf eisa i386 include isa pci

tags:
	-ctags -wdtf ${TI386} ${COMM} ${SI386}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AI386} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TI386}
	sort -o ${TI386} ${TI386}

links:
	-for i in ${DI386}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
