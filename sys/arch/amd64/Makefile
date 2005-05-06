#	$OpenBSD: Makefile,v 1.3 2005/05/06 02:59:51 marco Exp $

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	stand

TAMD64=	$S/arch/amd64/tags
SAMD64=	$S/arch/amd64/amd64/*.[ch] \
	$S/arch/amd64/include/*.h  \
	$S/arch/amd64/isa/*.[ch] $S/arch/amd64/pci/*.[ch]
AAMD64=	$S/arch/amd64/amd64/*.[sS] $S/arch/amd64/isa/*.[sS] \
	$S/lib/libkern/arch/amd64/*.[sS]

# Directories in which to place amd64 tags links
DAMD64=	conf amd64 include isa pci

tags:
	-ctags -wdtf ${TAMD64} ${COMM} ${SAMD64}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AAMD64} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TAMD64}
	sort -o ${TAMD64} ${TAMD64}

links:
	-for i in ${DAMD64}; do \
	    (cd $$i && rm -f tags; ln -s ${TAMD64} tags); done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
