#	$OpenBSD: Makefile,v 1.2 2002/07/15 21:29:16 mickey Exp $


S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	stand

TSPARC64=	$S/arch/sparc64/tags
SSPARC64=	$S/arch/sparc64/sparc64/*.[ch] $S/arch/sparc64/include/*.h \
		$S/arch/sparc64/dev/*.[ch] $S/arch/sparc64/fpu/*.[ch]
ASPARC64=	$S/arch/sparc64/sparc64/*.S $S/lib/libkern/arch/sparc64/*.[sS]

# Directories in which to place sparc64 tags links
DSPARC64=	sparc64 dev fpu include

tags:
	-ctags -wdtf ${TSPARC64} ${COMM} ${SSPARC64}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ASPARC64} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TSPARC64}
	sort -o ${TSPARC64} ${TSPARC64}

links:
	-for i in ${DSPARC64}; do \
	    (cd $$i && rm -f tags; ln -s ${TSPARC64} tags); done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
