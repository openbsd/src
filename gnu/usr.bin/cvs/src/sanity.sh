#! /bin/sh
:
#	sanity.sh -- a growing testsuite for cvs.
#
# Copyright (C) 1992, 1993 Cygnus Support
#
# Original Author: K. Richard Pixley

# usage: sanity.sh [-r] @var{cvs-to-test} @var{tests-to-run}
# -r means to test remote instead of local cvs.
# @var{tests-to-run} are the names of the tests to run; if omitted run all
# tests.

# See TODO list at end of file.

# required to make this script work properly.
unset CVSREAD

TESTDIR=/tmp/cvs-sanity
# This will show up in cvs history output where it prints the working
# directory.  It should *not* appear in any cvs output referring to the
# repository; cvs should use the name of the repository as specified.
TMPPWD=`cd /tmp; /bin/pwd`

# "debugger"
#set -x

echo 'This test should produce no other output than this line, and a final "OK".'

if test x"$1" = x"-r"; then
	shift
	remote=yes
	# If we're going to do remote testing, make sure 'rsh' works first.
        host="`hostname`"
	if test "x`${CVS_RSH-rsh} $host 'echo hi'`" != "xhi"; then
	    echo "ERROR: cannot test remote CVS, because \`rsh $host' fails." >&2
	    exit 1
	fi
else
	remote=no
fi

# The --keep option will eventually cause all the tests to leave around the
# contents of the /tmp directory; right now only some implement it.  Not
# useful if you are running more than one test.
# FIXME: need some real option parsing so this doesn't depend on the order
# in which they are specified.
if test x"$1" = x"--keep"; then
  shift
  keep=yes
else
  keep=no
fi

