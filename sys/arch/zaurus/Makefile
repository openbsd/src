/* 	$OpenBSD: Makefile,v 1.2 2005/01/10 00:25:45 deraadt Exp $	*/
#	$NetBSD: Makefile,v 1.1 2001/06/12 08:32:46 chris Exp $

# Makefile for zaurus tags file and boot blocks

TCATS=	../zaurus/tags
SCATS=	../zaurus/zaurus/*.[ch] ../zaurus/include/*.h \
		../zaurus/isa/*.[ch]
ACATS=	../zaurus/zaurus/*.S

# Directories in which to place tags links
DCATS=	isa include

.include "../../kern/Make.tags.inc"

tags:
	-ctags -wdtf ${TCATS} ${SCATS} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ACATS} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TCATS}
	sort -o ${TCATS} ${TCATS}

links:
	-for i in ${DCATS}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done


SUBDIR=	stand

.include <bsd.subdir.mk>
