#	$OpenBSD: Makefile,v 1.1.1.1 2006/05/09 18:07:40 miod Exp $

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=
NOMAN=
NOOBJ=

SUBDIR=

TAVIION=	$S/arch/aviion/tags
SAVIION=	$S/arch/aviion/aviion/*.[ch] \
		$S/arch/aviion/dev/*.[ch] \
		$S/arch/aviion/include/*.h
AAVIION=	$S/arch/aviion/aviion/*.[sS]

# Directories in which to place aviion tags links
DAVIION=	conf dev aviion include

tags:
	-ctags -wdtf ${TAVIION} ${COMM} ${SAVIION}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AAVIION} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TAVIION}
	sort -o ${TAVIION} ${TAVIION}

links:
	-for i in ${DAVIION}; do \
	    (cd $$i && rm -f tags; ln -s ${TAVIION} tags); done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
