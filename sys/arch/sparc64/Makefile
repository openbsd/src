#	$OpenBSD: Makefile,v 1.1 2001/08/31 10:55:43 art Exp $

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	stand

TSPARC64=	../sparc64/tags
SSPARC64=	../sparc64/sparc64/*.[ch] ../sparc64/include/*.h ../sparc64/tc/*.[ch]
ASPARC64=	../sparc64/sparc64/*.S

# Directories in which to place sparc64 tags links
DSPARC64=	tc include

tags:
	-ctags -dtf ${TSPARC64} ${COMM} ${SSPARC64}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ASPARC64} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TSPARC64}
	sort -o ${TSPARC64} ${TSPARC64}

links:
	-for i in ${DSPARC64}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
