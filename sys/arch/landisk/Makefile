#	$OpenBSD: Makefile,v 1.1.1.1 2006/10/06 21:16:15 miod Exp $
#	$NetBSD: Makefile,v 1.1 2006/09/01 21:26:18 uwe Exp $

# Makefile for landisk tags file and boot blocks

TLANDISK=	../landisk/tags
SLANDISK=	../landisk/landisk/*.[ch] ../landisk/include/*.h
ALANDISK=	../landisk/landisk/*.[sS]

# Directories in which to place tags links
DLANDISK=	landisk include

.include "../../kern/Make.tags.inc"

tags:
	-ctags -wdtf ${TLANDISK} ${SLANDISK} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ALANDISK} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TLANDISK}
	sort -o ${TLANDISK} ${TLANDISK}

links:
	-for i in ${DLANDISK}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done

SUBDIR=	stand

.include <bsd.subdir.mk>
