#	$OpenBSD: Makefile,v 1.2 2007/10/22 01:26:17 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 2006/09/01 21:26:18 uwe Exp $

# Makefile for landisk tags file and boot blocks

S=${.CURDIR}/../..
.include "../../kern/Make.tags.inc"

SUBDIR=	stand

TLANDISK=	../landisk/tags
SLANDISK=	../landisk/landisk/*.[ch] ../landisk/include/*.h
ALANDISK=	../landisk/landisk/*.[sS]

# Directories in which to place tags links
DLANDISK=	landisk include

tags:
	-ctags -wdtf ${TLANDISK} ${SLANDISK} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ALANDISK} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TLANDISK}
	sort -o ${TLANDISK} ${TLANDISK}

links:
	-for i in ${DLANDISK}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done

.include <bsd.subdir.mk>
