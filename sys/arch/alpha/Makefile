#	$OpenBSD: Makefile,v 1.4 1999/12/30 17:31:45 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1995/02/27 16:36:07 cgd Exp $

# Makefile for alpha tags file and boot blocks

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	stand

TALPHA=	../alpha/tags
SALPHA=	../alpha/alpha/*.[ch] ../alpha/include/*.h ../alpha/tc/*.[ch]
AALPHA=	../alpha/alpha/*.S

# Directories in which to place alpha tags links
DALPHA=	tc include

tags:
	-ctags -dtf ${TALPHA} ${COMM} ${SALPHA}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AALPHA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TALPHA}
	sort -o ${TALPHA} ${TALPHA}

links:
	-for i in ${DALPHA}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done

obj: _SUBDIRUSE

.include <bsd.prog.mk>
