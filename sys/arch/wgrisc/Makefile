#	$OpenBSD: Makefile,v 1.3 2000/03/03 00:54:56 todd Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/16/93

# Makefile for pica links, tags file

.include "../../kern/Make.tags.inc"

all:
	@echo "make links or tags only"

DIRS=	conf dev dist include pica

links::
	-for i in ${DIRS}; do \
	    (cd $$i && { rm -f tags; ln -s ${SYSTAGS} tags; }) done

PICA=	/sys/pica/dev/*.[ch] /sys/pica/include/*.[ch] \
	/sys/pica/pica/*.[ch] /sys/pica/ultrix/*.[ch]
APICA=	/sys/pica/pica/*.s

tags::
	-ctags -wdtif ${COMM} ${PICA}
	egrep "^LEAF(.*)|^[AN]LEAF(.*)|^NON_LEAF(.*)" ${APICA} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> tags
	sort -o tags tags

obj:   _SUBDIRUSE

.include <bsd.prog.mk>
