#	$NetBSD: Makefile,v 1.1 1996/01/31 23:14:53 mark Exp $
#	@(#)Makefile	7.3 (Berkeley) 6/9/91

# Makefile for arm32 tags file and boot blocks

NOPROG=	noprog
NOMAN=	noman

SUBDIR=

TARM32=	../arm32/tags
SARM32=	../arm32/arm32/*.[ch] ../arm32/include/*.h ../arm32/dev/*.[ch] \
	../arm32/podulebus/*.[ch] ../arm32/mainbus/*.[ch]
AARM32=	../arm32/arm32/*.s

# Directories in which to place arm32 tags links
DARM32=	dev mainbus podulebus include

tags:
	-ctags -dtf ${TARM32} ${COMM} ${SARM32}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AARM32} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TARM32}
	sort -o ${TARM32} ${TARM32}

links:
	-for i in ${DARM32}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
