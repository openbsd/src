#	$OpenBSD: Makefile,v 1.2 2000/05/31 19:44:57 mickey Exp $

# Makefile for hppa tags file and boot blocks

.include "../../kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	stand

THPPA=	../hppa/tags
SHPPA=	../hppa/hppa/*.[ch] ../hppa/include/*.h ../hppa/dev/*.[ch] \
	../hppa/gsc/*.[ch]
AHPPA=	../hppa/hppa/*.S

# Directories in which to place hppa tags links
DHPPA=	conf dev gsc hppa include

tags:
	-ctags -dtf ${THPPA} ${COMM} ${SHPPA}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AHPPA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${THPPA}
	sort -o ${THPPA} ${THPPA}

links:
	-for i in ${DHPPA}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
