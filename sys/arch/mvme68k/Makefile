#	$OpenBSD: Makefile,v 1.8 1997/10/19 22:35:57 etheisen Exp $

# from: @(#)Makefile	8.1 (Berkeley) 6/11/93
#
# Makefile for mvme68k links, tags file

NOPROG=	noprog
NOMAN=	noman
NOOBJ=	noobj

SYSTAGS= ${.CURDIR}/tags
SUBDIR=	stand ../m68k
DIRS=	conf dev include mvme68k

links::
	-for i in ${DIRS}; do \
		echo $$i ; \
		cd ${.CURDIR} ; \
	    (cd $$i && { rm -f tags; ln -s ${SYSTAGS} tags; }) done

MVME68K=/sys/arch/mvme68k/dev/*.[ch] /sys/arch/mvme68k/include/*.[ch] \
	/sys/arch/mvme68k/mvme68k/*.[ch]
AMVME68K=/sys/arch/mvme68k/mvme68k/*.s

tags:
	-ctags -wdt ${COMM} ${MVME68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMVME68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> tags
	sort -o tags tags

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
