#	$OpenBSD: Makefile,v 1.2 2017/08/23 14:09:39 bluhm Exp $

# If you are unsure whether the output of ctfdump(1) is correct, you can
# install ctftools from ports and set CTFDUMP=/usr/local/bin/ctfdump.
CTFDUMP?=		/usr/bin/ctfdump
CTFSTRIP?=		/usr/bin/ctfstrip

PROG=			example
DEBUG+=			-g

REGRESS_TARGETS=	base_types_encoding

# Test the proper encoding and display of base types in the C language.
base_types_encoding: ${PROG}.ctf
	${MAKE} -C ${.CURDIR} run-${PROG} RT=$@ RE=' (INTEGER|FLOAT) '

# Run ctfdump(1) and massage the output to make it stable.  ${RT} is the
# original test target; ${RE} reduces the output to the relevant lines.
run-${PROG}: ${PROG}.ctf
	${CTFDUMP} ${PROG}.ctf 2>&1 | sed 's/\[[0-9]*\]/[]/' | sort | egrep '${RE}' >${RT}.out
.if exists(${RT}.${MACHINE})
	diff -u ${.CURDIR}/${RT}.${MACHINE} ${RT}.out
	@echo ok - ${RT}
.else
	# ${RT} for machine ${MACHINE} not found
	@echo SKIPPED
.endif

${PROG}.ctf: ${PROG}
	${CTFSTRIP} -o ${PROG}.ctf ${PROG}

CLEANFILES+= ${PROG}.ctf *.out

.include <bsd.regress.mk>
