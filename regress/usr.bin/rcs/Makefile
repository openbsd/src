#	$OpenBSD: Makefile,v 1.41 2008/02/02 16:23:44 xsa Exp $

# Regression tests by Niall O'Higgins <niallo@openbsd.org>.
# ksh -> Makefile by Ray Lai <ray@cyth.net>.

CI?=		ci
CO?=		co
MERGE?=		merge
RCS?=		rcs
RCSCLEAN?=	rcsclean
RCSDIFF?=	rcsdiff
RCSMERGE?=	rcsmerge
RLOG?=		rlog

DIFF=		diff -u

CLEANFILES=	RCS blah.c blah.c,v description file1 file2 file3 \
		file file,v newfile newfile,v test test,v \
		test-truncated truncated.out foo foo,v bar bar,v

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
	rcsclean \
	rcsdiff \
	rcsdiff-symbols \
	merge-eflag \
	rcsmerge \
	rcsmerge-symbols \
	ci-dflag \
	ci-xflag \
	comma \
	rcs-aflag \
	rcs-eflag \
	rcs-Aflag \
	rcs-tflag-stdin \
	rcs-tflag-stdin2 \
	rcs-tflag-stdin3 \
	rcs-tflag-inline \
	rcs-tflag-file \
	rcs-oflag \
	rcs-lock-unlock \
	co-lock-filemodes \
	co-unlock-filemodes \
	ci-filemodes \
	rcs-iflag \
	rlog-lflag \
	rlog-rflag \
	ci-nofile \
	ci-revert \
	ci-keywords \
	ci-keywords2 \
	ci-parse-keywords \
	ci-parse-keywords2 \
	co-parse-truncated \
	ci-2files \

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
	    ${DIFF} ${.CURDIR}/rcsdiff-uflag.out -

# Testing 'rcsdiff -u -r1.2 test'
test-rcsdiff-rflag:
	@${RCSDIFF} -q -u -r1.2 test | tail -n +3 |	\
	    ${DIFF} ${.CURDIR}/rcsdiff-rflag.out -

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

test-rcsclean: clean
	@touch file
	@${RCSCLEAN} -q file
	@${RCSCLEAN} -q1.1 file
	@${RCSCLEAN} -qsym file
	@test -f file

	@echo . | ${CI} -q -nsym file
	@${CO} -q file
	@${RCSCLEAN} -q file
	@test ! -f file
	@${CO} -q file
	@${RCSCLEAN} -q1.1 file
	@test ! -f file
	@${CO} -q file
	@${RCSCLEAN} -qsym file
	@test ! -f file

	@${CO} -q -l file
	@${RCSCLEAN} -q file
	@test -f file
	@${RCSCLEAN} -q -u file
	@test ! -f file
	@${CO} -q -l file
	@echo change >> file
	@${RCSCLEAN} -q file
	@${RCSCLEAN} -q -u file
	@test -f file

test-merge-eflag: clean
	@echo "line1\nline2\nfile1new" > file1
	@echo "line1\nline2" > file2
	@echo "line1\nfile3new\nline2" > file3
	@${MERGE} -p -q -e file1 file2 file3 | \
	    ${DIFF} ${.CURDIR}/merge-eflag.out -

test-rcsmerge:
	@cp -f ${.CURDIR}/rev1 blah.c
	@echo "descr" | ${CI} -q -l -m"first rev" blah.c
	@cp -f ${.CURDIR}/rev2 blah.c
	@${CI} -q -l -m"second rev" blah.c
	@cp -f ${.CURDIR}/rev3 blah.c
	@${CI} -q -l -m"third rev" blah.c

	@${RCSMERGE} -q -r1.1 -r1.3 -p blah.c |				\
	    ${DIFF} ${.CURDIR}/rcsmerge.out -

test-rcsmerge-symbols: clean
	@mkdir RCS
	@cp -f ${.CURDIR}/rev1 blah.c
	@echo "descr" | ${CI} -q -l -nsym1 -m"first rev" blah.c
	@cp -f ${.CURDIR}/rev2 blah.c
	@${CI} -q -l -nsym2 -m"second rev" blah.c
	@cp -f ${.CURDIR}/rev3 blah.c
	@${CI} -q -l -nsym3 -m"third rev" blah.c

	@${RCSMERGE} -q -rsym1 -rsym3 -p blah.c |		\
	    ${DIFF} ${.CURDIR}/rcsmerge.out -

test-rcsdiff:
	@rm -rf blah.c,v

	@cp -f ${.CURDIR}/rev1 blah.c
	@echo "descr" | ${CI} -q -l -m"first rev" blah.c
	@cp -f ${.CURDIR}/rev2 blah.c
	@${CI} -q -l -m"second rev" blah.c
	@cp -f ${.CURDIR}/rev3 blah.c
	@${CI} -q -l -m"third rev" blah.c

	@${RCSDIFF} -q -r1.1 -r1.3 -u blah.c | tail -n +3 |		\
	    ${DIFF} ${.CURDIR}/rcsdiff.out -

