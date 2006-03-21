#	$OpenBSD: Makefile,v 1.8 2006/03/21 08:32:10 xsa Exp $

# Regression tests by Niall O'Higgins <niallo@openbsd.org>.
# ksh -> Makefile by Ray Lai <ray@cyth.net>.

CI?=		ci
CO?=		co
RCS?=		rcs
RCSDIFF?=	rcsdiff
RCSMERGE?=	rcsmerge
RLOG?=		rlog

CLEANFILES=	blah.c blah.c,v test test,v RCS

# XXX - These may need to be done in order.
#	(At least start with ci-initial.)
LTESTS= ci-initial \
	ci-mflag \
	ci-lflag \
	ci-rflag \
	co-lflag \
	ci-rev \
	co-perm \
	co-perm2 \
	ci-perm \
	ci-perm2 \
	ci-dinvalid \
	ci-dold \
	ci-wflag \
	rcsdiff-uflag \
	rcsdiff-rflag \
	rcs-mflag \
	rcs-mflag2 \
	co-RCSINIT \
	ci-nflag \
	ci-Nflag \
	ci-sflag \
	co-lflag2 \
	rcsdiff \
	rcsmerge \
	ci-dflag \
	ci-xflag \
	comma

.for t in ${LTESTS}
REGRESS_TARGETS+=test-${t}
.endfor

test-ci-initial: clean
	@echo 'this is a test file' > test
	@echo "a test file" | ${CI} -q -l test
	@grep -q 'this is a test file' test,v

# Testing 'ci test' with non-interactive log message
test-ci-mflag:
	@echo 'another revision' >> test
	@${CI} -q -m'a second revision' test
	@test ! -e test
	@grep -q 'another revision' test,v

# Testing 'co -l test'
test-ci-lflag:
	@${CO} -q -l test
	@test -e test

# Testing 'ci -r1.30 test' with non-interactive log message
test-ci-rflag:
	@echo "new stuff" >> test
	@${CI} -q -r1.30 -m'bumped rev' test
	@test ! -e test

# Testing 'co -l test'
test-co-lflag:
	@${CO} -q -l test
	@test -e test

# Testing 'ci test' (should be rev 1.31) with non-interactive log message
test-ci-rev:
	@echo "a third revision" >> test
	@${CI} -q -m'this should be rev 1.31' test
	@grep -q '1.31' test,v

# Testing 'co -u test' - ensuring permissions are 0444
test-co-perm:
	@${CO} -q -u test
	@eval 'test `stat -f%p test` = 100444'

# Testing 'co -l test' - ensuring permissions are 0644
test-co-perm2:
	@rm -rf test
	@${CO} -q -l test
	@eval 'test `stat -f%p test` = 100644'

# Testing 'ci -u' - ensuring permissions are 0444
test-ci-perm:
	@echo "a line for ci -u" >> test
	@${CI} -q -m'message for ci -u' -u test
	@eval 'test `stat -f%p test` = 100444'

# Testing 'ci -l' - ensuring permissions are 0644
test-ci-perm2:
	@rm -rf test
	@${CO} -q -l test
	@echo "a line for ci -l" >> test
	@${CI} -q -m'message for ci -l' -l test
	@eval 'test `stat -f%p test` = 100644'

# Testing ci with an invalid date
test-ci-dinvalid:
	@echo 'some text for invalid date text' >> test
	@if @${CI} -q -d'an invalid date' -m'invalid date' -l test 2>/dev/null; then false; fi
	@if grep 'some text for invalid date text' test,v; then false; fi

# Testing ci with a date older than previous revision
test-ci-dold:
	@echo 'some text for old date test' >> test
	@if ${CI} -q -d'1990-01-12 04:00:00+00' -m'old dated revision' -l test 2>/dev/null; then false; fi
	@if grep 'some text for old date test' test,v; then false; fi

# Testing ci -wtestuser
test-ci-wflag:
	@rm -rf test
	@${CO} -q -l test
	@echo "blah blah" >> test
	@echo "output for ci -w" >> test
	@${CI} -q -wtestuser -mcomment -l test
	@grep -q 'author testuser' test,v

# Testing 'rcsdiff -u test' after adding another line
test-rcsdiff-uflag:
	@echo "a line for rcsdiff test" >> test
	@${RCSDIFF} -q -u test | tail -n 5 |		\
	    diff -u ${.CURDIR}/rcsdiff-uflag.out -

# Testing 'rcsdiff -u -r1.2 test'
test-rcsdiff-rflag:
	@${RCSDIFF} -q -u -r1.2 test | tail -n +3 |	\
	    diff -u ${.CURDIR}/rcsdiff-rflag.out -

