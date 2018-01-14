# $OpenBSD: Makefile,v 1.1.1.1 2018/01/14 22:01:48 bluhm Exp $

ED ?=		/bin/ed
TESTDIR =	${.CURDIR}/../../../bin/ed/test

TS !=		cd ${TESTDIR} && ls *.t
ERRS !=		cd ${TESTDIR} && ls *.err
CLEANFILES =	*.t *.d *.ed *.err *.red *.o !.z stamp-*

run-regress-i3.red run-regress-nl.red run-regress-s2.red:
	@echo '\n======== $t.red ========'
	# This test currently fails, but it passes with rev 1.1 of ed.
	# The regression should be investigated.
	@echo DISABLED

.for t in ${TS:R}
# from /usr/src/bin/ed/test/mkscripts.sh, converted to make rule
$t.ed: ${TESTDIR}/$t.t ${TESTDIR}/$t.d Makefile
	@echo '\n======== setup $t.ed ========'
	cp ${TESTDIR}/$t.t ${TESTDIR}/$t.d .
	( \
	    echo "#!/bin/sh -"; \
	    echo "${ED} - <<\EOT"; \
	    echo H; \
	    echo "r $t.d"; \
	    cat $t.t; \
	    echo "w $t.o"; \
	    echo EOT; \
	) >$t.tmp
	mv $t.tmp $t.ed
	chmod +x $t.ed

REGRESS_TARGETS +=	run-regress-$t.ed
# from /usr/src/bin/ed/test/ckscripts.sh, converted to make rule
run-regress-$t.ed: $t.ed
	@echo '\n======== $t.ed ========'
	./$t.ed
	cmp -s $t.o ${TESTDIR}/$t.r
.endfor

.for t in ${ERRS:R}
# from /usr/src/bin/ed/test/mkscripts.sh, converted to make rule
$t.red: ${TESTDIR}/$t.err Makefile
	@echo '\n======== setup $t.red ========'
	cp ${TESTDIR}/$t.err .
	( \
	    echo "#!/bin/sh -"; \
	    echo "${ED} - <<\EOT"; \
	    echo H; \
	    echo "r $t.err"; \
	    cat $t.err; \
	    echo "w $t.o"; \
	    echo EOT; \
	) >$t.tmp
	mv $t.tmp $t.red
	chmod +x $t.red

REGRESS_TARGETS +=	run-regress-$t.red
# from /usr/src/bin/ed/test/ckscripts.sh, converted to make rule
run-regress-$t.red: $t.red
	@echo '\n======== $t.red ========'
	! ./$t.red
.endfor

# from /usr/src/bin/ed/test/README, known failing tests
REGRESS_SKIP_TARGETS+=	run-regress-eq.red \
			run-regress-a1.red \
			run-regress-i1.red \
			run-regress-k1.red \
			run-regress-r1.red

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
