#	$OpenBSD: Makefile,v 1.1 2008/05/10 21:04:19 kettenis Exp $

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

.if ${MACHINE} == "docppc"
SUBDIR= stand
.endif

TSOCPPC=	$S/arch/socppc/tags
SSOCPPC=	$S/arch/socppc/socppc/*.[ch]	\
		$S/arch/socppc/include/*.h	\
		$S/arch/socppc/dev/*.[ch]	\
		$S/arch/powerpc/include/*.h
ASOCPPC=	$S/arch/socppc/socppc/*.[sS]

# Directories in which to place socppc tags links
DSOCPPC=	conf socppc include

tags:
	-ctags -wdtf ${TSOCPPC} ${COMM} ${SSOCPPC}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ASOCPPC} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TSOCPPC}
	sort -o ${TSOCPPC} ${TSOCPPC}

links:
	-for i in ${DSOCPPC}; do \
	    cd $$i && rm -f tags; ln -s ${TSOCPPC} tags; done


obj:	_SUBDIRUSE

.include <bsd.prog.mk>
