#	$OpenBSD: Makefile,v 1.4 2000/12/28 21:21:23 smurph Exp $

# from: @(#)Makefile	8.1 (Berkeley) 6/11/93
#
# Makefile for mvme88k links, tags file

NOPROG=	noprog
NOMAN=	noman
NOOBJ=	noobj

SYSTAGS= ${.CURDIR}/tags
SUBDIR=	stand 
DIRS=	conf dev include mvme88k

links::
	-for i in ${DIRS}; do \
		echo $$i ; \
		cd ${.CURDIR} ; \
	    (cd $$i && { rm -f tags; ln -s ${SYSTAGS} tags; }) done

MVME88K=/sys/arch/mvme88k/dev/*.[ch] /sys/arch/mvme88k/include/*.[ch] \
	/sys/arch/mvme88k/mvme88k/*.[ch]
AMVME88K=/sys/arch/mvme88k/mvme88k/*.s

tags:
	-ctags -wdt ${COMM} ${MVME88K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMVME88K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> tags
	sort -o tags tags

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