test-rcsdiff-symbols: clean
	@mkdir RCS
	@cp -f ${.CURDIR}/rev1 blah.c
	@echo "descr" | ${CI} -q -l -nsym1 -m"first rev" blah.c
	@cp -f ${.CURDIR}/rev2 blah.c
	@${CI} -q -l -nsym2 -m"second rev" blah.c
	@cp -f ${.CURDIR}/rev3 blah.c
	@${CI} -q -l -nsym3 -m"third rev" blah.c

	@${RCSDIFF} -q -rsym1 -rsym3 -u blah.c | tail -n +3 |		\
	    ${DIFF} ${.CURDIR}/rcsdiff.out -

# Testing 'ci -d'2037-01-12 04:00:00+00' -l test
test-ci-dflag: clean
	@echo "some text for date test" >> test
	@echo . | ${CI} -q -d'2037-01-12 04:00:00+00' -m'dated revision' -l test
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

	@touch file
	@echo more >> file
	@echo . | ${CI} -q -x file
	@fgrep -q more RCS/file

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
	@${RLOG} test | ${DIFF} ${.CURDIR}/rcs-aflag.out -

# Testing 'rcs -efoo,bar,baz'
test-rcs-eflag: test-rcs-aflag
	@${RCS} -q -efoo,bar,baz test
	@${RLOG} test | ${DIFF} ${.CURDIR}/rcs-eflag.out -

# Testing  'rcs -Atest newfile'
test-rcs-Aflag: test-rcs-aflag
	@echo "." | ${RCS} -q -i newfile
	@${RCS} -q -Atest newfile
	@${RLOG} newfile | ${DIFF} ${.CURDIR}/rcs-Aflag.out -

test-rcs-tflag-stdin: clean
	@echo 'This is a description.' | ${RCS} -q -i -t file
	@fgrep -q 'This is a description.' file,v

test-rcs-tflag-stdin2: clean
	@echo '.This is not the description end.' | ${RCS} -q -i -t file
	@fgrep -q '.This is not the description end.' file,v

test-rcs-tflag-stdin3: clean
	@echo "This is the description end.\n.\nThis should not be here." | \
	    ${RCS} -q -i -t file
	@fgrep -q 'This should not be here.' file,v || \
	    case "$$?" in 1) exit 0;; esac && exit 1

test-rcs-tflag-inline: clean
	@${RCS} -q -i '-t-This is a description.' file
	@fgrep -q 'This is a description.' file,v

test-rcs-tflag-file: clean
	@echo 'This is a description.' > description
	@${RCS} -q -i -tdescription file
	@fgrep -q 'This is a description.' file,v

# Testing deletion of ranges
test-rcs-oflag: clean
	@cp ${.CURDIR}/rev3 blah.c
	@echo "blah" | ${CI} -q blah.c
	@${CO} -q -l blah.c
	@echo "blah2" >> blah.c
	@echo "blah2" | ${CI} -q blah.c
	@${CO} -q -l blah.c
	@echo "blah3" >> blah.c
	@echo "blah3" | ${CI} -q blah.c
	@${CO} -q -l blah.c
	@echo "blah4" >> blah.c
	@echo "blah4" | ${CI} -q blah.c
	@${CO} -q -l blah.c
	@echo "blah5" >> blah.c
	@echo "blah5" | ${CI} -q blah.c
	@${CO} -q -l blah.c
	@echo "blah6" >> blah.c
	@echo "blah6" | ${CI} -q blah.c
	@${CO} -q -l blah.c
	@${RCS} -q -o1.3:1.5 blah.c
	@tr '\n' ' ' < blah.c,v | grep -q '[[:space:]]1.3[[:space:]]' || \
	    case "$$?" in 1) exit 0;; esac && exit 1
	@tr '\n' ' ' < blah.c,v | grep -q '[[:space:]]1.4[[:space:]]' || \
	    case "$$?" in 1) exit 0;; esac && exit 1
	@tr '\n' ' ' < blah.c,v | grep -q '[[:space:]]1.5[[:space:]]' || \
	    case "$$?" in 1) exit 0;; esac && exit 1
	
