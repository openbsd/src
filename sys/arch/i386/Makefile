#	$NetBSD: Makefile,v 1.8 1994/10/27 04:14:23 cgd Exp $
#	@(#)Makefile	7.3 (Berkeley) 6/9/91

# Makefile for i386 tags file and boot blocks

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	boot

TI386=	../i386/tags
SI386=	../i386/i386/*.[ch] ../i386/include/*.h ../i386/isa/*.[ch]
AI386=	../i386/i386/*.s

# Directories in which to place i386 tags links
DI386=	eisa isa mca include

tags:
	-ctags -dtf ${TI386} ${COMM} ${SI386}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AI386} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TI386}
	sort -o ${TI386} ${TI386}

links:
	-for i in ${DI386}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
