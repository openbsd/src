#	from: @(#)Makefile	8.1 (Berkeley) 6/16/93
#	$Id: Makefile,v 1.2 1996/07/16 07:46:11 pefo Exp $

# Makefile for arc links, tags file

.include "../../kern/Make.tags.inc"

all:
	@echo "make links or tags only"

DIRS=	arc conf dev include isa pica

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

.include <bsd.prog.mk>