test-rcs-lock-unlock: clean
	@touch file
	@mkdir -p RCS
	@echo . | ${CI} -q -l file
	@echo sometext > file
	@echo . | ${CI} -q file

	@${RCS} -q -l file
	@${RLOG} file | fgrep -x -A 1 'locks: strict' | head -n 2 | fgrep -q 1.2
	@${RCS} -q -u file
	@${RLOG} file | fgrep -x -A 1 'locks: strict' | head -n 2 | fgrep -q 1.2 || \
	    case "$$?" in 1) exit 0;; esac && exit 1

	@${RCS} -q -l1.1 file
	@${RLOG} file | fgrep -x -A 1 'locks: strict' | head -n 2 | fgrep -q 1.1
	@${RCS} -q -u1.1 file
	@${RLOG} file | fgrep -x -A 1 'locks: strict' | head -n 2 | fgrep -q 1.1 || \
	    case "$$?" in 1) exit 0;; esac && exit 1

	@${RCS} -q -l1.2 file
	@${RLOG} file | fgrep -x -A 1 'locks: strict' | head -n 2 | fgrep -q 1.2
	@${RCS} -q -u1.2 file
	@${RLOG} file | fgrep -x -A 1 'locks: strict' | head -n 2 | fgrep -q 1.2 || \
	    case "$$?" in 1) exit 0;; esac && exit 1

	@${RCS} -q -u file
	@${RCS} -q -l file
	@${RCS} -q -l file
	@${RCS} -q -l1.3 file || case "$$?" in 1) exit 0;; esac && exit 1
	@${RCS} -q -u1.3 file || case "$$?" in 1) exit 0;; esac && exit 1

# Testing 'co -l blah.c' for permissions inheritance
test-co-lock-filemodes:
	@rm -rf RCS/blah.c,v blah.c
	@mkdir -p RCS
	@cp ${.CURDIR}/rev3 blah.c
	@chmod 444 blah.c
	@echo "blah" | ${CI} -q blah.c
	@chmod 755 RCS/blah.c,v
	@${CO} -q -l blah.c
	@eval 'test `stat -f%p blah.c` = 100755'
	@rm -rf blah.c
	@chmod 666 RCS/blah.c,v
	@${CO} -q -l blah.c
	@eval 'test `stat -f%p blah.c` = 100644'
	@rm -rf blah.c
	@chmod 600 RCS/blah.c,v
	@${CO} -q -l blah.c
	@eval 'test `stat -f%p blah.c` = 100600'
	@rm -rf blah.c
	@chmod 604 RCS/blah.c,v
	@${CO} -q -l blah.c
	@eval 'test `stat -f%p blah.c` = 100604'
	@rm -rf blah.c
	@chmod 754 RCS/blah.c,v
	@${CO} -q -l blah.c
	@eval 'test `stat -f%p blah.c` = 100754'
	

# Testing 'co -u blah.c' for permissions inheritance
test-co-unlock-filemodes: test-co-lock-filemodes
	@rm -rf RCS/blah.c,v blah.c
	@mkdir -p RCS
	@cp ${.CURDIR}/rev3 blah.c
	@echo "blah" | ${CI} -q blah.c
	@chmod 755 RCS/blah.c,v
	@${CO} -q -u blah.c
	@eval 'test `stat -f%p blah.c` = 100555'
	@rm -rf blah.c
	@chmod 666 RCS/blah.c,v
	@${CO} -q -u blah.c
	@eval 'test `stat -f%p blah.c` = 100444'
	@rm -rf blah.c
	@chmod 600 RCS/blah.c,v
	@${CO} -q -u blah.c
	@eval 'test `stat -f%p blah.c` = 100400'
	@rm -rf blah.c
	@chmod 604 RCS/blah.c,v
	@${CO} -q -u blah.c
	@eval 'test `stat -f%p blah.c` = 100404'
	@rm -rf blah.c
	@chmod 754 RCS/blah.c,v
	@${CO} -q -u blah.c
	@eval 'test `stat -f%p blah.c` = 100554'

# Testing 'ci blah.c' for permissions inheritance
test-ci-filemodes: test-co-unlock-filemodes
	@rm -rf RCS/blah.c,v blah.c
	@mkdir -p RCS
	@cp ${.CURDIR}/rev3 blah.c
	@chmod 755 blah.c
	@echo "blah" | ${CI} -q blah.c
	@eval 'test `stat -f%p RCS/blah.c,v` = 100555'
	@rm -rf RCS/blah.c,v blah.c
	@cp ${.CURDIR}/rev3 blah.c
	@chmod 666 blah.c
	@echo "blah" | ${CI} -q blah.c
	@eval 'test `stat -f%p RCS/blah.c,v` = 100444'
	@rm -rf RCS/blah.c,v blah.c
	@cp ${.CURDIR}/rev3 blah.c
	@chmod 700 blah.c
	@echo "blah" | ${CI} -q blah.c
	@eval 'test `stat -f%p RCS/blah.c,v` = 100500'
	@rm -rf RCS/blah.c,v blah.c
	@cp ${.CURDIR}/rev3 blah.c
	@chmod 606 blah.c
	@echo "blah" | ${CI} -q blah.c
	@eval 'test `stat -f%p RCS/blah.c,v` = 100404'

