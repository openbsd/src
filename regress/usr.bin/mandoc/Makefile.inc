# $OpenBSD: Makefile.inc,v 1.5 2011/05/31 08:00:25 schwarze Exp $

DIFF=diff -au
MANDOC?=mandoc
#MANDOCOPTS=-Wall
NROFF=/usr/local/bin/nroff -c -mandoc -Tascii
SED=sed
SH=/bin/sh

all: clean

groff: ${GROFF_TARGETS:C/$/.out_ascii/}

.SUFFIXES: .in .out_ascii .sh

.in:
	@echo "  -> ${*}"
	@${MANDOC} ${MANDOCOPTS} ${.CURDIR}/${*}.in | ${SED} '$$d' \
		> ${*}.mandoc_ascii
	@${DIFF} ${.CURDIR}/${*}.out_ascii ${*}.mandoc_ascii

.in.out_ascii:
	@echo "  -> ${*}"
	@${NROFF} ${<} | ${SED} '$$d' > ${@}

.sh:
	@echo "  -> ${*}"
	@MANDOC="${MANDOC}" MANDOCOPTS="${MANDOCOPTS}" \
		${SH} ${.CURDIR}/${*}.sh 

clean:
	rm -f *.mandoc_ascii *.core

groff-clean:
.if defined(GROFF_TARGETS)
	rm -f ${GROFF_TARGETS:C/$/.out_ascii/}
.endif

cleandir: groff-clean clean
