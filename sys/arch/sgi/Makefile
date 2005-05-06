#	$OpenBSD: Makefile,v 1.3 2005/05/06 11:16:48 pedro Exp $

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

SUBDIR=	stand

NOPROG=	noprog
NOOBJ=	noman

TSGI=	$S/arch/sgi/tags
SSGI=	$S/arch/sgi/sgi/*.[ch]  \
	$S/arch/sgi/include/*.h \
	$S/arch/sgi/pci/*.[ch]
ASGI=	$S/arch/sgi/sgi/*.[sS]

# Directories in which to place sgi tags links
DSGI=	conf sgi include pci

tags:
	-ctags -wdtf ${TSGI} ${COMM} ${SSGI}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ASGI} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TSGI}
	sort -o ${TSGI} ${TSGI}

links:
	-for i in ${DSGI}; do \
	    cd $$i && rm -f tags; ln -s ${TSGI} tags; done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
