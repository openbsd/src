# $OpenBSD: regress.m4,v 1.1 2016/07/30 13:55:54 tb Exp $
# $FreeBSD: head/usr.bin/tests/regress.m4 263227 2014-03-16 08:04:06Z jmmv $

dnl Originally /usr/src/usr.bin/tests/regress.m4 on FreeBSD
dnl Merged into jot tests for OpenBSD by attila@stalphonsos.com

dnl A library of routines for doing regression tests for userland utilities.

dnl Start up.  We initialise the exit status to 0 (no failure) and change
dnl into the directory specified by our first argument, which is the
dnl directory to run the tests inside.

dnl We need backticks and square brackets, use [[ ]] for m4 quoting
changequote([[,]])

dnl Set some things up before we start running tests
define([[REGRESSION_START]],
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

TOTAL=0
NFAILED=0
FAILED=""
STATUS=0
)

dnl Check $? to see if we passed or failed.  The first parameter is the test
dnl which passed or failed.  It may be nil.
define([[REGRESSION_PASSFAIL]],
if [ $? -eq 0 ]; then
  echo "ok - $1 # Test detected no regression. (in $TESTDIR)"
else
  STATUS=$?
  NFAILED=`expr 1 + ${NFAILED}`
  [ -n "${FAILED}" ] && FAILED="${FAILED} "
  FAILED="${FAILED}$1"
  SEE_ABOVE=""
  if [ ${VERBOSE-0} != 0 ]; then
    diff -u ${SRCDIR:-.}/regress.$1.out ./test.$1.out
    SEE_ABOVE="See above. "
  fi
  echo "not ok - $1 # Test failed: regression detected. ${SEE_ABOVE}(in $TESTDIR)"
fi)

dnl An actual test.  The first parameter is the test name.  The second is the
dnl command/commands to execute for the actual test.  Their exit status is
dnl checked.  It is assumed that the test will output to stdout, and that the
dnl output to be used to check for regression will be in regress.TESTNAME.out.
define([[REGRESSION_TEST]],
TOTAL=`expr 1 + ${TOTAL}`
$2 >test.$1.out
diff -q ${SRCDIR:-.}/regress.$1.out ./test.$1.out >/dev/null
REGRESSION_PASSFAIL($1))

dnl Cleanup.  Exit with the status code of the last failure.  Should probably
dnl be the number of failed tests, but hey presto, this is what it does.  This
dnl could also clean up potential droppings, if some forms of regression tests
dnl end up using mktemp(1) or such.
define([[REGRESSION_END]],
if [ ${NFAILED} -ne 0 ]; then
  echo "FAILED ${NFAILED} tests out of ${TOTAL}: ${FAILED}"
else
  echo "PASSED ${TOTAL} tests"
fi
exit $STATUS)
