#	$OpenBSD: Makefile,v 1.5 2002/07/15 21:29:16 mickey Exp $
#	$NetBSD: Makefile,v 1.3 1996/03/27 03:56:22 scottr Exp $

#	@(#)Makefile	7.3 (Berkeley) 6/9/91

# Makefile for mac68k tags file

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

TMAC68K=$S/arch/mac68k/tags
SMAC68K=$S/arch/m68k/m68k/*.c $S/arch/m68k/include/*.h \
	$S/arch/mac68k/mac68k/*.[ch] $S/arch/mac68k/include/*.h \
	$S/arch/mac68k/dev/*.[ch]
AMAC68K=$S/arch/m68k/m68k/*.s $S/arch/mac68k/mac68k/*.s \
	$S/lib/libkern/arch/m68k/*.[sS]

SUBDIR=../m68k

# Directories in which to place mac68k tags links
DMAC68K=	mac68k dev include

tags:
	-ctags -wdtf ${TMAC68K} ${COMM} ${SMAC68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMAC68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TMAC68K}
	sort -o ${TMAC68K} ${TMAC68K}

links:
	-for i in ${DMAC68K}; do \
	    cd $$i && rm -f tags; ln -s ${TMAC68K} tags; done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