# Test various operations on a file with no revisions.
test-rcs-iflag: clean
	@mkdir -p RCS
	@echo . | ${RCS} -i -q file
	@test -f RCS/file,v
	@${CO} -q file
	@test -f file
	@test ! -s file
	@rm -f file
	@${CO} -q -l file
	@echo text >> file
	@${CI} -q file
	@fgrep -q 1.1 RCS/file,v

test-rlog-lflag: clean
	@mkdir RCS
	@touch file
	@echo rev1 | ${CI} -q -l file
	@${RLOG} -l file | fgrep -q 'revision 1.1'
	@echo line >> file
	@echo rev2 | ${CI} -q file
	@${RLOG} -l file | fgrep -q 'revision 1.2' ||	\
	    case "$$?" in 1) exit 0;; esac && exit 1

# Test various cases for the -r flag
test-rlog-rflag: clean
	@touch file
	@echo "foo" > file
	@echo "descr" | ${CI} -q -m"first rev" -d'2006-01-01 00:00:00+00' \
		-wfoo file
	@${CO} -q -l file
	@echo "foo" >> file
	@${CI} -q -m"second rev" -d'2006-01-01 00:00:00+00' -wfoo file
	@${CO} -q -l file
	@echo "foo" >> file
	@${CI} -q -m"third rev" -d'2006-01-01 00:00:00+00' -wfoo file

	@${RLOG} -r1.1 file | ${DIFF} ${.CURDIR}/rlog-rflag1.out -
	@${RLOG} -r1.1:1.3 file | ${DIFF} ${.CURDIR}/rlog-rflag2.out -
	@${RLOG} -r1.2: file | ${DIFF} ${.CURDIR}/rlog-rflag3.out -
	@${RLOG} -r:1.1 file | ${DIFF} ${.CURDIR}/rlog-rflag4.out -

test-rlog-zflag: clean
	@touch file
	@echo "descr" | ${CI} -q -m"first rev" -d'2006-01-01 00:00:00+00' \
		-wfoo file
	@${RLOG} -zLT file | ${DIFF} ${.CURDIR}/rlog-zflag1.out -
	@${RLOG} -z+03:14:23 file | ${DIFF} ${.CURDIR}/rlog-zflag2.out -
	@${RLOG} -z+03:14 file | ${DIFF} ${.CURDIR}/rlog-zflag3.out -
	@${RLOG} -z+0314 file | ${DIFF} ${.CURDIR}/rlog-zflag4.out -
	@${RLOG} -z-03:14:23 file | ${DIFF} ${.CURDIR}/rlog-zflag5.out -

test-ci-nofile:
	@${CI} -q nonexistent || case "$$?" in 1) exit 0;; esac && exit 1

test-ci-revert: clean
	@mkdir RCS
	@touch file
	@echo . | ${CI} -q -l file
	@${CI} -q -mm -l file
	@# Make sure reverting doesn't unlock file.
	@${CI} -q -mm -l file

test-ci-keywords: clean
	@mkdir RCS
	@sed 's/.*/$$&$$/' ${.CURDIR}/keywords.in > file
	@sed 's/^[A-Z][A-Z]*[a-z][a-z]*: .*/$$&$$/' ${.CURDIR}/keywords.out > newfile
	@echo . | ${CI} -q -u file
	@sed -e 's,${USER},USER,' -e "s/\($$[A-Z][a-z]*: \).*file,v/\1file,v/" \
	    -e 's,[0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9],YYYY/MM/DD HH:MI:SS,' \
	    file | ${DIFF} newfile -

# Lots of expansion.
test-ci-keywords2: clean
	@perl -e 'print "\$$Id\$$\n" x 10000;' > file
	@echo . | ${CI} -l -q file

test-ci-parse-keywords: clean
	@echo '$Id' > test
	@echo . | ${CI} -q -k test

test-ci-parse-keywords2: clean
	@echo '$Id: blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah' > test 
	@echo . | ${CI} -q -k test

# Check for correct EOF handling in rcs parser
test-co-parse-truncated: clean
	@sh -c 'ulimit -d 5000 && ${CO} -q ${.CURDIR}/test-truncated > truncated.out 2>&1' || \
	    case "$$?" in 1) exit 0;; esac && exit 1
	@grep -q 'co: problem parsing deltatexts' truncated.out

test-ci-2files: clean
	@touch foo bar
	@${CI} -q -t-first -l foo
	@test -f foo,v -a ! -f bar,v
	@${CI} -q -t-second -l foo bar
	@test -f foo,v -a -f bar,v

clean:
	@rm -rf ${CLEANFILES}

.include <bsd.regress.mk>