# Use full path for CVS executable, so that CVS_SERVER gets set properly
# for remote.
case $1 in
/*)
	testcvs=$1
	;;
*)
	testcvs=`pwd`/$1
	;;
esac

shift

# Regexp to match what CVS will call itself in output that it prints.
# FIXME: we don't properly quote this--if the name contains . we'll
# just spuriously match a few things; if the name contains other regexp
# special characters we are probably in big trouble.
PROG=`basename ${testcvs}`

# FIXME: try things (what things? checkins?) without -m.
#
# Some of these tests are written to expect -Q.  But testing with
# -Q is kind of bogus, it is not the way users actually use CVS (usually).
# So new tests probably should invoke ${testcvs} directly, rather than ${CVS}.
# and then they've obviously got to do something with the output....
#
CVS="${testcvs} -Q"

LOGFILE=`pwd`/check.log

# Save the previous log in case the person running the tests decides
# they want to look at it.  The extension ".plog" is chosen for consistency
# with dejagnu.
if test -f check.log; then
	mv check.log check.plog
fi

GEXPRLOCS="`echo $PATH | sed 's/:/ /g'` /usr/local/bin /usr/contrib/bin /usr/gnu/bin /local/bin /local/gnu/bin /gun/bin"

EXPR=expr

# Cause NextStep 3.3 users to lose in a more graceful fashion.
if $EXPR 'abc
def' : 'abc
def' >/dev/null; then
  : good, it works
else
  for path in $GEXPRLOCS ; do
    if test -x $path/gexpr ; then
      if test "X`$path/gexpr --version`" != "X--version" ; then
        EXPR=$path/gexpr
        break
      fi
    fi
    if test -x $path/expr ; then
      if test "X`$path/expr --version`" != "X--version" ; then
        EXPR=$path/expr
        break
      fi
    fi
  done
  if test -z "$EXPR" ; then
    echo 'Running these tests requires an "expr" program that can handle'
    echo 'multi-line patterns.  Make sure that such an expr (GNU, or many but'
    echo 'not all vendor-supplied versions) is in your path.'
    exit 1
  fi
fi

# Warn SunOS, SysVr3.2, etc., users that they may be partially losing
# if we can't find a GNU expr to ease their troubles...
if $EXPR 'a
b' : 'a
c' >/dev/null; then
  for path in $GEXPRLOCS ; do
    if test -x $path/gexpr ; then
      if test "X`$path/gexpr --version`" != "X--version" ; then
        EXPR=$path/gexpr
        break
      fi
    fi
    if test -x $path/expr ; then
      if test "X`$path/expr --version`" != "X--version" ; then
        EXPR=$path/expr
        break
      fi
    fi
  done
  if test -z "$EXPR" ; then
    echo 'Warning: you are using a version of expr which does not correctly'
    echo 'match multi-line patterns.  Some tests may spuriously pass.'
    echo 'You may wish to make sure GNU expr is in your path.'
    EXPR=expr
  fi
else
  : good, it works
fi

# That we should have to do this is total bogosity, but GNU expr
# version 1.9.4-1.12 uses the emacs definition of "$" instead of the unix
# (e.g. SunOS 4.1.3 expr) one.  Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
ENDANCHOR="$"
if $EXPR 'abc
def' : 'abc$' >/dev/null; then
  ENDANCHOR='\'\'
fi

# Work around another GNU expr (version 1.10-1.12) bug/incompatibility.
# "." doesn't appear to match a newline (it does with SunOS 4.1.3 expr).
# Note that the workaround is not a complete equivalent of .* because
# the first parenthesized expression in the regexp must match something
# in order for expr to return a successful exit status.
# Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
DOTSTAR='.*'
if $EXPR 'abc
def' : "a${DOTSTAR}f" >/dev/null; then
  : good, it works
else
  DOTSTAR='\(.\|
\)*'
fi

# Work around yet another GNU expr (version 1.10) bug/incompatibility.
# "+" is a special character, yet for unix expr (e.g. SunOS 4.1.3)
# it is not.  I doubt that POSIX allows us to use \+ and assume it means
# (non-special) +, so here is another workaround
# Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
PLUS='+'
if $EXPR 'a +b' : "a ${PLUS}b" >/dev/null; then
  : good, it works
else
  PLUS='\+'
fi

# Likewise, for ?
QUESTION='?'
if $EXPR 'a?b' : "a${QUESTION}b" >/dev/null; then
  : good, it works
else
  QUESTION='\?'
fi

pass ()
{
  echo "PASS: $1" >>${LOGFILE}
}

fail ()
{
  echo "FAIL: $1" | tee -a ${LOGFILE}
  # This way the tester can go and see what remnants were left
  exit 1
}

# See dotest and dotest_fail for explanation (this is the parts
# of the implementation common to the two).
dotest_internal ()
{
  # expr can't distinguish between "zero characters matched" and "no match",
  # so special-case it.
  if test -z "$3"; then
    if test -s ${TESTDIR}/dotest.tmp; then
      echo "** expected: " >>${LOGFILE}
      echo "$3" >>${LOGFILE}
      echo "$3" > ${TESTDIR}/dotest.exp
      rm -f ${TESTDIR}/dotest.ex2
      echo "** got: " >>${LOGFILE}
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      fail "$1"
    else
      pass "$1"
    fi
  else
    if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : \
	"$3"${ENDANCHOR} >/dev/null; then
      pass "$1"
    else
      if test x"$4" != x; then
	if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : \
	    "$4"${ENDANCHOR} >/dev/null; then
	  pass "$1"
	else
	  echo "** expected: " >>${LOGFILE}
	  echo "$3" >>${LOGFILE}
	  echo "$3" > ${TESTDIR}/dotest.ex1
	  echo "** or: " >>${LOGFILE}
	  echo "$4" >>${LOGFILE}
	  echo "$4" > ${TESTDIR}/dotest.ex2
	  echo "** got: " >>${LOGFILE}
	  cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	  fail "$1"
	fi
      else
	echo "** expected: " >>${LOGFILE}
	echo "$3" >>${LOGFILE}
	echo "$3" > ${TESTDIR}/dotest.exp
	echo "** got: " >>${LOGFILE}
	cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	fail "$1"
      fi
    fi
  fi
}

dotest_all_in_one ()
{
  if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : \
         "`cat ${TESTDIR}/dotest.exp`" >/dev/null; then
    return 0
  fi
  return 1
}

# WARNING: this won't work with REs that match newlines....
#
dotest_line_by_line ()
{
  line=1
  while [ $line -le `wc -l ${TESTDIR}/dotest.tmp` ] ; do
    echo "$line matched \c" >>$LOGFILE
    if $EXPR "`sed -n ${line}p ${TESTDIR}/dotest.tmp`" : \
       "`sed -n ${line}p ${TESTDIR}/dotest.exp`" >/dev/null; then
      :
    else
      echo "**** expected line: " >>${LOGFILE}
      sed -n ${line}p ${TESTDIR}/dotest.exp >>${LOGFILE}
      echo "**** got line: " >>${LOGFILE}
      sed -n ${line}p ${TESTDIR}/dotest.tmp >>${LOGFILE}
      unset line
      return 1
    fi
    line=`expr $line + 1`
  done
  unset line
  return 0
}

# If you are having trouble telling which line of a multi-line
# expression is not being matched, replace calls to dotest_internal()
# with calls to this function:
#
dotest_internal_debug ()
{
  if test -z "$3"; then
    if test -s ${TESTDIR}/dotest.tmp; then
      echo "** expected: " >>${LOGFILE}
      echo "$3" >>${LOGFILE}
      echo "$3" > ${TESTDIR}/dotest.exp
      rm -f ${TESTDIR}/dotest.ex2
      echo "** got: " >>${LOGFILE}
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      fail "$1"
    else
      pass "$1"
    fi
  else
    echo "$3" > ${TESTDIR}/dotest.exp
    if dotest_line_by_line "$1" "$2"; then
      pass "$1"
    else
      if test x"$4" != x; then
	mv ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.ex1
	echo "$4" > ${TESTDIR}/dotest.exp
	if dotest_line_by_line "$1" "$2"; then
	  pass "$1"
	else
	  mv ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.ex2
	  echo "** expected: " >>${LOGFILE}
	  echo "$3" >>${LOGFILE}
	  echo "** or: " >>${LOGFILE}
	  echo "$4" >>${LOGFILE}
	  echo "** got: " >>${LOGFILE}
	  cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	  fail "$1"
	fi
      else
	echo "** expected: " >>${LOGFILE}
	echo "$3" >>${LOGFILE}
	echo "** got: " >>${LOGFILE}
	cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	fail "$1"
      fi
    fi
  fi
}

# Usage:
#  dotest TESTNAME COMMAND OUTPUT [OUTPUT2]
# TESTNAME is the name used in the log to identify the test.
# COMMAND is the command to run; for the test to pass, it exits with
# exitstatus zero.
# OUTPUT is a regexp which is compared against the output (stdout and
# stderr combined) from the test.  It is anchored to the start and end
# of the output, so should start or end with ".*" if that is what is desired.
# Trailing newlines are stripped from the command's actual output before
# matching against OUTPUT.
# If OUTPUT2 is specified and the output matches it, then it is also
# a pass (partial workaround for the fact that some versions of expr
# lack \|).
dotest ()
{
  rm -f ${TESTDIR}/dotest.ex? 2>&1
  if $2 >${TESTDIR}/dotest.tmp 2>&1; then
    : so far so good
  else
    status=$?
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  dotest_internal "$@"
}

# Like dotest except only 2 args and result must exactly match stdin
dotest_lit ()
{
  rm -f ${TESTDIR}/dotest.ex? 2>&1
  if $2 >${TESTDIR}/dotest.tmp 2>&1; then
    : so far so good
  else
    status=$?
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  cat >${TESTDIR}/dotest.exp
  if cmp ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.tmp >/dev/null 2>&1; then
    pass "$1"
  else
    echo "** expected: " >>${LOGFILE}
    cat ${TESTDIR}/dotest.exp >>${LOGFILE}
    echo "** got: " >>${LOGFILE}
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    fail "$1"
  fi
}

# Like dotest except exitstatus should be nonzero.
dotest_fail ()
{
  rm -f ${TESTDIR}/dotest.ex? 2>&1
  if $2 >${TESTDIR}/dotest.tmp 2>&1; then
    status=$?
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  else
    : so far so good
  fi
  dotest_internal "$@"
}

# Like dotest except second argument is the required exitstatus.
dotest_status ()
{
  $3 >${TESTDIR}/dotest.tmp 2>&1
  status=$?
  if test "$status" = "$2"; then
    : so far so good
  else
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status; expected $2" >>${LOGFILE}
    fail "$1"
  fi
  dotest_internal "$1" "$3" "$4" "$5"
}

# clean any old remnants
rm -rf ${TESTDIR}
mkdir ${TESTDIR}
cd ${TESTDIR}

# Avoid picking up any stray .cvsrc, etc., from the user running the tests
mkdir home
HOME=${TESTDIR}/home; export HOME

# Remaining arguments are the names of tests to run.
#
# The testsuite is broken up into (hopefully manageably-sized)
# independently runnable tests, so that one can quickly get a result
# from a cvs or testsuite change, and to facilitate understanding the
# tests.

if test x"$*" = x; then
	# This doesn't yet include log2, because the bug it tests for
	# is not yet fixed, and/or we might want to wait until after 1.9.
	#
	# We also omit rdiff for now, because we have put off
	# committing the changes that make it work until after the 1.9
	# release.
	tests="basica basicb basic1 deep basic2 death death2 branches multibranch import join new newb conflicts conflicts2 modules mflag errmsg1 devcom ignore binfiles binwrap info serverpatch log"
else
	tests="$*"
fi

# this should die
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
	echo "FAIL: test 1" | tee -a ${LOGFILE}
	exit 1
else
	echo "PASS: test 1" >>${LOGFILE}
fi

# this should still die
mkdir cvsroot
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
	echo "FAIL: test 2" | tee -a ${LOGFILE}
	exit 1
else
	echo "PASS: test 2" >>${LOGFILE}
fi

# this should still die
mkdir cvsroot/CVSROOT
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
	echo "FAIL: test 3" | tee -a ${LOGFILE}
	exit 1
else
	echo "PASS: test 3" >>${LOGFILE}
fi

# This one should work, although it should spit a warning.
mkdir tmp ; cd tmp
${CVS} -d `pwd`/../cvsroot co CVSROOT 2>> ${LOGFILE}
cd .. ; rm -rf tmp

# set up a minimal modules file...
# (now that mkmodules is gone, this doesn't test -i the way it
# used to.  In fact, it looks like a noop to me).
echo "CVSROOT		CVSROOT" > cvsroot/CVSROOT/modules
# The following line stolen from cvsinit.sh.  FIXME: create our
# repository via cvsinit.sh; that way we test it too.
(cd cvsroot/CVSROOT; ci -q -u -t/dev/null \
  -m'initial checkin of modules' modules)

# This one should succeed.  No warnings.
mkdir tmp ; cd tmp
if ${CVS} -d `pwd`/../cvsroot co CVSROOT ; then
	echo "PASS: test 4" >>${LOGFILE}
else
	echo "FAIL: test 4" | tee -a ${LOGFILE}
	exit 1
fi

if echo "yes" | ${CVS} -d `pwd`/../cvsroot release -d CVSROOT ; then
	echo "PASS: test 4.5" >>${LOGFILE}
else
	echo "FAIL: test 4.5" | tee -a ${LOGFILE}
	exit 1
fi
# this had better be empty
cd ..; rmdir tmp
dotest_fail 4.75 "test -d tmp" ''

# a simple function to compare directory contents
#
# Returns: {nothing}
# Side Effects: ISDIFF := true|false
#
directory_cmp ()
{
	OLDPWD=`pwd`
	DIR_1=$1
	DIR_2=$2
	ISDIFF=false

	cd $DIR_1
	find . -print | fgrep -v /CVS | sort > /tmp/dc$$d1

	# go back where we were to avoid symlink hell...
	cd $OLDPWD
	cd $DIR_2
	find . -print | fgrep -v /CVS | sort > /tmp/dc$$d2

	if diff /tmp/dc$$d1 /tmp/dc$$d2 >/dev/null 2>&1
	then
		:
	else
		ISDIFF=true
		return
	fi
	cd $OLDPWD
	while read a
	do
		if test -f $DIR_1/"$a" ; then
			cmp -s $DIR_1/"$a" $DIR_2/"$a"
			if test $? -ne 0 ; then
				ISDIFF=true
			fi
		fi
	done < /tmp/dc$$d1
	rm -f /tmp/dc$$*
}

# so much for the setup.  Let's try something harder.

# Try setting CVSROOT so we don't have to worry about it anymore.  (now that
# we've tested -d cvsroot.)
CVSROOT_DIRNAME=${TESTDIR}/cvsroot
CVSROOT=${CVSROOT_DIRNAME} ; export CVSROOT
if test "x$remote" = xyes; then
	# Use rsh so we can test it without having to muck with inetd
	# or anything like that.  Also needed to get CVS_SERVER to
	# work.
	CVSROOT=:ext:`hostname`:${CVSROOT_DIRNAME} ; export CVSROOT
	CVS_SERVER=${testcvs}; export CVS_SERVER
fi

# start keeping history
touch ${CVSROOT_DIRNAME}/CVSROOT/history

### The big loop
for what in $tests; do
	case $what in
	basica)
	  # Similar in spirit to some of the basic1, and basic2
	  # tests, but hopefully a lot faster.  Also tests operating on
	  # files two directories down *without* operating on the parent dirs.

	  # Using mkdir in the repository is used throughout these
	  # tests to create a top-level directory.  I think instead it
	  # should be:
	  #   cvs co -l .
	  #   mkdir first-dir
	  #   cvs add first-dir
	  # but currently that works only for local CVS, not remote.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest basica-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  # Test a few operations, to ensure they gracefully do
	  # nothing in an empty directory.
	  dotest basica-1a0 "${testcvs} -q update" ''
	  dotest basica-1a1 "${testcvs} -q diff -c" ''
	  dotest basica-1a2 "${testcvs} -q status" ''

	  mkdir sdir
	  # Remote CVS gives the "cannot open CVS/Entries" error, which is
	  # clearly a bug, but not a simple one to fix.
	  dotest basica-1a10 "${testcvs} -n add sdir" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir added to the repository' \
"${PROG} add: cannot open CVS/Entries for reading: No such file or directory
Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir added to the repository"
	  dotest_fail basica-1a11 \
	    "test -d ${CVSROOT_DIRNAME}/first-dir/sdir" ''
	  dotest basica-2 "${testcvs} add sdir" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir added to the repository'
	  cd sdir
	  mkdir ssdir
	  dotest basica-3 "${testcvs} add ssdir" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir added to the repository'
	  cd ssdir
	  echo ssfile >ssfile

	  # Trying to commit it without a "cvs add" should be an error.
	  # The "use `cvs add' to create an entry" message is the one
	  # that I consider to be more correct, but local cvs prints the
	  # "nothing known" message and noone has gotten around to fixing it.
	  dotest_fail basica-notadded "${testcvs} -q ci ssfile" \
"${PROG} [a-z]*: use "'`cvs add'\'' to create an entry for ssfile
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!' \
"${PROG}"' [a-z]*: nothing known about `ssfile'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'

	  dotest basica-4 "${testcvs} add ssfile" \
"${PROG}"' [a-z]*: scheduling file `ssfile'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest_fail basica-4a "${testcvs} tag tag0 ssfile" \
"${PROG} [a-z]*: nothing known about ssfile
${PROG} "'\[[a-z]* aborted\]: correct the above errors first!'
	  cd ../..
	  dotest basica-5 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v
done
Checking in sdir/ssdir/ssfile;
/tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
initial revision: 1\.1
done'
	  dotest_fail basica-5a \
	    "${testcvs} -q tag BASE sdir/ssdir/ssfile" \
"${PROG} [a-z]*: Attempt to add reserved tag name BASE
${PROG} \[[a-z]* aborted\]: failed to set tag BASE to revision 1\.1 in /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v"
	  dotest basica-5b "${testcvs} -q tag NOT_RESERVED" \
'T sdir/ssdir/ssfile'

	  dotest basica-6 "${testcvs} -q update" ''
	  echo "ssfile line 2" >>sdir/ssdir/ssfile
	  dotest_status basica-6.2 1 "${testcvs} -q diff -c" \
'Index: sdir/ssdir/ssfile
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -r1\.1 ssfile
\*\*\* ssfile	[0-9/]* [0-9:]*	1\.1
--- ssfile	[0-9/]* [0-9:]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  ssfile
'"${PLUS} ssfile line 2"
	  dotest_status basica-6.3 1 "${testcvs} -q diff -c -rBASE" \
'Index: sdir/ssdir/ssfile
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -r1\.1 ssfile
\*\*\* ssfile	[0-9/]* [0-9:]*	1\.1
--- ssfile	[0-9/]* [0-9:]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  ssfile
'"${PLUS} ssfile line 2"
	  dotest basica-7 "${testcvs} -q ci -m modify-it" \
'Checking in sdir/ssdir/ssfile;
/tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 1\.2; previous revision: 1\.1
done'
	  dotest_fail basica-nonexist "${testcvs} -q ci nonexist" \
"${PROG}"' [a-z]*: nothing known about `nonexist'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'
	  dotest basica-8 "${testcvs} -q update" ''

	  # The .* here will normally be "No such file or directory",
	  # but if memory serves some systems (AIX?) have a different message.
:	  dotest_fail basica-9 \
	    "${testcvs} -q -d /tmp/cvs-sanity/nonexist update" \
"${PROG}: cannot access cvs root /tmp/cvs-sanity/nonexist: .*"
	  dotest_fail basica-9 \
	    "${testcvs} -q -d /tmp/cvs-sanity/nonexist update" \
"${PROG} \[[a-z]* aborted\]: /tmp/cvs-sanity/nonexist/CVSROOT: .*"

	  dotest basica-10 "${testcvs} annotate" \
'Annotations for sdir/ssdir/ssfile
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          .[a-z0-9@][a-z0-9@ ]* [0-9a-zA-Z-]*.: ssfile
1\.2          .[a-z0-9@][a-z0-9@ ]* [0-9a-zA-Z-]*.: ssfile line 2'
	  cd ..

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	basicb)
	  # More basic tests, including non-branch tags and co -d.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest basicb-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  mkdir sdir1 sdir2
	  dotest basicb-2 "${testcvs} add sdir1 sdir2" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir1 added to the repository
Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir2 added to the repository'
	  cd sdir1
	  echo sfile1 starts >sfile1
	  dotest basicb-2a10 "${testcvs} -n add sfile1" \
"${PROG} [a-z]*: scheduling file .sfile1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-2a11 "${testcvs} status sfile1" \
"${PROG} [a-z]*: use .cvs add' to create an entry for sfile1
===================================================================
File: sfile1           	Status: Unknown

   Working revision:	No entry for sfile1
   Repository revision:	No revision control file"
	  dotest basicb-3 "${testcvs} add sfile1" \
"${PROG} [a-z]*: scheduling file .sfile1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-3a1 "${testcvs} status sfile1" \
"===================================================================
File: sfile1           	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  cd ../sdir2
	  echo sfile2 starts >sfile2
	  dotest basicb-4 "${testcvs} add sfile2" \
"${PROG} [a-z]*: scheduling file .sfile2. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  cd ..
	  dotest basicb-5 "${testcvs} -q ci -m add" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/sdir1/sfile1,v
done
Checking in sdir1/sfile1;
/tmp/cvs-sanity/cvsroot/first-dir/sdir1/sfile1,v  <--  sfile1
initial revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/sdir2/sfile2,v
done
Checking in sdir2/sfile2;
/tmp/cvs-sanity/cvsroot/first-dir/sdir2/sfile2,v  <--  sfile2
initial revision: 1\.1
done'
	  echo sfile1 develops >sdir1/sfile1
	  dotest basicb-6 "${testcvs} -q ci -m modify" \
'Checking in sdir1/sfile1;
/tmp/cvs-sanity/cvsroot/first-dir/sdir1/sfile1,v  <--  sfile1
new revision: 1\.2; previous revision: 1\.1
done'
	  dotest basicb-7 "${testcvs} -q tag release-1" 'T sdir1/sfile1
T sdir2/sfile2'
	  echo not in time for release-1 >sdir2/sfile2
	  dotest basicb-8 "${testcvs} -q ci -m modify-2" \
'Checking in sdir2/sfile2;
/tmp/cvs-sanity/cvsroot/first-dir/sdir2/sfile2,v  <--  sfile2
new revision: 1\.2; previous revision: 1\.1
done'
	  cd ..

	  # Test that we recurse into the correct directory when checking
	  # for existing files, even if co -d is in use.
	  touch first-dir/extra
	  dotest basicb-cod-1 "${testcvs} -q co -d first-dir1 first-dir" \
'U first-dir1/sdir1/sfile1
U first-dir1/sdir2/sfile2'
	  rm -rf first-dir1

	  rm -rf first-dir
	  dotest basicb-9 \
"${testcvs} -q co -d newdir -r release-1 first-dir/sdir1 first-dir/sdir2" \
'U newdir/sdir1/sfile1
U newdir/sdir2/sfile2'
	  dotest basicb-10 "cat newdir/sdir1/sfile1 newdir/sdir2/sfile2" \
"sfile1 develops
sfile2 starts"

	  rm -rf newdir

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	basic1) # first dive - add a files, first singly, then in a group.
		mkdir ${CVSROOT_DIRNAME}/first-dir
		# check out an empty directory
		if ${CVS} co first-dir  ; then
		  echo "PASS: test 13a" >>${LOGFILE}
		else
		  echo "FAIL: test 13a" | tee -a ${LOGFILE}; exit 1
		fi

		cd first-dir
		files=first-file
		for i in a b ; do
			for j in ${files} ; do
				echo $j > $j
			done

			for do in add rm ; do
				for j in ${do} "commit -m test" ; do
					# ${do}
					if ${CVS} $j ${files}  >> ${LOGFILE} 2>&1; then
					  echo "PASS: test 14-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 14-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update it.
					if test "${do}" = "rm" -a "$j" != "commit -m test" || ${CVS} update ${files} ; then
					  echo "PASS: test 15-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 15-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update all.
					if ${CVS} update  ; then
					  echo "PASS: test 16-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 16-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# status all.
					if ${CVS} status  >> ${LOGFILE}; then
					  echo "PASS: test 17-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 17-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

		# FIXME: this one doesn't work yet for added files.
					# log all.
					if ${CVS} log  >> ${LOGFILE}; then
					  echo "PASS: test 18-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 18-${do}-$j" | tee -a ${LOGFILE}
					fi

					cd ..
					# update all.
					if ${CVS} update  ; then
					  echo "PASS: test 21-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 21-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# log all.
		# FIXME: doesn't work right for added files.
					if ${CVS} log first-dir  >> ${LOGFILE}; then
					  echo "PASS: test 22-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 22-${do}-$j" | tee -a ${LOGFILE}
					fi

					# status all.
					if ${CVS} status first-dir  >> ${LOGFILE}; then
					  echo "PASS: test 23-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 23-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update all.
					if ${CVS} update first-dir  ; then
					  echo "PASS: test 24-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 24-${do}-$j" | tee -a ${LOGFILE} ; exit 1
					fi

					# update all.
					if ${CVS} co first-dir  ; then
					  echo "PASS: test 27-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 27-${do}-$j" | tee -a ${LOGFILE} ; exit 1
					fi

					cd first-dir
				done # j
				rm -f ${files}
			done # do

			files="file2 file3 file4 file5"
		done
		if ${CVS} tag first-dive  ; then
		  echo "PASS: test 28" >>${LOGFILE}
		else
		  echo "FAIL: test 28" | tee -a ${LOGFILE} ; exit 1
		fi
		cd ..
		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -rf first-dir
		;;

	deep)
	  # Test the ability to operate on directories nested rather deeply.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest deep-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  for i in dir1 dir2 dir3 dir4 dir5 dir6 dir7 dir8; do
	    mkdir $i
	    dotest deep-2-$i "${testcvs} add $i" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/dir1[/dir0-9]* added to the repository'
	    cd $i
	    echo file1 >file1
	    dotest deep-3-$i "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  done
	  cd ../../../../../../../../..
	  dotest_lit deep-4 "${testcvs} -q ci -m add-them first-dir" <<'HERE'
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/file1,v
done
Checking in first-dir/dir1/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/file1,v
done
Checking in first-dir/dir1/dir2/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/file1,v
done
Checking in first-dir/dir1/dir2/dir3/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v  <--  file1
initial revision: 1.1
done
HERE

	  cd first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8
	  rm file1
	  dotest deep-4a0 "${testcvs} rm file1" \
"${PROG} [a-z]*: scheduling .file1. for removal
${PROG} [a-z]*: use .cvs commit. to remove this file permanently"
	  dotest deep-4a1 "${testcvs} -q ci -m rm-it" 'Removing file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done'
	  cd ../../..
	  dotest deep-4a2 "${testcvs} -q update -P dir6/dir7" ''
	  # Should be using "test -e" if that is portable enough.
	  dotest_fail deep-4a3 "test -d dir6/dir7/dir8" ''
	  cd ../../../../../..

	  if echo "yes" | ${testcvs} release -d first-dir >>${LOGFILE}; then
	    pass deep-5
	  else
	    fail deep-5
	  fi
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	basic2)
		# Test rtag, import, history, various miscellaneous operations

		# NOTE: this section has reached the size and
		# complexity where it is getting to be a good idea to
		# add new tests to a new section rather than
		# continuing to piggyback them onto the tests here.

		# First empty the history file
		rm ${CVSROOT_DIRNAME}/CVSROOT/history
		touch ${CVSROOT_DIRNAME}/CVSROOT/history

### XXX maybe should use 'cvs imprt -b1 -m new-module first-dir F F1' in an
### empty directory to do this instead of hacking directly into $CVSROOT
		mkdir ${CVSROOT_DIRNAME}/first-dir
		dotest basic2-1 "${testcvs} -q co first-dir" ''
		for i in first-dir dir1 dir2 ; do
			if test ! -d $i ; then
				mkdir $i
				if ${CVS} add $i  >> ${LOGFILE}; then
				  echo "PASS: test 29-$i" >>${LOGFILE}
				else
				  echo "FAIL: test 29-$i" | tee -a ${LOGFILE} ; exit 1
				fi
			fi

			cd $i

			for j in file6 file7; do
				echo $j > $j
			done

			if ${CVS} add file6 file7  2>> ${LOGFILE}; then
				echo "PASS: test 30-$i-$j" >>${LOGFILE}
			else
				echo "FAIL: test 30-$i-$j" | tee -a ${LOGFILE} ; exit 1
			fi
		done
		cd ../../..
		if ${CVS} update first-dir  ; then
			echo "PASS: test 31" >>${LOGFILE}
		else
			echo "FAIL: test 31" | tee -a ${LOGFILE} ; exit 1
		fi

		# fixme: doesn't work right for added files.
		if ${CVS} log first-dir  >> ${LOGFILE}; then
			echo "PASS: test 32" >>${LOGFILE}
		else
			echo "FAIL: test 32" | tee -a ${LOGFILE} # ; exit 1
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
			echo "PASS: test 33" >>${LOGFILE}
		else
			echo "FAIL: test 33" | tee -a ${LOGFILE} ; exit 1
		fi

# XXX why is this commented out???
#		if ${CVS} diff -u first-dir   >> ${LOGFILE} || test $? = 1 ; then
#			echo "PASS: test 34" >>${LOGFILE}
#		else
#			echo "FAIL: test 34" | tee -a ${LOGFILE} # ; exit 1
#		fi

		if ${CVS} ci -m "second dive" first-dir  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 35" >>${LOGFILE}
		else
			echo "FAIL: test 35" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} tag second-dive first-dir  ; then
			echo "PASS: test 36" >>${LOGFILE}
		else
			echo "FAIL: test 36" | tee -a ${LOGFILE} ; exit 1
		fi

		# third dive - in bunch o' directories, add bunch o' files,
		# delete some, change some.

		for i in first-dir dir1 dir2 ; do
			cd $i

			# modify a file
			echo file6 >>file6

			# delete a file
			rm file7

			if ${CVS} rm file7  2>> ${LOGFILE}; then
				echo "PASS: test 37-$i" >>${LOGFILE}
			else
				echo "FAIL: test 37-$i" | tee -a ${LOGFILE} ; exit 1
			fi

			# and add a new file
			echo file14 >file14

			if ${CVS} add file14  2>> ${LOGFILE}; then
				echo "PASS: test 38-$i" >>${LOGFILE}
			else
				echo "FAIL: test 38-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done
		cd ../../..
		if ${CVS} update first-dir  ; then
			echo "PASS: test 39" >>${LOGFILE}
		else
			echo "FAIL: test 39" | tee -a ${LOGFILE} ; exit 1
		fi

		# FIXME: doesn't work right for added files
		if ${CVS} log first-dir  >> ${LOGFILE}; then
			echo "PASS: test 40" >>${LOGFILE}
		else
			echo "FAIL: test 40" | tee -a ${LOGFILE} # ; exit 1
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
			echo "PASS: test 41" >>${LOGFILE}
		else
			echo "FAIL: test 41" | tee -a ${LOGFILE} ; exit 1
		fi

# XXX why is this commented out?
#		if ${CVS} diff -u first-dir  >> ${LOGFILE} || test $? = 1 ; then
#			echo "PASS: test 42" >>${LOGFILE}
#		else
#			echo "FAIL: test 42" | tee -a ${LOGFILE} # ; exit 1
#		fi

		if ${CVS} ci -m "third dive" first-dir  >>${LOGFILE} 2>&1; then
			echo "PASS: test 43" >>${LOGFILE}
		else
			echo "FAIL: test 43" | tee -a ${LOGFILE} ; exit 1
		fi
		dotest 43.5 "${testcvs} -q update first-dir" ''

		if ${CVS} tag third-dive first-dir  ; then
			echo "PASS: test 44" >>${LOGFILE}
		else
			echo "FAIL: test 44" | tee -a ${LOGFILE} ; exit 1
		fi

		if echo "yes" | ${CVS} release -d first-dir  ; then
			echo "PASS: test 45" >>${LOGFILE}
		else
			echo "FAIL: test 45" | tee -a ${LOGFILE} ; exit 1
		fi

		# end of third dive
		if test -d first-dir ; then
			echo "FAIL: test 45.5" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 45.5" >>${LOGFILE}
		fi

		# now try some rtags

		# rtag HEADS
		if ${CVS} rtag rtagged-by-head first-dir  ; then
			echo "PASS: test 46" >>${LOGFILE}
		else
			echo "FAIL: test 46" | tee -a ${LOGFILE} ; exit 1
		fi

		# tag by tag
		if ${CVS} rtag -r rtagged-by-head rtagged-by-tag first-dir  ; then
			echo "PASS: test 47" >>${LOGFILE}
		else
			echo "FAIL: test 47" | tee -a ${LOGFILE} ; exit 1
		fi

		# tag by revision
		if ${CVS} rtag -r1.1 rtagged-by-revision first-dir  ; then
			echo "PASS: test 48" >>${LOGFILE}
		else
			echo "FAIL: test 48" | tee -a ${LOGFILE} ; exit 1
		fi

		# rdiff by revision
		if ${CVS} rdiff -r1.1 -rrtagged-by-head first-dir  >> ${LOGFILE} || test $? = 1 ; then
			echo "PASS: test 49" >>${LOGFILE}
		else
			echo "FAIL: test 49" | tee -a ${LOGFILE} ; exit 1
		fi

		# now export by rtagged-by-head and rtagged-by-tag and compare.
		rm -rf first-dir
		if ${CVS} export -r rtagged-by-head first-dir  ; then
			echo "PASS: test 50" >>${LOGFILE}
		else
			echo "FAIL: test 50" | tee -a ${LOGFILE} ; exit 1
		fi

		mv first-dir 1dir
		if ${CVS} export -r rtagged-by-tag first-dir  ; then
			echo "PASS: test 51" >>${LOGFILE}
		else
			echo "FAIL: test 51" | tee -a ${LOGFILE} ; exit 1
		fi

		directory_cmp 1dir first-dir

		if $ISDIFF ; then
			echo "FAIL: test 52" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 52" >>${LOGFILE}
		fi
		rm -rf 1dir first-dir

		# checkout by revision vs export by rtagged-by-revision and compare.
		if ${CVS} export -rrtagged-by-revision -d export-dir first-dir  ; then
			echo "PASS: test 53" >>${LOGFILE}
		else
			echo "FAIL: test 53" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} co -r1.1 first-dir  ; then
			echo "PASS: test 54" >>${LOGFILE}
		else
			echo "FAIL: test 54" | tee -a ${LOGFILE} ; exit 1
		fi

		# directory copies are done in an oblique way in order to avoid a bug in sun's tmp filesystem.
		mkdir first-dir.cpy ; (cd first-dir ; tar cf - * | (cd ../first-dir.cpy ; tar xf -))

		directory_cmp first-dir export-dir

		if $ISDIFF ; then
			echo "FAIL: test 55" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 55" >>${LOGFILE}
		fi

		# interrupt, while we've got a clean 1.1 here, let's import it
		# into a couple of other modules.
		cd export-dir
		dotest 56 "${testcvs} import -m first-import second-dir first-immigration immigration1 immigration1_0" \
'N second-dir/file14
N second-dir/file6
N second-dir/file7
'"${PROG}"' [a-z]*: Importing /tmp/cvs-sanity/cvsroot/second-dir/dir1
N second-dir/dir1/file14
N second-dir/dir1/file6
N second-dir/dir1/file7
'"${PROG}"' [a-z]*: Importing /tmp/cvs-sanity/cvsroot/second-dir/dir1/dir2
N second-dir/dir1/dir2/file14
N second-dir/dir1/dir2/file6
N second-dir/dir1/dir2/file7

No conflicts created by this import'
		cd ..

		if ${CVS} export -r HEAD second-dir  ; then
			echo "PASS: test 57" >>${LOGFILE}
		else
			echo "FAIL: test 57" | tee -a ${LOGFILE} ; exit 1
		fi

		directory_cmp first-dir second-dir

		if $ISDIFF ; then
			echo "FAIL: test 58" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 58" >>${LOGFILE}
		fi

		rm -rf second-dir

		rm -rf export-dir first-dir
		mkdir first-dir
		(cd first-dir.cpy ; tar cf - * | (cd ../first-dir ; tar xf -))

		# update the top, cancelling sticky tags, retag, update other copy, compare.
		cd first-dir
		if ${CVS} update -A -l *file*  2>> ${LOGFILE}; then
			echo "PASS: test 59" >>${LOGFILE}
		else
			echo "FAIL: test 59" | tee -a ${LOGFILE} ; exit 1
		fi

		# If we don't delete the tag first, cvs won't retag it.
		# This would appear to be a feature.
		if ${CVS} tag -l -d rtagged-by-revision  ; then
			echo "PASS: test 60a" >>${LOGFILE}
		else
			echo "FAIL: test 60a" | tee -a ${LOGFILE} ; exit 1
		fi
		if ${CVS} tag -l rtagged-by-revision  ; then
			echo "PASS: test 60b" >>${LOGFILE}
		else
			echo "FAIL: test 60b" | tee -a ${LOGFILE} ; exit 1
		fi

		cd ..
		mv first-dir 1dir
		mv first-dir.cpy first-dir
		cd first-dir

		dotest 61 "${testcvs} -q diff -u" ''

		if ${CVS} update  ; then
			echo "PASS: test 62" >>${LOGFILE}
		else
			echo "FAIL: test 62" | tee -a ${LOGFILE} ; exit 1
		fi

		cd ..

		#### FIXME: is this expected to work???  Need to investigate
		#### and fix or remove the test.
#		directory_cmp 1dir first-dir
#
#		if $ISDIFF ; then
#			echo "FAIL: test 63" | tee -a ${LOGFILE} # ; exit 1
#		else
#			echo "PASS: test 63" >>${LOGFILE}
#		fi
		rm -rf 1dir first-dir

		# Test the cvs history command.

		# The reason that there are two patterns rather than using
		# \(/tmp/cvs-sanity\|<remote>\) is that we are trying to
		# make this portable.  Perhaps at some point we should
		# ditch that notion and require GNU expr (or dejagnu or....)
		# since it seems to be so painful.

		# why are there two lines at the end of the local output
		# which don't exist in the remote output?  would seem to be
		# a CVS bug.
		dotest basic2-64 "${testcvs} his -e -a" \
'O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir           =first-dir= '"${TMPPWD}"'/cvs-sanity/\*
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir           == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir           == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1      == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1      == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1/dir2 == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1/dir2 == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir           == '"${TMPPWD}"'/cvs-sanity
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir           == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1      == '"${TMPPWD}"'/cvs-sanity
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1      == '"${TMPPWD}"'/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1/dir2 == '"${TMPPWD}"'/cvs-sanity
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1/dir2 == '"${TMPPWD}"'/cvs-sanity
F [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]*                     =first-dir= '"${TMPPWD}"'/cvs-sanity/\*
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-revision:1\.1\]
O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* \[1\.1\] first-dir           =first-dir= '"${TMPPWD}"'/cvs-sanity/\*
U [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir           == '"${TMPPWD}"'/cvs-sanity/first-dir
U [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file7     first-dir           == '"${TMPPWD}"'/cvs-sanity/first-dir' \
'O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir           =first-dir= <remote>/\*
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir           == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir           == <remote>
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1      == <remote>
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1/dir2 == <remote>
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1/dir2 == <remote>
F [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]*                     =first-dir= <remote>/\*
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-revision:1\.1\]
O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z0-9@][a-z0-9@]* \[1\.1\] first-dir           =first-dir= <remote>/\*'

		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -rf ${CVSROOT_DIRNAME}/second-dir
		;;

	rdiff)
		# Test rdiff
		# XXX for now this is just the most essential test...
		cd ${TESTDIR}

		mkdir testimport
		cd testimport
		echo '$''Id$' > foo
		echo '$''Name$' >> foo
		echo '$''Id$' > bar
		echo '$''Name$' >> bar
		dotest rdiff-1 \
		  "${testcvs} import -I ! -m test-import-with-keyword trdiff TRDIFF T1" \
'N trdiff/foo
N trdiff/bar

No conflicts created by this import'
		dotest rdiff-2 \
		  "${testcvs} co -ko trdiff" \
'cvs [a-z]*: Updating trdiff
U trdiff/bar
U trdiff/foo'
		cd trdiff
		echo something >> foo
		dotest rdiff-3 \
		  "${testcvs} ci -m added-something foo" \
'Checking in foo;
/tmp/cvs-sanity/cvsroot/trdiff/foo,v  <--  foo
new revision: 1\.2; previous revision: 1\.1
done'
		echo '#ident	"@(#)trdiff:$''Name$:$''Id$"' > new
		echo "new file" >> new
		dotest rdiff-4 \
		  "${testcvs} add -m new-file-description new" \
"cvs [a-z]*: scheduling file \`new' for addition
cvs [a-z]*: use 'cvs commit' to add this file permanently"
		dotest rdiff-5 \
		  "${testcvs} commit -m added-new-file new" \
'RCS file: /tmp/cvs-sanity/cvsroot/trdiff/new,v
done
Checking in new;
/tmp/cvs-sanity/cvsroot/trdiff/new,v  <--  new
initial revision: 1\.1
done'
		dotest rdiff-6 \
		  "${testcvs} tag local-v0" \
'cvs [a-z]*: Tagging .
T bar
T foo
T new'
		dotest rdiff-7 \
		  "${testcvs} status -v foo" \
'===================================================================
File: foo              	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	/tmp/cvs-sanity/cvsroot/trdiff/foo,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-ko

   Existing Tags:
	local-v0                 	(revision: 1\.2)
	T1                       	(revision: 1\.1\.1\.1)
	TRDIFF                   	(branch: 1\.1\.1)'

		cd ..
		rm -rf trdiff

		dotest rdiff-8 \
		  "${testcvs} rdiff -r T1 -r local-v0 trdiff" \
'cvs [a-z]*: Diffing trdiff
Index: trdiff/foo
diff -c trdiff/foo:1\.1\.1\.1 trdiff/foo:1\.2
\*\*\* trdiff/foo:1\.1\.1\.1	.*
--- trdiff/foo	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1,2 \*\*\*\*
! \$''Id\$
! \$''Name\$
--- 1,3 ----
! \$''Id: foo,v 1\.2 [0-9/]* [0-9:]* [a-zA-Z0-9][a-zA-Z0-9]* Exp \$
! \$''Name: local-v0 \$
! something
Index: trdiff/new
diff -c /dev/null trdiff/new:1\.1
\*\*\* /dev/null	.*
--- trdiff/new	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1,2 ----
'"${PLUS}"' #ident	"@(#)trdiff:\$''Name: local-v0 \$:\$''Id: new,v 1\.1 [0-9/]* [0-9:]* [a-zA-Z0-9][a-zA-Z0-9]* Exp \$"
'"${PLUS}"' new file'

		# This appears to be broken client/server
		if test "x$remote" = xno; then
		dotest rdiff-9 \
		  "${testcvs} rdiff -Ko -kv -r T1 -r local-v0 trdiff" \
'cvs [a-z]*: Diffing trdiff
Index: trdiff/foo
diff -c trdiff/foo:1\.1\.1\.1 trdiff/foo:1\.2
\*\*\* trdiff/foo:1\.1\.1\.1	.*
--- trdiff/foo	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1,2 \*\*\*\*
! \$''Id\$
! \$''Name\$
--- 1,3 ----
! foo,v 1\.2 .* Exp
! local-v0
! something
Index: trdiff/new
diff -c /dev/null trdiff/new:1\.1
\*\*\* /dev/null	.*
--- trdiff/new	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1,2 ----
'"${PLUS}"' #ident	"@(#)trdiff:local-v0:new,v 1\.1 .* Exp"
'"${PLUS}"' new file'
		fi # end tests we are skipping for client/server

# FIXME: will this work here?
#		if test "$keep" = yes; then
#		  echo Keeping /tmp/cvs-sanity and exiting due to --keep
#		  exit 0
#		fi

		rm -rf ${CVSROOT_DIRNAME}/trdiff
		;;

	death)
		# next dive.  test death support.

		# NOTE: this section has reached the size and
		# complexity where it is getting to be a good idea to
		# add new death support tests to a new section rather
		# than continuing to piggyback them onto the tests here.

		mkdir  ${CVSROOT_DIRNAME}/first-dir
		if ${CVS} co first-dir  ; then
			echo "PASS: test 65" >>${LOGFILE}
		else
			echo "FAIL: test 65" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir

		# Create a directory with only dead files, to make sure CVS
		# doesn't get confused by it.
		mkdir subdir
		dotest 65a0 "${testcvs} add subdir" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/subdir added to the repository'
		cd subdir
		echo file in subdir >sfile
		dotest 65a1 "${testcvs} add sfile" \
"${PROG}"' [a-z]*: scheduling file `sfile'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
		dotest 65a2 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/subdir/sfile,v
done
Checking in sfile;
/tmp/cvs-sanity/cvsroot/first-dir/subdir/sfile,v  <--  sfile
initial revision: 1\.1
done'
		rm sfile
		dotest 65a3 "${testcvs} rm sfile" \
"${PROG}"' [a-z]*: scheduling `sfile'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG}"' commit'\'' to remove this file permanently'
		dotest 65a4 "${testcvs} -q ci -m remove-it" \
'Removing sfile;
/tmp/cvs-sanity/cvsroot/first-dir/subdir/sfile,v  <--  sfile
new revision: delete; previous revision: 1\.1
done'
		cd ..
		dotest 65a5 "${testcvs} -q update -P" ''
		dotest_fail 65a6 "test -d subdir" ''

		# add a file.
		touch file1
		if ${CVS} add file1  2>> ${LOGFILE}; then
			echo "PASS: test 66" >>${LOGFILE}
		else
			echo "FAIL: test 66" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 67" >>${LOGFILE}
		else
			echo "FAIL: test 67" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove
		rm file1
		if ${CVS} rm file1  2>> ${LOGFILE}; then
			echo "PASS: test 68" >>${LOGFILE}
		else
			echo "FAIL: test 68" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
			echo "PASS: test 69" >>${LOGFILE}
		else
			echo "FAIL: test 69" | tee -a ${LOGFILE} ; exit 1
		fi

		dotest_fail 69a0 "test -f file1" ''
		# get the old contents of file1 back
		if ${testcvs} update -p -r 1.1 file1 >file1 2>>${LOGFILE}; then
		  pass 69a1
		else
		  fail 69a1
		fi
		dotest 69a2 "cat file1" ''

		# create second file
		touch file2
		if ${CVS} add file1 file2  2>> ${LOGFILE}; then
			echo "PASS: test 70" >>${LOGFILE}
		else
			echo "FAIL: test 70" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 71" >>${LOGFILE}
		else
			echo "FAIL: test 71" | tee -a ${LOGFILE} ; exit 1
		fi

		# log
		if ${CVS} log file1  >> ${LOGFILE}; then
			echo "PASS: test 72" >>${LOGFILE}
		else
			echo "FAIL: test 72" | tee -a ${LOGFILE} ; exit 1
		fi

		# file4 will be dead at the time of branching and stay dead.
		echo file4 > file4
		dotest death-file4-add "${testcvs} add file4" \
"${PROG}"' [a-z]*: scheduling file `file4'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
		dotest death-file4-ciadd "${testcvs} -q ci -m add file4" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1\.1
done'
		rm file4
		dotest death-file4-rm "${testcvs} remove file4" \
"${PROG}"' [a-z]*: scheduling `file4'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG}"' commit'\'' to remove this file permanently'
		dotest death-file4-cirm "${testcvs} -q ci -m remove file4" \
'Removing file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1
done'

		# Tag the branchpoint.
		dotest death-72a "${testcvs} -q tag bp_branch1" 'T file1
T file2'

		# branch1
		if ${CVS} tag -b branch1  ; then
			echo "PASS: test 73" >>${LOGFILE}
		else
			echo "FAIL: test 73" | tee -a ${LOGFILE} ; exit 1
		fi

		# and move to the branch.
		if ${CVS} update -r branch1  ; then
			echo "PASS: test 74" >>${LOGFILE}
		else
			echo "FAIL: test 74" | tee -a ${LOGFILE} ; exit 1
		fi

		dotest_fail death-file4-3 "test -f file4" ''

		# add a file in the branch
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
			echo "PASS: test 75" >>${LOGFILE}
		else
			echo "FAIL: test 75" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 76" >>${LOGFILE}
		else
			echo "FAIL: test 76" | tee -a ${LOGFILE} ; exit 1
		fi

		# Remote CVS outputs nothing for 76a0 and 76a1; until
		# this bug is fixed just skip those tests for remote.
		if test "x$remote" = xno; then
		  dotest death-76a0 \
"${testcvs} -q rdiff -r bp_branch1 -r branch1 first-dir" \
"Index: first-dir/file3
diff -c /dev/null first-dir/file3:1\.1\.2\.1
\*\*\* /dev/null	.*
--- first-dir/file3	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} line1 from branch1"
		  dotest death-76a1 \
"${testcvs} -q rdiff -r branch1 -r bp_branch1 first-dir" \
'Index: first-dir/file3
diff -c first-dir/file3:1\.1\.2\.1 first-dir/file3:removed
\*\*\* first-dir/file3:1\.1\.2\.1	.*
--- first-dir/file3	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- line1 from branch1
--- 0 ----'
		fi

		# remove
		rm file3
		if ${CVS} rm file3  2>> ${LOGFILE}; then
			echo "PASS: test 77" >>${LOGFILE}
		else
			echo "FAIL: test 77" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
			echo "PASS: test 78" >>${LOGFILE}
		else
			echo "FAIL: test 78" | tee -a ${LOGFILE} ; exit 1
		fi

		# add again
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
			echo "PASS: test 79" >>${LOGFILE}
		else
			echo "FAIL: test 79" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 80" >>${LOGFILE}
		else
			echo "FAIL: test 80" | tee -a ${LOGFILE} ; exit 1
		fi

		# change the first file
		echo line2 from branch1 >> file1

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 81" >>${LOGFILE}
		else
			echo "FAIL: test 81" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove the second
		rm file2
		if ${CVS} rm file2  2>> ${LOGFILE}; then
			echo "PASS: test 82" >>${LOGFILE}
		else
			echo "FAIL: test 82" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
			echo "PASS: test 83" >>${LOGFILE}
		else
			echo "FAIL: test 83" | tee -a ${LOGFILE} ; exit 1
		fi

		# back to the trunk.
		if ${CVS} update -A  2>> ${LOGFILE}; then
			echo "PASS: test 84" >>${LOGFILE}
		else
			echo "FAIL: test 84" | tee -a ${LOGFILE} ; exit 1
		fi

		dotest_fail death-file4-4 "test -f file4" ''

		if test -f file3 ; then
			echo "FAIL: test 85" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 85" >>${LOGFILE}
		fi

		# join
		dotest 86 "${testcvs} -q update -j branch1" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
retrieving revision 1\.3
retrieving revision 1\.3\.2\.1
Merging differences between 1\.3 and 1\.3\.2\.1 into file1
'"${PROG}"' [a-z]*: scheduling file2 for removal
U file3'

		dotest_fail death-file4-5 "test -f file4" ''

		if test -f file3 ; then
			echo "PASS: test 87" >>${LOGFILE}
		else
			echo "FAIL: test 87" | tee -a ${LOGFILE} ; exit 1
		fi

		# Make sure that we joined the correct change to file1
		if echo line2 from branch1 | cmp - file1 >/dev/null; then
			echo 'PASS: test 87a' >>${LOGFILE}
		else
			echo 'FAIL: test 87a' | tee -a ${LOGFILE}
			exit 1
		fi

		# update
		if ${CVS} update  ; then
			echo "PASS: test 88" >>${LOGFILE}
		else
			echo "FAIL: test 88" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		dotest 89 "${testcvs} -q ci -m test" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done
Removing file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done
Checking in file3;
/tmp/cvs-sanity/cvsroot/first-dir/file3,v  <--  file3
new revision: 1\.2; previous revision: 1\.1
done'
		cd ..
		mkdir 2
		cd 2
		dotest 89a "${testcvs} -q co first-dir" 'U first-dir/file1
U first-dir/file3'
		cd ..
		rm -rf 2
		cd first-dir

		# remove first file.
		rm file1
		if ${CVS} rm file1  2>> ${LOGFILE}; then
			echo "PASS: test 90" >>${LOGFILE}
		else
			echo "FAIL: test 90" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
			echo "PASS: test 91" >>${LOGFILE}
		else
			echo "FAIL: test 91" | tee -a ${LOGFILE} ; exit 1
		fi

		if test -f file1 ; then
			echo "FAIL: test 92" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 92" >>${LOGFILE}
		fi

		# typo; try to get to the branch and fail
		dotest_fail 92.1a "${testcvs} update -r brnach1" \
		  "${PROG}"' \[[a-z]* aborted\]: no such tag brnach1'
		# Make sure we are still on the trunk
		if test -f file1 ; then
			echo "FAIL: 92.1b" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: 92.1b" >>${LOGFILE}
		fi
		if test -f file3 ; then
			echo "PASS: 92.1c" >>${LOGFILE}
		else
			echo "FAIL: 92.1c" | tee -a ${LOGFILE} ; exit 1
		fi

		# back to branch1
		if ${CVS} update -r branch1  2>> ${LOGFILE}; then
			echo "PASS: test 93" >>${LOGFILE}
		else
			echo "FAIL: test 93" | tee -a ${LOGFILE} ; exit 1
		fi

		dotest_fail death-file4-6 "test -f file4" ''

		if test -f file1 ; then
			echo "PASS: test 94" >>${LOGFILE}
		else
			echo "FAIL: test 94" | tee -a ${LOGFILE} ; exit 1
		fi

		# and join
		dotest 95 "${testcvs} -q update -j HEAD" \
"${PROG}"' [a-z]*: file file1 has been modified, but has been removed in revision HEAD
'"${PROG}"' [a-z]*: file file3 exists, but has been added in revision HEAD'

		dotest_fail death-file4-7 "test -f file4" ''

		# file2 should not have been recreated.  It was
		# deleted on the branch, and has not been modified on
		# the trunk.  That means that there have been no
		# changes between the greatest common ancestor (the
		# trunk version) and HEAD.
		dotest_fail death-file2-1 "test -f file2" ''

		cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		;;

	death2)
	  # More tests of death support.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest death2-1 "${testcvs} -q co first-dir" ''

	  cd first-dir

	  # Add a file on the trunk.
	  echo "first revision" > file1
	  dotest death2-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'

	  dotest death2-3 "${testcvs} -q commit -m add" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done'

	  # Make a branch and a non-branch tag.
	  dotest death2-4 "${testcvs} -q tag -b branch" 'T file1'
	  dotest death2-5 "${testcvs} -q tag tag" 'T file1'

	  # Switch over to the branch.
	  dotest death2-6 "${testcvs} -q update -r branch" ''

	  # Delete the file on the branch.
	  rm file1
	  dotest death2-7 "${testcvs} rm file1" \
"${PROG} [a-z]*: scheduling .file1. for removal
${PROG} [a-z]*: use .cvs commit. to remove this file permanently"
	  dotest death2-8 "${testcvs} -q ci -m removed" \
'Removing file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1\.2
done'

	  # Test diff of a dead file.
	  dotest_fail death2-diff-1 \
"${testcvs} -q diff -r1.1 -rbranch -c file1" \
"${PROG} [a-z]*: file1 was removed, no comparison available"

	  dotest_fail death2-diff-2 \
"${testcvs} -q diff -r1.1 -rbranch -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* [a-zA-Z0-9/.]*[ 	][	]*[a-zA-Z0-9: ]*
--- /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  dotest_fail death2-diff-3 "${testcvs} -q diff -rtag -c ." \
"${PROG} [a-z]*: file1 no longer exists, no comparison available"

	  dotest_fail death2-diff-4 "${testcvs} -q diff -rtag -N -c ." \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* [a-zA-Z0-9/.]*[ 	][	]*[a-zA-Z0-9: ]*
--- /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  # Test rdiff of a dead file.
	  dotest death2-rdiff-1 \
"${testcvs} -q rtag -rbranch rdiff-tag first-dir" ''

	  dotest death2-rdiff-2 "${testcvs} -q rdiff -rtag -rbranch first-dir" \
"Index: first-dir/file1
diff -c first-dir/file1:1\.1 first-dir/file1:removed
\*\*\* first-dir/file1:1\.1[ 	][	]*[a-zA-Z0-9: ]*
--- first-dir/file1[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  # Readd the file to the branch.
	  echo "second revision" > file1
	  dotest death2-9 "${testcvs} add file1" \
"${PROG}"' [a-z]*: file `file1'\'' will be added on branch `branch'\'' from version 1\.1\.2\.1
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest death2-10 "${testcvs} -q commit -m add" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done'

	  # Back to the trunk.
	  dotest death2-11 "${testcvs} -q update -A" 'U file1' 'P file1'

	  # Add another file on the trunk.
	  echo "first revision" > file2
	  dotest death2-12 "${testcvs} add file2" \
"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest death2-13 "${testcvs} -q commit -m add" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file2,v
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done'

	  # Back to the branch.
	  # The ``no longer in the repository'' message doesn't really
	  # look right to me, but that's what CVS currently prints for
	  # this case.
	  dotest death2-14 "${testcvs} -q update -r branch" \
"U file1
${PROG} [a-z]*: file2 is no longer in the repository" \
"P file1
${PROG} [a-z]*: file2 is no longer in the repository"

	  # Add a file on the branch with the same name.
	  echo "branch revision" > file2
	  dotest death2-15 "${testcvs} add file2" \
"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest death2-16 "${testcvs} -q commit -m add" \
'Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done'

	  # Add a new file on the branch.
	  echo "first revision" > file3
	  dotest death2-17 "${testcvs} add file3" \
"${PROG}"' [a-z]*: scheduling file `file3'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest death2-18 "${testcvs} -q commit -m add" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/Attic/file3,v
done
Checking in file3;
/tmp/cvs-sanity/cvsroot/first-dir/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done'

	  # Test diff of a nonexistent tag
	  dotest_fail death2-diff-5 "${testcvs} -q diff -rtag -c file3" \
"${PROG} [a-z]*: tag tag is not in file file3"

	  dotest_fail death2-diff-6 "${testcvs} -q diff -rtag -N -c file3" \
"Index: file3
===================================================================
RCS file: file3
diff -N file3
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- [a-zA-Z0-9/.]*[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

	  dotest_fail death2-diff-7 "${testcvs} -q diff -rtag -c ." \
"Index: file1
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
diff -c -r1\.1 -r1\.1\.2\.2
\*\*\* file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
--- file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! first revision
--- 1 ----
! second revision
${PROG} [a-z]*: tag tag is not in file file2
${PROG} [a-z]*: tag tag is not in file file3"

	  dotest_fail death2-diff-8 "${testcvs} -q diff -rtag -c -N ." \
"Index: file1
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
diff -c -r1\.1 -r1\.1\.2\.2
\*\*\* file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
--- file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! first revision
--- 1 ----
! second revision
Index: file2
===================================================================
RCS file: file2
diff -N file2
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- [a-zA-Z0-9/.]*[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} branch revision
Index: file3
===================================================================
RCS file: file3
diff -N file3
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- [a-zA-Z0-9/.]*[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

	  # Switch to the nonbranch tag.
	  dotest death2-19 "${testcvs} -q update -r tag" \
"U file1
${PROG} [a-z]*: file2 is no longer in the repository
${PROG} [a-z]*: file3 is no longer in the repository" \
"P file1
${PROG} [a-z]*: file2 is no longer in the repository
${PROG} [a-z]*: file3 is no longer in the repository"

	  dotest_fail death2-20 "test -f file2"

	  # Make sure we can't add a file on this nonbranch tag.
	  # FIXME: Right now CVS will let you add a file on a
	  # nonbranch tag, so this test is commented out.
	  # echo "bad revision" > file2
	  # dotest death2-21 "${testcvs} add file2" "some error message"

	  # Make sure diff only reports appropriate files.
	  dotest_fail death2-diff-9 "${testcvs} -q diff -r rdiff-tag" \
"${PROG} [a-z]*: file1 is a new entry, no comparison available"

	  dotest_fail death2-diff-10 "${testcvs} -q diff -r rdiff-tag -c -N" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- [a-zA-Z0-9/.]*[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

	  cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
	  ;;

	branches)
	  # More branch tests, including branches off of branches
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest branches-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 1:ancest >file1
	  echo 2:ancest >file2
	  echo 3:ancest >file3
	  echo 4:trunk-1 >file4
	  dotest branches-2 "${testcvs} add file1 file2 file3 file4" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file3'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file4'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest_lit branches-3 "${testcvs} -q ci -m add-it" <<'HERE'
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file2,v
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file3,v
done
Checking in file3;
/tmp/cvs-sanity/cvsroot/first-dir/file3,v  <--  file3
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1.1
done
HERE
	  echo 4:trunk-2 >file4
	  dotest branches-3.2 "${testcvs} -q ci -m trunk-before-branch" \
'Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2; previous revision: 1\.1
done'
	  dotest branches-4 "${testcvs} tag -b br1" "${PROG}"' [a-z]*: Tagging \.
T file1
T file2
T file3
T file4'
	  dotest branches-5 "${testcvs} update -r br1" \
"${PROG}"' [a-z]*: Updating \.'
	  echo 1:br1 >file1
	  echo 2:br1 >file2
	  echo 4:br1 >file4
	  dotest branches-6 "${testcvs} -q ci -m modify" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.1; previous revision: 1\.2
done'
	  dotest branches-7 "${testcvs} -q tag -b brbr" 'T file1
T file2
T file3
T file4'
	  dotest branches-8 "${testcvs} -q update -r brbr" ''
	  echo 1:brbr >file1
	  echo 4:brbr >file4
	  dotest branches-9 "${testcvs} -q ci -m modify" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1\.2\.1; previous revision: 1\.1\.2\.1
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.1\.2\.1; previous revision: 1\.2\.2\.1
done'
	  dotest branches-10 "cat file1 file2 file3 file4" '1:brbr
2:br1
3:ancest
4:brbr'
	  dotest branches-11 "${testcvs} -q update -r br1" \
'[UP] file1
[UP] file4'
	  dotest branches-12 "cat file1 file2 file3 file4" '1:br1
2:br1
3:ancest
4:br1'
	  echo 4:br1-2 >file4
	  dotest branches-12.2 "${testcvs} -q ci -m change-on-br1" \
'Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.2; previous revision: 1\.2\.2\.1
done'
	  dotest branches-13 "${testcvs} -q update -A" '[UP] file1
[UP] file2
[UP] file4'
	  dotest branches-14 "cat file1 file2 file3 file4" '1:ancest
2:ancest
3:ancest
4:trunk-2'
	  echo 4:trunk-3 >file4
	  dotest branches-14.2 \
	    "${testcvs} -q ci -m trunk-change-after-branch" \
'Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.3; previous revision: 1\.2
done'
	  dotest branches-14.3 "${testcvs} log file4" \
'
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
Working file: file4
head: 1\.3
branch:
locks: strict
access list:
symbolic names:
	brbr: 1\.2\.2\.1\.0\.2
	br1: 1\.2\.0\.2
keyword substitution: kv
total revisions: 6;	selected revisions: 6
description:
----------------------------
revision 1\.3
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
trunk-change-after-branch
----------------------------
revision 1\.2
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
branches:  1\.2\.2;
trunk-before-branch
----------------------------
revision 1\.1
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;
add-it
----------------------------
revision 1\.2\.2\.2
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
change-on-br1
----------------------------
revision 1\.2\.2\.1
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
branches:  1\.2\.2\.1\.2;
modify
----------------------------
revision 1\.2\.2\.1\.2\.1
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
modify
============================================================================='
	  dotest_status branches-14.4 1 \
	    "${testcvs} diff -c -r 1.1 -r 1.3 file4" \
'Index: file4
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.3
diff -c -r1\.1 -r1\.3
\*\*\* file4	[0-9/]* [0-9:]*	1\.1
--- file4	[0-9/]* [0-9:]*	1\.3
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:trunk-3'
	  dotest_status branches-14.5 1 \
	    "${testcvs} diff -c -r 1.1 -r 1.2.2.1 file4" \
'Index: file4
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.2\.2\.1
diff -c -r1\.1 -r1\.2\.2\.1
\*\*\* file4	[0-9/]* [0-9:]*	1\.1
--- file4	[0-9/]* [0-9:]*	1\.2\.2\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:br1'
	  dotest branches-15 \
	    "${testcvs} update -j 1.1.2.1 -j 1.1.2.1.2.1 file1" \
	    'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
retrieving revision 1\.1\.2\.1
retrieving revision 1\.1\.2\.1\.2\.1
Merging differences between 1\.1\.2\.1 and 1\.1\.2\.1\.2\.1 into file1
rcsmerge: warning: conflicts during merge'
	  dotest branches-16 "cat file1" '<<<<<<< file1
1:ancest
=======
1:brbr
[>]>>>>>> 1\.1\.2\.1\.2\.1'
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping /tmp/cvs-sanity and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	multibranch)
	  # Test the ability to have several branchpoints coming off the
	  # same revision.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest multibranch-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 1:trunk-1 >file1
	  dotest multibranch-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest_lit multibranch-3 "${testcvs} -q ci -m add-it" <<'HERE'
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1.1
done
HERE
	  dotest multibranch-4 "${testcvs} tag -b br1" \
"${PROG} [a-z]*: Tagging \.
T file1"
	  dotest multibranch-5 "${testcvs} tag -b br2" \
"${PROG} [a-z]*: Tagging \.
T file1"
	  dotest multibranch-6 "${testcvs} -q update -r br1" ''
	  echo on-br1 >file1
	  dotest multibranch-7 "${testcvs} -q ci -m modify-on-br1" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done'
	  dotest multibranch-8 "${testcvs} -q update -r br2" '[UP] file1'
	  echo br2 adds a line >>file1
	  dotest multibranch-9 "${testcvs} -q ci -m modify-on-br2" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done'
	  dotest multibranch-10 "${testcvs} -q update -r br1" '[UP] file1'
	  dotest multibranch-11 "cat file1" 'on-br1'
	  dotest multibranch-12 "${testcvs} -q update -r br2" '[UP] file1'
	  dotest multibranch-13 "cat file1" '1:trunk-1
br2 adds a line'

	  dotest multibranch-14 "${testcvs} log file1" \
"
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	br2: 1\.1\.0\.4
	br1: 1\.1\.0\.2
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: [0-9a-zA-Z-]*;  state: Exp;
branches:  1\.1\.2;  1\.1\.4;
add-it
----------------------------
revision 1\.1\.4\.1
date: [0-9/]* [0-9:]*;  author: [0-9a-zA-Z-]*;  state: Exp;  lines: ${PLUS}1 -0
modify-on-br2
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: [0-9a-zA-Z-]*;  state: Exp;  lines: ${PLUS}1 -1
modify-on-br1
============================================================================="
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping /tmp/cvs-sanity and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	import) # test death after import
		# import
		mkdir import-dir ; cd import-dir

		for i in 1 2 3 4 ; do
			echo imported file"$i" > imported-file"$i"
		done

		# This directory should be on the default ignore list,
		# so it shouldn't get imported.
		mkdir RCS
		echo ignore.me >RCS/ignore.me

		echo 'import should not expand $''Id$' >>imported-file2
		cp imported-file2 ../imported-file2-orig.tmp

		if ${CVS} import -m first-import first-dir vendor-branch junk-1_0  ; then
			echo "PASS: test 96" >>${LOGFILE}
		else
			echo "FAIL: test 96" | tee -a ${LOGFILE} ; exit 1
		fi

		if cmp ../imported-file2-orig.tmp imported-file2; then
		  pass 96.5
		else
		  fail 96.5
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
			echo "PASS: test 97" >>${LOGFILE}
		else
			echo "FAIL: test 97" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir
		for i in 1 2 3 4 ; do
			if test -f imported-file"$i" ; then
				echo "PASS: test 98-$i" >>${LOGFILE}
			else
				echo "FAIL: test 98-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done
		if test -d RCS; then
		  echo "FAIL: test 98.5" | tee -a ${LOGFILE} ; exit 1
		else
		  echo "PASS: test 98.5" >>${LOGFILE}
		fi

		# remove
		rm imported-file1
		if ${CVS} rm imported-file1  2>> ${LOGFILE}; then
			echo "PASS: test 99" >>${LOGFILE}
		else
			echo "FAIL: test 99" | tee -a ${LOGFILE} ; exit 1
		fi

		# change
		echo local-change >> imported-file2

		# commit
		if ${CVS} ci -m local-changes  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 100" >>${LOGFILE}
		else
			echo "FAIL: test 100" | tee -a ${LOGFILE} ; exit 1
		fi

		# log
		if ${CVS} log imported-file1 | grep '1.1.1.2 (dead)'  ; then
			echo "FAIL: test 101" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 101" >>${LOGFILE}
		fi

		# update into the vendor branch.
		if ${CVS} update -rvendor-branch  ; then
			echo "PASS: test 102" >>${LOGFILE}
		else
			echo "FAIL: test 102" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove file4 on the vendor branch
		rm imported-file4

		if ${CVS} rm imported-file4  2>> ${LOGFILE}; then
			echo "PASS: test 103" >>${LOGFILE}
		else
			echo "FAIL: test 103" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m vendor-removed imported-file4 >>${LOGFILE}; then
			echo "PASS: test 104" >>${LOGFILE}
		else
			echo "FAIL: test 104" | tee -a ${LOGFILE} ; exit 1
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
			echo "PASS: test 105" >>${LOGFILE}
		else
			echo "FAIL: test 105" | tee -a ${LOGFILE} ; exit 1
		fi

		# second import - file4 deliberately unchanged
		cd ../import-dir
		for i in 1 2 3 ; do
			echo rev 2 of file $i >> imported-file"$i"
		done
		cp imported-file2 ../imported-file2-orig.tmp

		if ${CVS} import -m second-import first-dir vendor-branch junk-2_0  ; then
			echo "PASS: test 106" >>${LOGFILE}
		else
			echo "FAIL: test 106" | tee -a ${LOGFILE} ; exit 1
		fi
		if cmp ../imported-file2-orig.tmp imported-file2; then
		  pass 106.5
		else
		  fail 106.5
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
			echo "PASS: test 107" >>${LOGFILE}
		else
			echo "FAIL: test 107" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir

		if test -f imported-file1 ; then
			echo "FAIL: test 108" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 108" >>${LOGFILE}
		fi

		for i in 2 3 ; do
			if test -f imported-file"$i" ; then
				echo "PASS: test 109-$i" >>${LOGFILE}
			else
				echo "FAIL: test 109-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done

		# check vendor branch for file4
		if ${CVS} update -rvendor-branch  ; then
			echo "PASS: test 110" >>${LOGFILE}
		else
			echo "FAIL: test 110" | tee -a ${LOGFILE} ; exit 1
		fi

		if test -f imported-file4 ; then
			echo "PASS: test 111" >>${LOGFILE}
		else
			echo "FAIL: test 111" | tee -a ${LOGFILE} ; exit 1
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
			echo "PASS: test 112" >>${LOGFILE}
		else
			echo "FAIL: test 112" | tee -a ${LOGFILE} ; exit 1
		fi

		cd ..

		dotest import-113 \
"${testcvs} -q co -jjunk-1_0 -jjunk-2_0 first-dir" \
"${PROG}"' [a-z]*: file first-dir/imported-file1 is present in revision junk-2_0
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/imported-file2,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.2
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.2 into imported-file2
rcsmerge: warning: conflicts during merge'

		cd first-dir

		if test -f imported-file1 ; then
			echo "FAIL: test 114" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 114" >>${LOGFILE}
		fi

		for i in 2 3 ; do
			if test -f imported-file"$i" ; then
				echo "PASS: test 115-$i" >>${LOGFILE}
			else
				echo "FAIL: test 115-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done

		dotest import-116 'cat imported-file2' \
'imported file2
[<]<<<<<< imported-file2
import should not expand \$''Id: imported-file2,v 1\.2 [0-9/]* [0-9:]* [a-z0-9@][a-z0-9@]* Exp \$
local-change
[=]======
import should not expand \$''Id: imported-file2,v 1\.1\.1\.2 [0-9/]* [0-9:]* [a-z0-9@][a-z0-9@]* Exp \$
rev 2 of file 2
[>]>>>>>> 1\.1\.1\.2'

		cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		rm -rf import-dir
		;;

	join)
	  # Test doing joins which involve adding and removing files.

	  # We check merging changes from T1 to T2 into the main line.
	  # Here are the interesting cases I can think of:
	  #   1) File added between T1 and T2, not on main line.
	  #      File should be marked for addition.
	  #   2) File added between T1 and T2, also added on main line.
	  #      Conflict.
	  #   3) File removed between T1 and T2, unchanged on main line.
	  #      File should be marked for removal.
	  #   4) File removed between T1 and T2, modified on main line.
	  #      If mod checked in, file should be marked for removal.
	  #	 If mod still in working directory, conflict.
	  #   5) File removed between T1 and T2, was never on main line.
	  #      Nothing should happen.
	  #   6) File removed between T1 and T2, also removed on main line.
	  #      Nothing should happen.
	  #   7) File added on main line, not added between T1 and T2.
	  #      Nothing should happen.
	  #   8) File removed on main line, not modified between T1 and T2.
	  #      Nothing should happen.

	  # We also check merging changes from a branch into the main
	  # line.  Here are the interesting cases:
	  #   1) File added on branch, not on main line.
	  #      File should be marked for addition.
	  #   2) File added on branch, also added on main line.
	  #      Conflict.
	  #   3) File removed on branch, unchanged on main line.
	  #      File should be marked for removal.
	  #   4) File removed on branch, modified on main line.
	  #      Conflict.
	  #   5) File removed on branch, was never on main line.
	  #      Nothing should happen.
	  #   6) File removed on branch, also removed on main line.
	  #      Nothing should happen.
	  #   7) File added on main line, not added on branch.
	  #      Nothing should happen.
	  #   8) File removed on main line, not modified on branch.
	  #      Nothing should happen.

	  # In the tests below, fileN represents case N in the above
	  # lists.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest join-1 "${testcvs} -q co first-dir" ''

	  cd first-dir

	  # Add two files.
	  echo 'first revision of file3' > file3
	  echo 'first revision of file4' > file4
	  echo 'first revision of file6' > file6
	  echo 'first revision of file8' > file8
	  dotest join-2 "${testcvs} add file3 file4 file6 file8" \
"${PROG}"' [a-z]*: scheduling file `file3'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file4'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file6'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file8'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'

	  dotest join-3 "${testcvs} -q commit -m add" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file3,v
done
Checking in file3;
/tmp/cvs-sanity/cvsroot/first-dir/file3,v  <--  file3
initial revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file6,v
done
Checking in file6;
/tmp/cvs-sanity/cvsroot/first-dir/file6,v  <--  file6
initial revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file8,v
done
Checking in file8;
/tmp/cvs-sanity/cvsroot/first-dir/file8,v  <--  file8
initial revision: 1\.1
done'

	  # Make a branch.
	  dotest join-4 "${testcvs} -q tag -b branch ." \
'T file3
T file4
T file6
T file8'

	  # Add file2 and file7, modify file4, and remove file6 and file8.
	  echo 'first revision of file2' > file2
	  echo 'second revision of file4' > file4
	  echo 'first revision of file7' > file7
	  rm file6 file8
	  dotest join-5 "${testcvs} add file2 file7" \
"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file7'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest join-6 "${testcvs} rm file6 file8" \
"${PROG}"' [a-z]*: scheduling `file6'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file8'\'' for removal
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to remove these files permanently'
	  dotest join-7 "${testcvs} -q ci -mx ." \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file2,v
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2; previous revision: 1\.1
done
Removing file6;
/tmp/cvs-sanity/cvsroot/first-dir/file6,v  <--  file6
new revision: delete; previous revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file7,v
done
Checking in file7;
/tmp/cvs-sanity/cvsroot/first-dir/file7,v  <--  file7
initial revision: 1\.1
done
Removing file8;
/tmp/cvs-sanity/cvsroot/first-dir/file8,v  <--  file8
new revision: delete; previous revision: 1\.1
done'

	  # Check out the branch.
	  cd ../..
	  mkdir 2
	  cd 2
	  dotest join-8 "${testcvs} -q co -r branch first-dir" \
'U first-dir/file3
U first-dir/file4
U first-dir/file6
U first-dir/file8'

	  cd first-dir

	  # Modify the files on the branch, so that T1 is not an
	  # ancestor of the main line, and add file5
	  echo 'first branch revision of file3' > file3
	  echo 'first branch revision of file4' > file4
	  echo 'first branch revision of file6' > file6
	  echo 'first branch revision of file5' > file5
	  dotest join-9 "${testcvs} add file5" \
"${PROG}"' [a-z]*: scheduling file `file5'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest join-10 "${testcvs} -q ci -mx ." \
'Checking in file3;
/tmp/cvs-sanity/cvsroot/first-dir/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/Attic/file5,v
done
Checking in file5;
/tmp/cvs-sanity/cvsroot/first-dir/Attic/file5,v  <--  file5
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file6;
/tmp/cvs-sanity/cvsroot/first-dir/Attic/file6,v  <--  file6
new revision: 1\.1\.2\.1; previous revision: 1\.1
done'

	  # Tag the current revisions on the branch.
	  dotest join-11 "${testcvs} -q tag T1 ." \
'T file3
T file4
T file5
T file6
T file8'

	  # Add file1 and file2, and remove the other files.
	  echo 'first branch revision of file1' > file1
	  echo 'first branch revision of file2' > file2
	  rm file3 file4 file5 file6
	  dotest join-12 "${testcvs} add file1 file2" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest join-13 "${testcvs} rm file3 file4 file5 file6" \
"${PROG}"' [a-z]*: scheduling `file3'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file4'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file5'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file6'\'' for removal
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to remove these files permanently'
	  dotest join-14 "${testcvs} -q ci -mx ." \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/Attic/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/Attic/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Removing file3;
/tmp/cvs-sanity/cvsroot/first-dir/file3,v  <--  file3
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file5;
/tmp/cvs-sanity/cvsroot/first-dir/Attic/file5,v  <--  file5
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file6;
/tmp/cvs-sanity/cvsroot/first-dir/Attic/file6,v  <--  file6
new revision: delete; previous revision: 1\.1\.2\.1
done'

	  # Tag the current revisions on the branch.
	  dotest join-15 "${testcvs} -q tag T2 ." \
'T file1
T file2
T file8'

	  # Do a checkout with a merge.
	  cd ../..
	  mkdir 3
	  cd 3
	  dotest join-16 "${testcvs} -q co -jT1 -jT2 first-dir" \
'U first-dir/file1
U first-dir/file2
'"${PROG}"' [a-z]*: file first-dir/file2 exists, but has been added in revision T2
U first-dir/file3
'"${PROG}"' [a-z]*: scheduling first-dir/file3 for removal
U first-dir/file4
'"${PROG}"' [a-z]*: scheduling first-dir/file4 for removal
U first-dir/file7'

	  # Verify that the right changes have been scheduled.
	  cd first-dir
	  dotest join-17 "${testcvs} -q update" \
'A file1
R file3
R file4'

	  # Modify file4 locally, and do an update with a merge.
	  cd ../../1/first-dir
	  echo 'third revision of file4' > file4
	  dotest join-18 "${testcvs} -q update -jT1 -jT2 ." \
'U file1
'"${PROG}"' [a-z]*: file file2 exists, but has been added in revision T2
'"${PROG}"' [a-z]*: scheduling file3 for removal
M file4
'"${PROG}"' [a-z]*: file file4 is locally modified, but has been removed in revision T2'

	  # Verify that the right changes have been scheduled.
	  dotest join-19 "${testcvs} -q update" \
'A file1
R file3
M file4'

	  # Do a checkout with a merge from a single revision.

	  # FIXME: CVS currently gets this wrong.  file2 has been
	  # added on both the branch and the main line, and so should
	  # be regarded as a conflict.  However, given the way that
	  # CVS sets up the RCS file, there is no way to distinguish
	  # this case from the case of file2 having existed before the
	  # branch was made.  This could be fixed by reserving
	  # a revision somewhere, perhaps 1.1, as an always dead
	  # revision which can be used as the source for files added
	  # on branches.
	  cd ../../3
	  rm -rf first-dir
	  dotest join-20 "${testcvs} -q co -jbranch first-dir" \
'U first-dir/file1
U first-dir/file2
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into file2
U first-dir/file3
'"${PROG}"' [a-z]*: scheduling first-dir/file3 for removal
U first-dir/file4
'"${PROG}"' [a-z]*: file first-dir/file4 has been modified, but has been removed in revision branch
U first-dir/file7'

	  # Verify that the right changes have been scheduled.
	  # The M file2 line is a bug; see above join-20.
	  cd first-dir
	  dotest join-21 "${testcvs} -q update" \
'A file1
M file2
R file3'

	  # Checkout the main line again.
	  cd ../../1
	  rm -rf first-dir
	  dotest join-22 "${testcvs} -q co first-dir" \
'U first-dir/file2
U first-dir/file3
U first-dir/file4
U first-dir/file7'

	  # Modify file4 locally, and do an update with a merge from a
	  # single revision.
	  # The file2 handling is a bug; see above join-20.
	  cd first-dir
	  echo 'third revision of file4' > file4
	  dotest join-23 "${testcvs} -q update -jbranch ." \
'U file1
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into file2
'"${PROG}"' [a-z]*: scheduling file3 for removal
M file4
'"${PROG}"' [a-z]*: file file4 is locally modified, but has been removed in revision branch'

	  # Verify that the right changes have been scheduled.
	  # The M file2 line is a bug; see above join-20
	  dotest join-24 "${testcvs} -q update" \
'A file1
M file2
R file3
M file4'

	  cd ../..
	  rm -rf 1 2 3 ${CVSROOT_DIRNAME}/first-dir
	  ;;

	new) # look for stray "no longer pertinent" messages.
		mkdir ${CVSROOT_DIRNAME}/first-dir

		if ${CVS} co first-dir  ; then
			echo "PASS: test 117" >>${LOGFILE}
		else
			echo "FAIL: test 117" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir
		touch a

		if ${CVS} add a  2>>${LOGFILE}; then
			echo "PASS: test 118" >>${LOGFILE}
		else
			echo "FAIL: test 118" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} ci -m added  >>${LOGFILE} 2>&1; then
			echo "PASS: test 119" >>${LOGFILE}
		else
			echo "FAIL: test 119" | tee -a ${LOGFILE} ; exit 1
		fi

		rm a

		if ${CVS} rm a  2>>${LOGFILE}; then
			echo "PASS: test 120" >>${LOGFILE}
		else
			echo "FAIL: test 120" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} ci -m removed >>${LOGFILE} ; then
			echo "PASS: test 121" >>${LOGFILE}
		else
			echo "FAIL: test 121" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} update -A  2>&1 | grep longer ; then
			echo "FAIL: test 122" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 122" >>${LOGFILE}
		fi

		if ${CVS} update -rHEAD 2>&1 | grep longer ; then
			echo "FAIL: test 123" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 123" >>${LOGFILE}
		fi

		cd .. ; rm -rf first-dir ; rm -rf ${CVSROOT_DIRNAME}/first-dir
		;;

	newb)
	  # Test removing a file on a branch and then checking it out.

	  # We call this "newb" only because it, like the "new" tests,
	  # has something to do with "no longer pertinent" messages.
	  # Not necessarily the most brilliant nomenclature.

	  # Create file 'a'.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest newb-123a "${testcvs} -q co first-dir" ''
	  cd first-dir
	  touch a
	  dotest newb-123b "${testcvs} add a" \
"${PROG} [a-z]*: scheduling file .a. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest newb-123c "${testcvs} -q ci -m added" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/a,v
done
Checking in a;
/tmp/cvs-sanity/cvsroot/first-dir/a,v  <--  a
initial revision: 1\.1
done'

	  # Make a branch.
	  dotest newb-123d "${testcvs} -q tag -b branch" "T a"

	  # Check out the branch.
	  cd ..
	  rm -rf first-dir
	  mkdir 1
	  cd 1
	  dotest newb-123e "${testcvs} -q co -r branch first-dir" \
"U first-dir/a"

	  # Remove 'a' on another copy of the branch.
	  cd ..
	  mkdir 2
	  cd 2
	  dotest newb-123f "${testcvs} -q co -r branch first-dir" \
"U first-dir/a"
	  cd first-dir
	  rm a
	  dotest newb-123g "${testcvs} rm a" \
"${PROG} [a-z]*: scheduling .a. for removal
${PROG} [a-z]*: use .cvs commit. to remove this file permanently"
	  dotest newb-123h "${testcvs} -q ci -m removed" \
'Removing a;
/tmp/cvs-sanity/cvsroot/first-dir/a,v  <--  a
new revision: delete; previous revision: 1\.1\.2
done'

	  # Check out the file on the branch.  This should report
	  # that the file is not pertinent, but it should not
	  # say anything else.
	  cd ..
	  rm -rf first-dir
	  dotest newb-123i "${testcvs} -q co -r branch first-dir/a" \
"${PROG} [a-z]*: warning: first-dir/a is not (any longer) pertinent"

	  # Update the other copy, and make sure that a is removed.
	  cd ../1/first-dir
	  # "Needs Patch" is a rather strange output here.  Something like
	  # "Removed in Repository" would make more sense.
	  dotest newb-123j0 "${testcvs} status a" \
"===================================================================
File: a                	Status: Needs Patch

   Working revision:	1\.1.*
   Repository revision:	1\.1\.2\.1	/tmp/cvs-sanity/cvsroot/first-dir/a,v
   Sticky Tag:		branch (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest newb-123j "${testcvs} -q update" \
"${PROG} [a-z]*: warning: a is not (any longer) pertinent"

	  if test -f a; then
	    fail newb-123k
	  else
	    pass newb-123k
	  fi

	  cd ../..
	  rm -rf 1 2 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	conflicts)
		mkdir ${CVSROOT_DIRNAME}/first-dir

		mkdir 1
		cd 1

		dotest conflicts-124 "${testcvs} -q co first-dir" ''

		cd first-dir
		touch a

		dotest conflicts-125 "${testcvs} add a" \
"${PROG} [a-z]*: scheduling file .a. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
		dotest conflicts-126 "${testcvs} -q ci -m added" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/a,v
done
Checking in a;
/tmp/cvs-sanity/cvsroot/first-dir/a,v  <--  a
initial revision: 1\.1
done'

		cd ../..
		mkdir 2
		cd 2

		# TODO-maybe: we could also check this (also in an empty
		# directory) after the file has nonempty contents.
		#
		# The need for TMPPWD here is a (minor) CVS bug; the
		# output should use the name of the repository as specified.
		dotest conflicts-126.5 "${testcvs} co -p first-dir" \
"${PROG} [a-z]*"': Updating first-dir
===================================================================
Checking out first-dir/a
RCS:  '"${TMPPWD}"'/cvs-sanity/cvsroot/first-dir/a,v
VERS: 1\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*'
		if ${CVS} co first-dir ; then
			echo 'PASS: test 127' >>${LOGFILE}
		else
			echo 'FAIL: test 127' | tee -a ${LOGFILE}
		fi
		cd first-dir
		if test -f a; then
			echo 'PASS: test 127a' >>${LOGFILE}
		else
			echo 'FAIL: test 127a' | tee -a ${LOGFILE}
		fi

		cd ../../1/first-dir
		echo add a line >>a
		mkdir dir1
		dotest conflicts-127b "${testcvs} add dir1" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/dir1 added to the repository'
		dotest conflicts-128 "${testcvs} -q ci -m changed" \
'Checking in a;
/tmp/cvs-sanity/cvsroot/first-dir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done'
		cd ../../2/first-dir
		echo add a conflicting line >>a
		dotest_fail conflicts-129 "${testcvs} -q ci -m changed" \
"${PROG}"' [a-z]*: Up-to-date check failed for `a'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'
		mkdir dir1
		mkdir sdir
		dotest conflicts-130 "${testcvs} -q update" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/a,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into a
rcsmerge: warning: conflicts during merge
'"${PROG}"' [a-z]*: conflicts found in a
C a
'"${QUESTION}"' dir1
'"${QUESTION}"' sdir' \
''"${QUESTION}"' dir1
'"${QUESTION}"' sdir
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/a,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into a
rcsmerge: warning: conflicts during merge
'"${PROG}"' [a-z]*: conflicts found in a
C a'
		rmdir dir1 sdir

		# Try to check in the file with the conflict markers in it.
		if ${CVS} ci -m try 2>>${LOGFILE}; then
			echo 'FAIL: test 131' | tee -a ${LOGFILE}
		else
			# Should tell us to resolve conflict first
			echo 'PASS: test 131' >>${LOGFILE}
		fi

		echo lame attempt at resolving it >>a
		# Try to check in the file with the conflict markers in it.
		if ${CVS} ci -m try >>${LOGFILE} 2>&1; then
			echo 'FAIL: test 132' | tee -a ${LOGFILE}
		else
			# Should tell us to resolve conflict first
			echo 'PASS: test 132' >>${LOGFILE}
		fi

		echo resolve conflict >a
		if ${CVS} ci -m resolved >>${LOGFILE} 2>&1; then
			echo 'PASS: test 133' >>${LOGFILE}
		else
			echo 'FAIL: test 133' | tee -a ${LOGFILE}
		fi

		# Now test that we can add a file in one working directory
		# and have an update in another get it.
		cd ../../1/first-dir
		echo abc >abc
		if ${testcvs} add abc >>${LOGFILE} 2>&1; then
			echo 'PASS: test 134' >>${LOGFILE}
		else
			echo 'FAIL: test 134' | tee -a ${LOGFILE}
		fi
		if ${testcvs} ci -m 'add abc' abc >>${LOGFILE} 2>&1; then
			echo 'PASS: test 135' >>${LOGFILE}
		else
			echo 'FAIL: test 135' | tee -a ${LOGFILE}
		fi
		cd ../../2
		mkdir first-dir/dir1 first-dir/sdir
		dotest conflicts-136 "${testcvs} -q update" \
'[UP] first-dir/abc
'"${QUESTION}"' first-dir/dir1
'"${QUESTION}"' first-dir/sdir' \
''"${QUESTION}"' first-dir/dir1
'"${QUESTION}"' first-dir/sdir
[UP] first-dir/abc'
		dotest conflicts-137 'test -f first-dir/abc' ''
		rmdir first-dir/dir1 first-dir/sdir

		# Now test something similar, but in which the parent directory
		# (not the directory in question) has the Entries.Static flag
		# set.
		cd ../1/first-dir
		mkdir subdir
		if ${testcvs} add subdir >>${LOGFILE}; then
			echo 'PASS: test 138' >>${LOGFILE}
		else
			echo 'FAIL: test 138' | tee -a ${LOGFILE}
		fi
		cd ../..
		mkdir 3
		cd 3
		if ${testcvs} -q co first-dir/abc first-dir/subdir \
		    >>${LOGFILE}; then
		  echo 'PASS: test 139' >>${LOGFILE}
		else
		  echo 'FAIL: test 139' | tee -a ${LOGFILE}
		fi
		cd ../1/first-dir/subdir
		echo sss >sss
		if ${testcvs} add sss >>${LOGFILE} 2>&1; then
		  echo 'PASS: test 140' >>${LOGFILE}
		else
		  echo 'FAIL: test 140' | tee -a ${LOGFILE}
		fi
		if ${testcvs} ci -m adding sss >>${LOGFILE} 2>&1; then
		  echo 'PASS: test 140' >>${LOGFILE}
		else
		  echo 'FAIL: test 140' | tee -a ${LOGFILE}
		fi
		cd ../../../3/first-dir
		if ${testcvs} -q update >>${LOGFILE}; then
		  echo 'PASS: test 141' >>${LOGFILE}
		else
		  echo 'FAIL: test 141' | tee -a ${LOGFILE}
		fi
		if test -f subdir/sss; then
		  echo 'PASS: test 142' >>${LOGFILE}
		else
		  echo 'FAIL: test 142' | tee -a ${LOGFILE}
		fi
		cd ../..
		rm -rf 1 2 3 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
		;;

	conflicts2)
	  # More conflicts tests; separate from conflicts to keep each
	  # test a manageable size.
	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  mkdir 1
	  cd 1

	  dotest conflicts2-142a1 "${testcvs} -q co first-dir" ''

	  cd first-dir
	  touch a abc

	  dotest conflicts2-142a2 "${testcvs} add a abc" \
"${PROG} [a-z]*: scheduling file .a. for addition
${PROG} [a-z]*: scheduling file .abc. for addition
${PROG} [a-z]*: use .cvs commit. to add these files permanently"
	  dotest conflicts2-142a3 "${testcvs} -q ci -m added" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/a,v
done
Checking in a;
/tmp/cvs-sanity/cvsroot/first-dir/a,v  <--  a
initial revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/abc,v
done
Checking in abc;
/tmp/cvs-sanity/cvsroot/first-dir/abc,v  <--  abc
initial revision: 1\.1
done'

	  cd ../..
	  mkdir 2
	  cd 2

	  dotest conflicts2-142a4 "${testcvs} -q co first-dir" 'U first-dir/a
U first-dir/abc'
	  cd ..

	  # Now test that if one person modifies and commits a
	  # file and a second person removes it, it is a
	  # conflict
	  cd 1/first-dir
	  echo modify a >>a
	  dotest conflicts2-142b2 "${testcvs} -q ci -m modify-a" \
'Checking in a;
/tmp/cvs-sanity/cvsroot/first-dir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done'
	  cd ../../2/first-dir
	  rm a
	  dotest conflicts2-142b3 "${testcvs} rm a" \
"${PROG} [a-z]*: scheduling .a. for removal
${PROG} [a-z]*: use .cvs commit. to remove this file permanently"
	  dotest_fail conflicts2-142b4 "${testcvs} -q update" \
"${PROG} [a-z]*: conflict: removed a was modified by second party
C a"
	  # Resolve the conflict by deciding not to remove the file
	  # after all.
	  dotest conflicts2-142b5 "${testcvs} add a" "U a
${PROG} [a-z]*: a, version 1\.1, resurrected"
	  dotest conflicts2-142b6 "${testcvs} -q update" ''
	  cd ../..

	  # Now test that if one person removes a file and
	  # commits it, and a second person removes it, is it
	  # not a conflict.
	  cd 1/first-dir
	  rm abc
	  dotest conflicts2-142c0 "${testcvs} rm abc" \
"${PROG} [a-z]*: scheduling .abc. for removal
${PROG} [a-z]*: use .cvs commit. to remove this file permanently"
	  dotest conflicts2-142c1 "${testcvs} -q ci -m remove-abc" \
'Removing abc;
/tmp/cvs-sanity/cvsroot/first-dir/abc,v  <--  abc
new revision: delete; previous revision: 1\.1
done'
	  cd ../../2/first-dir
	  rm abc
	  dotest conflicts2-142c2 "${testcvs} rm abc" \
"${PROG} [a-z]*: scheduling .abc. for removal
${PROG} [a-z]*: use .cvs commit. to remove this file permanently"
	  dotest conflicts2-142c3 "${testcvs} update" \
"${PROG} [a-z]*: Updating \."
	  cd ../..

	  # conflicts2-142d*: test that if one party adds a file, and another
	  # party has a file of the same name, cvs notices
	  cd 1/first-dir
	  touch aa.c
	  dotest conflicts2-142d0 "${testcvs} add aa.c" \
"${PROG} [a-z]*: scheduling file .aa\.c. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest conflicts2-142d1 "${testcvs} -q ci -m added" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/aa.c,v
done
Checking in aa.c;
/tmp/cvs-sanity/cvsroot/first-dir/aa.c,v  <--  aa.c
initial revision: 1\.1
done'
	  cd ../../2/first-dir
	  echo "don't you dare obliterate this text" >aa.c
	  # Doing this test separately for remote and local is a fair
	  # bit of a kludge, but the exit status differs.  I'm not sure
	  # which exit status is the more appropriate one.
	  if test "$remote" = yes; then
	    dotest conflicts2-142d2 "${testcvs} -q update" \
"${QUESTION} aa\.c
U aa\.c
${PROG} update: move away \./aa\.c; it is in the way"
	  else
	    dotest_fail conflicts2-142d2 "${testcvs} -q update" \
"${PROG} [a-z]*: move away aa\.c; it is in the way
C aa\.c"
	  fi
	  cd ../..

	  rm -rf 1 2 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modules)
	  rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  mkdir 1
	  cd 1

	  if ${testcvs} -q co first-dir; then
	    echo 'PASS: test 143' >>${LOGFILE}
	  else
	    echo 'FAIL: test 143' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd first-dir
	  mkdir subdir
	  ${testcvs} add subdir >>${LOGFILE}
	  cd subdir

	  mkdir ssdir
	  ${testcvs} add ssdir >>${LOGFILE}

	  touch a b

	  if ${testcvs} add a b 2>>${LOGFILE} ; then
	    echo 'PASS: test 144' >>${LOGFILE}
	  else
	    echo 'FAIL: test 144' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 145' >>${LOGFILE}
	  else
	    echo 'FAIL: test 145' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd ..
	  if ${testcvs} -q co CVSROOT >>${LOGFILE}; then
	    echo 'PASS: test 146' >>${LOGFILE}
	  else
	    echo 'FAIL: test 146' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  # Here we test that CVS can deal with CVSROOT (whose repository
	  # is at top level) in the same directory as subdir (whose repository
	  # is a subdirectory of first-dir).  TODO: Might want to check that
	  # files can actually get updated in this state.
	  if ${testcvs} -q update; then
	    echo 'PASS: test 147' >>${LOGFILE}
	  else
	    echo 'FAIL: test 147' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  echo realmodule first-dir/subdir a >>CVSROOT/modules
	  echo dirmodule first-dir/subdir >>CVSROOT/modules
	  echo namedmodule -d nameddir first-dir/subdir >>CVSROOT/modules
	  echo aliasmodule -a first-dir/subdir/a >>CVSROOT/modules
	  echo aliasnested -a first-dir/subdir/ssdir >>CVSROOT/modules
	  echo topfiles -a first-dir/file1 first-dir/file2 >>CVSROOT/modules
	  echo world -a . >>CVSROOT/modules

	  # Options must come before arguments.  It is possible this should
	  # be relaxed at some point (though the result would be bizarre for
	  # -a); for now test the current behavior.
	  echo bogusalias first-dir/subdir/a -a >>CVSROOT/modules
	  if ${testcvs} ci -m 'add modules' CVSROOT/modules \
	      >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 148' >>${LOGFILE}
	  else
	    echo 'FAIL: test 148' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd ..
	  dotest 148a0 "${testcvs} co -c" 'CVSROOT      CVSROOT
aliasmodule  -a first-dir/subdir/a
aliasnested  -a first-dir/subdir/ssdir
bogusalias   first-dir/subdir/a -a
dirmodule    first-dir/subdir
namedmodule  -d nameddir first-dir/subdir
realmodule   first-dir/subdir a
topfiles     -a first-dir/file1 first-dir/file2
world        -a .'
	  # I don't know why aliasmodule isn't printed (I would have thought
	  # that it gets printed without the -a; although I'm not sure that
	  # printing expansions without options is useful).
	  dotest 148a1 "${testcvs} co -s" 'CVSROOT      NONE        CVSROOT
bogusalias   NONE        first-dir/subdir/a -a
dirmodule    NONE        first-dir/subdir
namedmodule  NONE        first-dir/subdir
realmodule   NONE        first-dir/subdir a'

	  # Test that real modules check out to realmodule/a, not subdir/a.
	  if ${testcvs} co realmodule >>${LOGFILE}; then
	    echo 'PASS: test 149a1' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a1' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -d realmodule && test -f realmodule/a; then
	    echo 'PASS: test 149a2' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a2' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -f realmodule/b; then
	    echo 'FAIL: test 149a3' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 149a3' >>${LOGFILE}
	  fi
	  if ${testcvs} -q co realmodule; then
	    echo 'PASS: test 149a4' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a4' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if echo "yes" | ${testcvs} release -d realmodule >>${LOGFILE} ; then
	    echo 'PASS: test 149a5' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a5' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  # Now test the ability to check out a single file from a directory
	  if ${testcvs} co dirmodule/a >>${LOGFILE}; then
	    echo 'PASS: test 150c' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150c' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -d dirmodule && test -f dirmodule/a; then
	    echo 'PASS: test 150d' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150d' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -f dirmodule/b; then
	    echo 'FAIL: test 150e' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 150e' >>${LOGFILE}
	  fi
	  if echo "yes" | ${testcvs} release -d dirmodule >>${LOGFILE} ; then
	    echo 'PASS: test 150f' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150f' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  # Now test the ability to correctly reject a non-existent filename.
	  # For maximum studliness we would check that an error message is
	  # being output.
	  if ${testcvs} co dirmodule/nonexist >>${LOGFILE} 2>&1; then
	    # We accept a zero exit status because it is what CVS does
	    # (Dec 95).  Probably the exit status should be nonzero,
	    # however.
	    echo 'PASS: test 150g1' >>${LOGFILE}
	  else
	    echo 'PASS: test 150g1' >>${LOGFILE}
	  fi
	  # We tolerate the creation of the dirmodule directory, since that
	  # is what CVS does, not because we view that as preferable to not
	  # creating it.
	  if test -f dirmodule/a || test -f dirmodule/b; then
	    echo 'FAIL: test 150g2' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 150g2' >>${LOGFILE}
	  fi
	  rm -rf dirmodule

	  # Now test that a module using -d checks out to the specified
	  # directory.
	  dotest 150h1 "${testcvs} -q co namedmodule" 'U nameddir/a
U nameddir/b'
	  if test -f nameddir/a && test -f nameddir/b; then
	    pass 150h2
	  else
	    fail 150h2
	  fi
	  echo add line >>nameddir/a
	  dotest 150h3 "${testcvs} -q co namedmodule" 'M nameddir/a'
	  rm nameddir/a
	  dotest 150h4 "${testcvs} -q co namedmodule" 'U nameddir/a'
	  if echo "yes" | ${testcvs} release -d nameddir >>${LOGFILE} ; then
	    pass 150h99
	  else
	    fail 150h99
	  fi

	  # Now test that alias modules check out to subdir/a, not
	  # aliasmodule/a.
	  if ${testcvs} co aliasmodule >>${LOGFILE}; then
	    echo 'PASS: test 151' >>${LOGFILE}
	  else
	    echo 'FAIL: test 151' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -d aliasmodule; then
	    echo 'FAIL: test 152' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 152' >>${LOGFILE}
	  fi
	  echo abc >>first-dir/subdir/a
	  if (${testcvs} -q co aliasmodule | tee test153.tmp) \
	      >>${LOGFILE}; then
	    echo 'PASS: test 153' >>${LOGFILE}
	  else
	    echo 'FAIL: test 153' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  echo 'M first-dir/subdir/a' >ans153.tmp
	  if cmp test153.tmp ans153.tmp; then
	    echo 'PASS: test 154' >>${LOGFILE}
	  else
	    echo 'FAIL: test 154' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd ..
	  rm -rf 1

	  mkdir 2
	  cd 2
	  dotest modules-155a0 "${testcvs} co aliasnested" \
"${PROG} [a-z]*: Updating first-dir/subdir/ssdir"
	  dotest modules-155a1 "test -d first-dir" ''
	  dotest modules-155a2 "test -d first-dir/subdir" ''
	  dotest modules-155a3 "test -d first-dir/subdir/ssdir" ''
	  # Test that nothing extraneous got created.
	  dotest modules-155a4 "ls" "first-dir"
	  cd ..
	  rm -rf 2

	  # Test checking out everything.
	  mkdir 1
	  cd 1
	  dotest modules-155b "${testcvs} -q co world" \
"U CVSROOT/modules
U first-dir/subdir/a
U first-dir/subdir/b"
	  cd ..
	  rm -rf 1

	  # Test checking out a module which lists at least two
	  # specific files twice.  At one time, this failed over
	  # remote CVS.
	  mkdir 1
	  cd 1
	  dotest modules-155c1 "${testcvs} -q co first-dir" \
"U first-dir/subdir/a
U first-dir/subdir/b"

	  cd first-dir
	  echo 'first revision' > file1
	  echo 'first revision' > file2
	  dotest modules-155c2 "${testcvs} add file1 file2" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest modules-155c3 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file2,v
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done'

	  cd ..
	  rm -rf first-dir
	  dotest modules-155c4 "${testcvs} -q co topfiles" \
"U first-dir/file1
U first-dir/file2"
	  dotest modules-155c5 "${testcvs} -q co topfiles" ""
	  cd ..
	  rm -rf 1

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;
	mflag)
	  for message in '' ' ' '	
           ' '    	  	test' ; do
	    # Set up
	    mkdir a-dir; cd a-dir
	    # Test handling of -m during import
	    echo testa >>test
	    if ${testcvs} import -m "$message" a-dir A A1 >>${LOGFILE} 2>&1;then
	      echo 'PASS: test 156' >>${LOGFILE}
	    else
	      echo 'FAIL: test 156' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Must import twice since the first time uses inline code that
	    # avoids RCS call.
	    echo testb >>test
	    if ${testcvs} import -m "$message" a-dir A A2 >>${LOGFILE} 2>&1;then
	      echo 'PASS: test 157' >>${LOGFILE}
	    else
	      echo 'FAIL: test 157' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Test handling of -m during ci
	    cd ..; rm -rf a-dir;
	    if ${testcvs} co a-dir >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 158' >>${LOGFILE}
	    else
	      echo 'FAIL: test 158' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    cd a-dir
	    echo testc >>test
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 159' >>${LOGFILE}
	    else
	      echo 'FAIL: test 159' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Test handling of -m during rm/ci
	    rm test;
	    if ${testcvs} rm test >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 160' >>${LOGFILE}
	    else
	      echo 'FAIL: test 160' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 161' >>${LOGFILE}
	    else
	      echo 'FAIL: test 161' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Clean up
	    cd ..; rm -rf a-dir ${CVSROOT_DIRNAME}/a-dir
	  done
	  ;;
	errmsg1)
	  mkdir ${CVSROOT_DIRNAME}/1dir
	  mkdir 1
	  cd 1
	  if ${testcvs} -q co 1dir; then
	    echo 'PASS: test 162' >>${LOGFILE}
	  else
	    echo 'FAIL: test 162' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd 1dir
	  touch foo
	  if ${testcvs} add foo 2>>${LOGFILE}; then
	    echo 'PASS: test 163' >>${LOGFILE}
	  else
	    echo 'FAIL: test 163' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 164' >>${LOGFILE}
	  else
	    echo 'FAIL: test 164' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd ../..
	  mkdir 2
	  cd 2
	  if ${testcvs} -q co 1dir >>${LOGFILE}; then
	    echo 'PASS: test 165' >>${LOGFILE}
	  else
	    echo 'FAIL: test 165' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  chmod a-w 1dir
	  cd ../1/1dir
	  rm foo;
	  if ${testcvs} rm foo >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 166' >>${LOGFILE}
	  else
	    echo 'FAIL: test 166' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} ci -m removed >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 167' >>${LOGFILE}
	  else
	    echo 'FAIL: test 167' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd ../../2/1dir
	  # FIXME: should be using dotest and PROG.
	  ${testcvs} -q update 2>../tst167.err
	  CVSBASE=`basename $testcvs`	# Get basename of CVS executable.
	  cat <<EOF >../tst167.ans
$CVSBASE server: warning: foo is not (any longer) pertinent
$CVSBASE update: unable to remove ./foo: Permission denied
EOF
	  if cmp ../tst167.ans ../tst167.err >/dev/null ||
	  ( echo "$CVSBASE [update aborted]: cannot rename file foo to CVS/,,foo: Permission denied" | cmp - ../tst167.err >/dev/null )
	  then
	    echo 'PASS: test 168' >>${LOGFILE}
	  else
	    echo 'FAIL: test 168' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd ..
	  chmod u+w 1dir
	  cd ..
	  rm -rf 1 2 ${CVSROOT_DIRNAME}/1dir
	  ;;

	devcom)
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  if ${testcvs} -q co first-dir >>${LOGFILE} ; then
	    echo 'PASS: test 169' >>${LOGFILE}
	  else
	    echo 'FAIL: test 169' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd first-dir
	  echo abb >abb
	  if ${testcvs} add abb 2>>${LOGFILE}; then
	    echo 'PASS: test 170' >>${LOGFILE}
	  else
	    echo 'FAIL: test 170' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 171' >>${LOGFILE}
	  else
	    echo 'FAIL: test 171' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  dotest_fail 171a0 "${testcvs} watch" "Usage${DOTSTAR}"
	  if ${testcvs} watch on; then
	    echo 'PASS: test 172' >>${LOGFILE}
	  else
	    echo 'FAIL: test 172' | tee -a ${LOGFILE}
	  fi
	  echo abc >abc
	  if ${testcvs} add abc 2>>${LOGFILE}; then
	    echo 'PASS: test 173' >>${LOGFILE}
	  else
	    echo 'FAIL: test 173' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 174' >>${LOGFILE}
	  else
	    echo 'FAIL: test 174' | tee -a ${LOGFILE}
	  fi

	  cd ../..
	  mkdir 2
	  cd 2

	  if ${testcvs} -q co first-dir >>${LOGFILE}; then
	    echo 'PASS: test 175' >>${LOGFILE}
	  else
	    echo 'FAIL: test 175' | tee -a ${LOGFILE}
	  fi
	  cd first-dir
	  if test -w abb; then
	    echo 'FAIL: test 176' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 176' >>${LOGFILE}
	  fi
	  if test -w abc; then
	    echo 'FAIL: test 177' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 177' >>${LOGFILE}
	  fi

	  if ${testcvs} editors >../ans178.tmp; then
	    echo 'PASS: test 178' >>${LOGFILE}
	  else
	    echo 'FAIL: test 178' | tee -a ${LOGFILE}
	  fi
	  cat ../ans178.tmp >>${LOGFILE}
	  if test -s ../ans178.tmp; then
	    echo 'FAIL: test 178a' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 178a' >>${LOGFILE}
	  fi

	  if ${testcvs} edit abb; then
	    echo 'PASS: test 179' >>${LOGFILE}
	  else
	    echo 'FAIL: test 179' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  if ${testcvs} editors >../ans180.tmp; then
	    echo 'PASS: test 180' >>${LOGFILE}
	  else
	    echo 'FAIL: test 180' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cat ../ans180.tmp >>${LOGFILE}
	  if test -s ../ans180.tmp; then
	    echo 'PASS: test 181' >>${LOGFILE}
	  else
	    echo 'FAIL: test 181' | tee -a ${LOGFILE}
	  fi

	  echo aaaa >>abb
	  if ${testcvs} ci -m modify abb >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 182' >>${LOGFILE}
	  else
	    echo 'FAIL: test 182' | tee -a ${LOGFILE}
	  fi
	  # Unedit of a file not being edited should be a noop.
	  dotest 182.5 "${testcvs} unedit abb" ''

	  if ${testcvs} editors >../ans183.tmp; then
	    echo 'PASS: test 183' >>${LOGFILE}
	  else
	    echo 'FAIL: test 183' | tee -a ${LOGFILE}
	  fi
	  cat ../ans183.tmp >>${LOGFILE}
	  if test -s ../ans183.tmp; then
	    echo 'FAIL: test 184' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 184' >>${LOGFILE}
	  fi

	  if test -w abb; then
	    echo 'FAIL: test 185' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 185' >>${LOGFILE}
	  fi

	  if ${testcvs} edit abc; then
	    echo 'PASS: test 186a1' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a1' | tee -a ${LOGFILE}
	  fi
	  # Unedit of an unmodified file.
	  if ${testcvs} unedit abc; then
	    echo 'PASS: test 186a2' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a2' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} edit abc; then
	    echo 'PASS: test 186a3' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a3' | tee -a ${LOGFILE}
	  fi
	  echo changedabc >abc
	  # Try to unedit a modified file; cvs should ask for confirmation
	  if (echo no | ${testcvs} unedit abc) >>${LOGFILE}; then
	    echo 'PASS: test 186a4' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a4' | tee -a ${LOGFILE}
	  fi
	  if echo changedabc | cmp - abc; then
	    echo 'PASS: test 186a5' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a5' | tee -a ${LOGFILE}
	  fi
	  # OK, now confirm the unedit
	  if (echo yes | ${testcvs} unedit abc) >>${LOGFILE}; then
	    echo 'PASS: test 186a6' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a6' | tee -a ${LOGFILE}
	  fi
	  if echo abc | cmp - abc; then
	    echo 'PASS: test 186a7' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a7' | tee -a ${LOGFILE}
	  fi

	  dotest devcom-a0 "${testcvs} watchers" ''
	  dotest devcom-a1 "${testcvs} watch add" ''
	  dotest devcom-a2 "${testcvs} watchers" \
'abb	[a-z0-9]*	edit	unedit	commit
abc	[a-z0-9]*	edit	unedit	commit'
	  dotest devcom-a3 "${testcvs} watch remove -a unedit abb" ''
	  dotest devcom-a4 "${testcvs} watchers abb" \
'abb	[a-z0-9]*	edit	commit'

	  # Check tagging and checking out while we have a CVS
	  # directory in the repository.
	  dotest devcom-t0 "${testcvs} -q tag tag" \
'T abb
T abc'
	  cd ../..
	  mkdir 3
	  cd 3
	  dotest devcom-t1 "${testcvs} -q co -rtag first-dir/abb" \
'U first-dir/abb'

	  # Now remove all the file attributes
	  cd ../2/first-dir
	  dotest devcom-b0 "${testcvs} watch off" ''
	  dotest devcom-b1 "${testcvs} watch remove" ''
	  # Test that CVS 1.6 and earlier can handle the repository.
	  dotest_fail devcom-b2 "test -d ${CVSROOT_DIRNAME}/first-dir/CVS"

	  cd ../..
	  rm -rf 1 2 3 ${CVSROOT_DIRNAME}/first-dir
	  ;;

	ignore)
	  dotest 187a1 "${testcvs} -q co CVSROOT" 'U CVSROOT/modules'
	  cd CVSROOT
	  echo rootig.c >cvsignore
	  dotest 187a2 "${testcvs} add cvsignore" "${PROG}"' [a-z]*: scheduling file `cvsignore'"'"' for addition
'"${PROG}"' [a-z]*: use '"'"'cvs commit'"'"' to add this file permanently'

	  # As of Jan 96, local CVS prints "Examining ." and remote doesn't.
	  # Accept either.
	  dotest 187a3 " ${testcvs} ci -m added" \
"${DOTSTAR}"'CS file: /tmp/cvs-sanity/cvsroot/CVSROOT/cvsignore,v
done
Checking in cvsignore;
/tmp/cvs-sanity/cvsroot/CVSROOT/cvsignore,v  <--  cvsignore
initial revision: 1\.1
done
'"${PROG}"' [a-z]*: Rebuilding administrative file database'

	  cd ..
	  if echo "yes" | ${testcvs} release -d CVSROOT >>${LOGFILE} ; then
	    echo 'PASS: test 187a4' >>${LOGFILE}
	  else
	    echo 'FAIL: test 187a4' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  # CVS looks at the home dir from getpwuid, not HOME (is that correct
	  # behavior?), so this is hard to test and we won't try.
	  # echo foobar.c >${HOME}/.cvsignore
	  CVSIGNORE=envig.c; export CVSIGNORE
	  mkdir dir-to-import
	  cd dir-to-import
	  touch foobar.c bar.c rootig.c defig.o envig.c optig.c
	  # We really should allow the files to be listed in any order.
	  # But we (kludgily) just list the orders which have been observed.
	  dotest 188a "${testcvs} import -m m -I optig.c first-dir tag1 tag2" \
	    'N first-dir/foobar.c
N first-dir/bar.c
I first-dir/rootig.c
I first-dir/defig.o
I first-dir/envig.c
I first-dir/optig.c

No conflicts created by this import' 'I first-dir/defig.o
I first-dir/envig.c
I first-dir/optig.c
N first-dir/foobar.c
N first-dir/bar.c
I first-dir/rootig.c

No conflicts created by this import'
	  dotest 188b "${testcvs} import -m m -I ! second-dir tag3 tag4" \
	    'N second-dir/foobar.c
N second-dir/bar.c
N second-dir/rootig.c
N second-dir/defig.o
N second-dir/envig.c
N second-dir/optig.c

No conflicts created by this import'
	  cd ..
	  rm -rf dir-to-import

	  dotest 189a "${testcvs} -q co second-dir" \
'U second-dir/bar.c
U second-dir/defig.o
U second-dir/envig.c
U second-dir/foobar.c
U second-dir/optig.c
U second-dir/rootig.c'
	  dotest 189b "${testcvs} -q co first-dir" 'U first-dir/bar.c
U first-dir/foobar.c'
	  cd first-dir
	  touch rootig.c defig.o envig.c optig.c notig.c
	  dotest 189c "${testcvs} -q update -I optig.c" "${QUESTION} notig.c"
	  # The fact that CVS requires us to specify -I CVS here strikes me
	  # as a bug.
	  dotest 189d "${testcvs} -q update -I ! -I CVS" "${QUESTION} rootig.c
${QUESTION} defig.o
${QUESTION} envig.c
${QUESTION} optig.c
${QUESTION} notig.c"

	  # Now test that commands other than update also print "? notig.c"
	  # where appropriate.  Only test this for remote, because local
	  # CVS only prints it on update.
	  rm optig.c
	  if test "x$remote" = xyes; then
	    dotest 189e "${testcvs} -q diff" "${QUESTION} notig.c"

	    # Force the server to be contacted.  Ugh.  Having CVS
	    # contact the server for the sole purpose of checking
	    # the CVSROOT/cvsignore file does not seem like such a
	    # good idea, so I imagine this will continue to be
	    # necessary.  Oh well, at least we test CVS's ablity to
	    # handle a file with a modified timestamp but unmodified
	    # contents.
	    touch bar.c

	    dotest 189f "${testcvs} -q ci -m commit-it" "${QUESTION} notig.c"
	  fi

	  # now test .cvsignore files
	  cd ..
	  echo notig.c >first-dir/.cvsignore
	  echo foobar.c >second-dir/.cvsignore
	  touch first-dir/notig.c second-dir/notig.c second-dir/foobar.c
	  dotest 190 "${testcvs} -qn update" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/notig.c
${QUESTION} second-dir/.cvsignore"
	  dotest 191 "${testcvs} -qn update -I!" \
"${QUESTION} first-dir/CVS
${QUESTION} first-dir/rootig.c
${QUESTION} first-dir/defig.o
${QUESTION} first-dir/envig.c
${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/CVS
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c" \
"${QUESTION} first-dir/CVS
${QUESTION} first-dir/rootig.c
${QUESTION} first-dir/defig.o
${QUESTION} first-dir/envig.c
${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/CVS
${QUESTION} second-dir/notig.c
${QUESTION} second-dir/.cvsignore"

	  rm -rf first-dir second-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir ${CVSROOT_DIRNAME}/second-dir
	  ;;

	binfiles)
	  # Test cvs's ability to handle binary files.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  dotest binfiles-1 "${testcvs} -q co first-dir" ''
	  awk 'BEGIN { printf "%c%c%c%c%c%c", 2, 10, 137, 0, 13, 10 }' \
	    </dev/null >binfile.dat
	  cat binfile.dat binfile.dat >binfile2.dat
	  cd first-dir
	  cp ../binfile.dat binfile
	  dotest binfiles-2 "${testcvs} add -kb binfile" \
"${PROG}"' [a-z]*: scheduling file `binfile'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest binfiles-3 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/binfile,v
done
Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
initial revision: 1\.1
done'
	  cd ../..
	  mkdir 2; cd 2
	  dotest binfiles-4 "${testcvs} -q co first-dir" 'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-5 "cmp ../../1/binfile.dat binfile" ''
	  # Testing that sticky options is -kb is the closest thing we have
	  # to testing that binary files work right on non-unix machines
	  # (until there is automated testing for such machines, of course).
	  dotest binfiles-5.5 "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'

	  # Test whether the default options from the RCS file are
	  # also used when operating on files instead of whole
	  # directories
          cd ../..
	  mkdir 3; cd 3
	  dotest binfiles-5.5b0 "${testcvs} -q co first-dir/binfile" \
'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-5.5b1 "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  cd ../..
	  rm -rf 3
	  cd 2/first-dir

	  cp ../../1/binfile2.dat binfile
	  dotest binfiles-6 "${testcvs} -q ci -m modify-it" \
'Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.2; previous revision: 1\.1
done'
	  cd ../../1/first-dir
	  dotest binfiles-7 "${testcvs} -q update" '[UP] binfile'
	  dotest binfiles-8 "cmp ../binfile2.dat binfile" ''

	  # Now test handling of conflicts with binary files.
	  cp ../binfile.dat binfile
	  dotest binfiles-con0 "${testcvs} -q ci -m modify-it" \
'Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.3; previous revision: 1\.2
done'
	  cd ../../2/first-dir
	  echo 'edits in dir 2' >binfile
	  dotest binfiles-con1 "${testcvs} -q update" \
'U binfile
cvs [a-z]*: binary file needs merge
cvs [a-z]*: revision 1\.3 from repository is now in binfile
cvs [a-z]*: file from working directory is now in \.#binfile\.1\.2
C binfile'
	  dotest binfiles-con2 "cmp binfile ../../1/binfile.dat" ''
	  dotest binfiles-con3 "cat .#binfile.1.2" 'edits in dir 2'

	  cp ../../1/binfile2.dat binfile
	  dotest binfiles-con4 "${testcvs} -q ci -m resolve-it" \
'Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.4; previous revision: 1\.3
done'
	  cd ../../1/first-dir
	  dotest binfiles-con5 "${testcvs} -q update" '[UP] binfile'

	  # The bugs which these test for are apparently not fixed for remote.
	  if test "$remote" = no; then
	    dotest binfiles-9 "${testcvs} -q update -A" ''
	    dotest binfiles-10 "${testcvs} -q update -kk" '[UP] binfile'
	    dotest binfiles-11 "${testcvs} -q update" ''
	    dotest binfiles-12 "${testcvs} -q update -A" '[UP] binfile'
	    dotest binfiles-13 "${testcvs} -q update -A" ''
	  fi

	  cd ../..
	  rm -rf 1

	  mkdir 3
	  cd 3
	  dotest binfiles-13a0 "${testcvs} -q co -r HEAD first-dir" \
'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-13a1 "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.4.*
   Repository revision:	1\.4	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		HEAD (revision: 1\.4)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  cd ../..
	  rm -rf 3

	  cd 2/first-dir
	  echo 'this file is $''RCSfile$' >binfile
	  dotest binfiles-14a "${testcvs} -q ci -m modify-it" \
'Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.5; previous revision: 1\.4
done'
	  dotest binfiles-14b "cat binfile" 'this file is $''RCSfile$'
	  # See binfiles-5.5 for discussion of -kb.
	  dotest binfiles-14c "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  dotest binfiles-14d "${testcvs} admin -kv binfile" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/binfile,v
done'
	  # cvs admin doesn't change the checked-out file or its sticky
	  # kopts.  There probably should be a way which does (but
	  # what if the file is modified?  And do we try to version
	  # control the kopt setting?)
	  dotest binfiles-14e "cat binfile" 'this file is $''RCSfile$'
	  dotest binfiles-14f "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  dotest binfiles-14g "${testcvs} -q update -A" '[UP] binfile'
	  dotest binfiles-14h "cat binfile" 'this file is binfile,v'
	  dotest binfiles-14i "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kv'

	  # Do sticky options work when used with 'cvs update'?
	  echo "Not a binary file." > nibfile
	  dotest binfiles-sticky1 "${testcvs} -q add nibfile" \
	    'cvs [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest binfiles-sticky2 "${testcvs} -q ci -m add-it nibfile" \
	    'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/nibfile,v
done
Checking in nibfile;
/tmp/cvs-sanity/cvsroot/first-dir/nibfile,v  <--  nibfile
initial revision: 1\.1
done'
	  dotest binfiles-sticky3 "${testcvs} -q update -kb nibfile" \
	    '[UP] nibfile'
	  dotest binfiles-sticky4 "${testcvs} -q status nibfile" \
'===================================================================
File: nibfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	/tmp/cvs-sanity/cvsroot/first-dir/nibfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  # Eventually we should test that -A removes the -kb here...

	  cd ../..
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 2
	  ;;

	binwrap)
	  # Test the ability to specify binary-ness based on file name.
	  # We could also be testing the ability to use the other
	  # ways to specify a wrapper (CVSROOT/cvswrappers, etc.).

	  mkdir dir-to-import
	  cd dir-to-import
	  touch foo.c foo.exe
	  if ${testcvs} import -m message -I ! -W "*.exe -k 'b'" \
	      first-dir tag1 tag2 >>${LOGFILE}; then
	    pass binwrap-1
	  else
	    fail binwrap-1
	  fi
	  cd ..
	  rm -rf dir-to-import
	  dotest binwrap-2 "${testcvs} -q co first-dir" 'U first-dir/foo.c
U first-dir/foo.exe'
	  dotest binwrap-3 "${testcvs} -q status first-dir" \
'===================================================================
File: foo\.c            	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	/tmp/cvs-sanity/cvsroot/first-dir/foo\.c,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: foo\.exe          	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	/tmp/cvs-sanity/cvsroot/first-dir/foo\.exe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
	  ;;

	info)
	  # Test CVS's ability to handle *info files.
	  dotest info-1 "${testcvs} -q co CVSROOT" "[UP] CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  echo "ALL sh -c \"echo x\${=MYENV}\${=OTHER}y\${=ZEE}=\$USER=\$CVSROOT= >>$TESTDIR/testlog; cat >/dev/null\"" > loginfo
	  dotest info-2 "${testcvs} add loginfo" \
"${PROG}"' [a-z]*: scheduling file `loginfo'"'"' for addition
'"${PROG}"' [a-z]*: use '"'"'cvs commit'"'"' to add this file permanently'
	  dotest info-3 "${testcvs} -q ci -m new-loginfo" \
'RCS file: /tmp/cvs-sanity/cvsroot/CVSROOT/loginfo,v
done
Checking in loginfo;
/tmp/cvs-sanity/cvsroot/CVSROOT/loginfo,v  <--  loginfo
initial revision: 1\.1
done
'"${PROG}"' [a-z]*: Rebuilding administrative file database'
	  cd ..
	  if echo "yes" | ${testcvs} release -d CVSROOT >>${LOGFILE} ; then
	    pass info-4
	  else
	    fail info-4
	  fi

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest info-5 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  touch file1
	  dotest info-6 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  echo "cvs -s OTHER=not-this -s MYENV=env-" >>$HOME/.cvsrc
	  dotest info-6a "${testcvs} -q -s OTHER=value ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
'"${PROG}"' [a-z]*: loginfo:1: no such user variable ${=ZEE}'
	  echo line1 >>file1
	  dotest info-7 "${testcvs} -q -s OTHER=value -s ZEE=z ci -m mod-it" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done'
	  cd ..
	  if echo "yes" | ${testcvs} release -d first-dir >>${LOGFILE} ; then
	    pass info-8
	  else
	    fail info-8
	  fi
	  dotest info-9 "cat $TESTDIR/testlog" 'xenv-valueyz=[a-z0-9@][a-z0-9@]*=/tmp/cvs-sanity/cvsroot='

	  # I think this might be doable with cvs remove, or at least
	  # checking in a version with only comments, but I'm too lazy
	  # at the moment.  Blow it away.
	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/loginfo*

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	serverpatch)
	  # Test remote CVS handling of unpatchable files.  This isn't
	  # much of a test for local CVS.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest serverpatch-1 "${testcvs} -q co first-dir" ''

	  cd first-dir

	  # Add a file with an RCS keyword.
	  echo '$''Name$' > file1
	  echo '1' >> file1
	  dotest serverpatch-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'

	  dotest serverpatch-3 "${testcvs} -q commit -m add" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done'

	  # Tag the file.
	  dotest serverpatch-4 "${testcvs} -q tag tag file1" 'T file1'

	  # Check out a tagged copy of the file.
	  cd ../..
	  mkdir 2
	  cd 2
	  dotest serverpatch-5 "${testcvs} -q co -r tag first-dir" \
'U first-dir/file1'

	  # Remove the tag.  This will leave the tag string in the
	  # expansion of the Name keyword.
	  dotest serverpatch-6 "${testcvs} -q update -A" ''

	  # Modify and check in the first copy.
	  cd ../1/first-dir
	  echo '2' >> file1
	  dotest serverpatch-7 "${testcvs} -q ci -mx file1" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done'

	  # Now update the second copy.  When using remote CVS, the
	  # patch will fail, forcing the file to be refetched.
	  cd ../../2/first-dir
	  dotest serverpatch-8 "${testcvs} -q update" \
'U file1' \
'P file1
'"${PROG}"' [a-z]*: checksum failure after patch to ./file1; will refetch
'"${PROG}"' [a-z]*: refetching unpatchable files
U file1'

	  cd ../..
	  rm -rf 1 2 ${CVSROOT_DIRNAME}/first-dir
	  ;;

	log)
	  # Test selecting revisions with cvs log.

	  # Check in a file with a few revisions and branches.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest log-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 'first revision' > file1
	  dotest log-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'

	  dotest log-3 "${testcvs} -q commit -m 1" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done'

	  echo 'second revision' > file1
	  dotest log-4 "${testcvs} -q ci -m2 file1" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done'

	  dotest log-5 "${testcvs} -q tag -b branch file1" 'T file1'

	  echo 'third revision' > file1
	  dotest log-6 "${testcvs} -q ci -m3 file1" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done'

	  dotest log-7 "${testcvs} -q update -r branch" '[UP] file1'

	  echo 'first branch revision' > file1
	  dotest log-8 "${testcvs} -q ci -m1b file1" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done'

	  dotest log-9 "${testcvs} -q tag tag file1" 'T file1'

	  echo 'second branch revision' > file1
	  dotest log-10 "${testcvs} -q ci -m2b file1" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.2; previous revision: 1\.2\.2\.1
done'

	  # Set up a bunch of shell variables to make the later tests
	  # easier to describe.=
	  log_header='
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.3
branch:
locks: strict
access list:'
	  log_tags='symbolic names:
	tag: 1\.2\.2\.1
	branch: 1\.2\.0\.2'
	  log_header2='keyword substitution: kv'
	  log_dash='----------------------------
revision'
	  log_date='date: [0-9/]* [0-9:]*;  author: [a-zA-Z0-9@]*;  state: Exp;'
	  log_lines="  lines: ${PLUS}1 -1"
	  log_rev1="${log_dash} 1\.1
${log_date}
1"
	  log_rev2="${log_dash} 1\.2
${log_date}${log_lines}
branches:  1\.2\.2;
2"
	  log_rev3="${log_dash} 1\.3
${log_date}${log_lines}
3"
	  log_rev1b="${log_dash} 1\.2\.2\.1
${log_date}${log_lines}
1b"
	  log_rev2b="${log_dash} 1\.2\.2\.2
${log_date}${log_lines}
2b"
	  log_trailer='============================================================================='

	  # Now, finally, test the log output.

	  dotest log-11 "${testcvs} log file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-12 "${testcvs} log -N file1" \
"${log_header}
${log_header2}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-13 "${testcvs} log -b file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 3
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-14 "${testcvs} log -r file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest log-15 "${testcvs} log -r1.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev2}
${log_trailer}"

	  dotest log-16 "${testcvs} log -r1.2.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  # This test would fail with the old invocation of rlog, but it
	  # works with the builtin log support.
	  dotest log-17 "${testcvs} log -rbranch file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-18 "${testcvs} log -r1.2.2. file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  # This test would fail with the old invocation of rlog, but it
	  # works with the builtin log support.
	  dotest log-19 "${testcvs} log -rbranch. file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  dotest log-20 "${testcvs} log -r1.2: file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"

	  dotest log-21 "${testcvs} log -r:1.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-22 "${testcvs} log -r1.1:1.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  cd ..
	  rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
	  ;;

	*)
	   echo $what is not the name of a test -- ignored
	   ;;
	esac
done

echo "OK, all tests completed."

# TODO:
# * use "test" not "[" and see if all test's support `-z'
# * Test `cvs admin'.
# * Test `cvs update -d foo' (where foo does not exist).
# * Test `cvs update foo bar' (where foo and bar are both from the same
#   repository).  Suppose one is a branch--make sure that both directories
#   get updated with the respective correct thing.
# * `cvs update ../foo'.  Also ../../foo ./../foo foo/../../bar /foo/bar
#   foo/.././../bar foo/../bar etc.
# * Test all flags in modules file.
#   Test that ciprog gets run both on checkin in that directory, or a
#     higher-level checkin which recurses into it.
# * Test that $ followed by "Header" followed by $ gets expanded on checkin.
# * Test operations on a directory that contains other directories but has
#   no files of its own.
# * -t global option
# * cvs rm followed by cvs add or vice versa (with no checkin in between).
# * cvs rm twice (should be a nice error message).
# * -P option to checkout--(a) refrains from checking out new empty dirs,
#   (b) prunes empty dirs already there.
# * Test that cvs -d `hostname`:/tmp/cvs-sanity/non/existent co foo
#   gives an appropriate error (e.g.
#     Cannot access /tmp/cvs-sanity/non-existent/CVSROOT
#     No such file or directory).
# * Test ability to send notifications in response to watches.  (currently
#   hard to test because CVS doesn't send notifications if username is the
#   same).
# * Test that remote edit and/or unedit works when disconnected from
#   server (e.g. set CVS_SERVER to "foobar").
# * Test things to do with the CVS/* files, esp. CVS/Root....
# End of TODO list.

# Remove the test directory, but first change out of it.
cd /tmp
rm -rf ${TESTDIR}

# end of sanity.sh
