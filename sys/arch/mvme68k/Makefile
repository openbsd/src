#	$OpenBSD: Makefile,v 1.4 1996/04/28 11:12:35 deraadt Exp $

# from: @(#)Makefile	8.1 (Berkeley) 6/11/93
#
# Makefile for mvme68k links, tags file

NOPROG=	noprog
NOMAN=	noman
NOOBJ=	noobj

DIRS=	conf dev include mvme68k

links::
	-for i in ${DIRS}; do \
	    (cd $$i && { rm -f tags; ln -s ${SYSTAGS} tags; }) done

MVME68K=/sys/arch/mvme68k/dev/*.[ch] /sys/arch/mvme68k/include/*.[ch] \
	/sys/arch/mvme68k/rcons/*.[ch] /sys/arch/mvme68k/mvme68k/*.[ch] \
AMVME68K=/sys/arch/mvme68k/mvme68k/*.s

tags:
	-ctags -wdt ${COMM} ${MVME68K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMVME68K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> tags
	sort -o tags tags

SUBDIR+=stand ../m68k

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
