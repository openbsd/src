# $OpenBSD: Makefile.inc,v 1.2 2010/04/23 00:31:00 schwarze Exp $

DIFF=diff -au
MANDOC?=mandoc
#MANDOCOPTS=-fstrict -Wall
NROFF=nroff -mandoc
SED=sed
SH=/bin/sh

all: clean

groff: ${GROFF_TARGETS:C/$/.out_ascii/}

.SUFFIXES: .in .out_ascii .sh

.in:
	@echo "  -> ${*}"
	@${MANDOC} ${MANDOCOPTS} ${.CURDIR}/${*}.in | ${SED} '$$d' \
		> ${*}.mandoc_ascii
	@${DIFF} ${*}.out_ascii ${*}.mandoc_ascii

.in.out_ascii:
	@echo "  -> ${*}"
	@${NROFF} ${.CURDIR}/${<} | ${SED} '$$d' > ${@}

.sh:
	@echo "  -> ${*}"
	@MANDOC="${MANDOC}" MANDOCOPTS="${MANDOCOPTS}" \
		${SH} ${.CURDIR}/${*}.sh 

clean:
	rm -f *.mandoc_ascii

groff-clean:
.if defined(GROFF_TARGETS)
	rm -f ${GROFF_TARGETS:C/$/.out_ascii/}
.endif
