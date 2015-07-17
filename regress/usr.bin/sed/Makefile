#	$OpenBSD: Makefile,v 1.5 2015/07/17 20:42:03 jasper Exp $
#	$NetBSD: Makefile,v 1.1 2005/04/04 16:48:45 peter Exp $

SED?=	/usr/bin/sed

REGRESS_TARGETS= sedtest substitute hanoi math sierpinski negation \
	inplace inplace2 inplace3

sedtest:
	sh ${.CURDIR}/$@.sh ${SED} $@.out
	diff ${.CURDIR}/$@.expected $@.out

substitute:
	sh ${.CURDIR}/$@.sh

hanoi:
	${SED} -f ${.CURDIR}/$@.sed ${.CURDIR}/$@.in > $@.out
	diff ${.CURDIR}/$@.expected $@.out

math:
	${SED} -f ${.CURDIR}/$@.sed ${.CURDIR}/$@.in > $@.out
	diff ${.CURDIR}/$@.expected $@.out

sierpinski:
	${SED} -nf ${.CURDIR}/$@.sed ${.CURDIR}/$@.in > $@.out
	diff ${.CURDIR}/$@.expected $@.out

negation:
	echo foo | ${SED} "1!!s/foo/bar/" > $@.out
	diff ${.CURDIR}/$@.expected $@.out

inplace:
	cp -f ${.CURDIR}/$@.in $@.txt
	${SED} -i -f ${.CURDIR}/sierpinski.sed $@.txt
	diff ${.CURDIR}/sierpinski.expected $@.txt

inplace2:
	cp -f ${.CURDIR}/$@.in $@.txt
	${SED} -i 's/PROG/GROP/g' $@.txt
	diff ${.CURDIR}/$@.expected $@.txt

inplace3:
	cp -f ${.CURDIR}/$@.in $@.txt
	${SED} -i 's/#PermitRootLogin no/PermitRootLogin yes/g' $@.txt
	diff ${.CURDIR}/$@.expected $@.txt

CLEANFILES+=*.out lines* script* *.txt

.include <bsd.regress.mk>
