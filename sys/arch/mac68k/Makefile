#	$OpenBSD: Makefile,v 1.4 1997/11/20 07:31:40 gene Exp $
#	$NetBSD: Makefile,v 1.3 1996/03/27 03:56:22 scottr Exp $

#	@(#)Makefile	7.3 (Berkeley) 6/9/91

# Makefile for mac68k tags file

NOPROG=	noprog
NOMAN=	noman

TMAC68K=	../mac68k/tags
SMAC68K=	../m68k/m68k/*.c ../m68k/include/*.h \
		../mac68k/mac68k/*.[ch] ../mac68k/include/*.h \
		../mac68k/dev/*.[ch]
AMAC68K=	../m68k/m68k/*.s ../mac68k/mac68k/*.s

SUBDIR=../m68k

# Directories in which to place mac68k tags links
DMAC68K=	mac68k dev include

tags:
	-ctags -dtf ${TMAC68K} ${COMM} ${SMAC68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMAC68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TMAC68K}
	sort -o ${TMAC68K} ${TMAC68K}

links:
	-for i in ${DMAC68K}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