# Testing 'rcs -m1.2:logmessage'
test-rcs-mflag:
	@${RCS} -q -m1.2:logmessage test
	@grep -q 'logmessage' test,v

# Testing 'rcs -m'1.2:a new log message''
test-rcs-mflag2:
	@${RCS} -q -m1.1:'a new log message, one which is quite long and set by rcsprog' test
	@grep -q 'a new log message, one which is quite long and set by rcsprog' test,v

# Testing RCSINIT environment variable
test-co-RCSINIT:
	@rm -rf test
	@RCSINIT=-l ${CO} -q test
	@eval 'test `stat -f%p test` = 100644'

# Testing check-in with symbol
test-ci-nflag:
	@echo "something to check in with a symbol" >> test
	@${CI} -q -n'symbolname' -m'test symbols' -l test
	@grep -q 'symbolname' test,v

# Testing check-in, forcing symbol
test-ci-Nflag:
	@echo "something to check in with a forced symbol" >> test
	@${CI} -q -N'symbolname' -m'test force symbol' -l test
	@grep -q 'test force symbol' test,v

# Trying some jiggerypokery with state
test-ci-sflag:
	@echo "blahblah" >>  test
	@if ${CI} -q -l -s'SPACE S' -m"state with a space" test; then false; fi
	@if grep -q 'SPACE S' test,v; then false; fi

# Trying to check it out
test-co-lflag2:
	@rm -rf test
	@${CO} -q -l test
	@test -e test

test-rcsmerge:
	@cp -f ${.CURDIR}/rev1 blah.c
	@echo "descr" | ${CI} -q -l -m"first rev" blah.c
	@cp -f ${.CURDIR}/rev2 blah.c
	@${CI} -q -l -m"second rev" blah.c
	@cp -f ${.CURDIR}/rev3 blah.c
	@${CI} -q -l -m"third rev" blah.c

	@${RCSMERGE} -q -r1.1 -r1.3 -p blah.c |				\
	    diff -u ${.CURDIR}/rcsmerge.out -

test-rcsdiff:
	@rm -rf blah.c,v

	@cp -f ${.CURDIR}/rev1 blah.c
	@echo "descr" | ${CI} -q -l -m"first rev" blah.c
	@cp -f ${.CURDIR}/rev2 blah.c
	@${CI} -q -l -m"second rev" blah.c
	@cp -f ${.CURDIR}/rev3 blah.c
	@${CI} -q -l -m"third rev" blah.c

	@${RCSDIFF} -q -r1.1 -r1.3 -u blah.c | tail -n +3 |		\
	    diff -u ${.CURDIR}/rcsdiff.out -

# Testing 'ci -d'2037-01-12 04:00:00+00' -l test
test-ci-dflag:
	@echo "some text for date test" >> test
	@${CI} -q -d'2037-01-12 04:00:00+00' -m'dated revision' -l test
	@grep -q 'dated revision' test,v

test-ci-xflag:
	@mkdir -p RCS
	@rm -rf RCS/file*
	@touch file
	@echo . | ${CI} -q -x,abcd/,v file
	@test -e RCS/file,abcd
	@test ! -e RCS/file,v

	@mv -f RCS/file,abcd RCS/file,v
	@${CO} -q -l file
	@echo revision >> file
	@echo . | ${CI} -q -x,abcd/,v/xyz file
	@test ! -e RCS/file,abcd
	@fgrep -q revision RCS/file,v
	@test ! -e RCS/filexyz

	@# XXX - Coming soon
	@#touch file
	@#echo more >> file
	@#${CI} -q -x file
	@#fgrep -q more RCS/file

test-comma:
	@rm -rf RCS
	@mkdir -p RCS
	@touch file,notext
	@echo . | ${CI} -q file,notext
	@test -e RCS/file,notext,v
	@test ! -e RCS/file,v

# Testing 'rcs -afoo,bar,baz'
test-rcs-aflag: clean
	@echo "." | ${RCS} -q -i test
	@${RCS} -q -afoo,bar,baz test
	@${RLOG} test | diff -u ${.CURDIR}/rcs-aflag.out -

# Testing 'rcs -efoo,bar,baz'
test-rcs-eflag: test-rcs-aflag
	@${RCS} -q -efoo,bar,baz test
	@${RLOG} test | diff -u ${.CURDIR}/rcs-eflag.out -

clean:
	@rm -rf ${CLEANFILES}

.include <bsd.regress.mk>
