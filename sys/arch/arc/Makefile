#	$OpenBSD: Makefile,v 1.6 2000/03/03 00:54:45 todd Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/16/93

# Makefile for arc links, tags file

.include "../../kern/Make.tags.inc"

SUBDIR=	stand

NOPROG=	noprog
NOOBJ=	noman

TARC=	../arc/tags
SARC=	../arc/arc/*.[ch] ../arc/dev/*.[ch] ../arc/dti/*.[ch] \
	../arc/include/*.h ../arc/isa/*.[ch] ../arc/pci/*.[ch] \
	../arc/pica/*.[ch]
AARC=	../arc/arc/*.S

DARC=	arc dev dti include isa pci pica

tags:
	-ctags -wdtf ${TARC} ${COMM} ${SARC}
	egrep "^LEAF(.*)|^[AN]LEAF(.*)|^NON_LEAF(.*)" ${AARC} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TARC}
	sort -o ${TARC} ${TARC}

links:
	-for i in ${DARC}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done


obj:	_SUBDIRUSE

.include <bsd.prog.mk>
