#	$OpenBSD: Makefile,v 1.1 1998/06/23 20:23:29 mickey Exp $

# Makefile for hppa tags file and boot blocks

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	stand

THPPA=	../hppa/tags
SHPPA=	../hppa/hppa/*.[ch] ../hppa/include/*.h ../hppa/dev/*.[ch]
AHPPA=	../hppa/hppa/*.S

# Directories in which to place hppa tags links
DHPPA=	dev hppa include

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
