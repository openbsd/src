# $OpenBSD: Makefile.inc,v 1.6 2011/07/24 20:26:52 schwarze Exp $

DIFF=diff -au
MANDOC?=mandoc
#MANDOCOPTS=-Wall
NROFF=/usr/local/bin/nroff -c -mandoc -Tascii
SED=sed
SH=/bin/sh

CLEANFILES=*.mandoc_ascii

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
