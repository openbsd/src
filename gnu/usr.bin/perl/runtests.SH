#!/bin/sh

case $PERL_CONFIG_SH in
'')
    if test ! -f config.sh; then
	ln ../config.sh . || \
	ln ../../config.sh . || \
	ln ../../../config.sh . || \
	(echo "Can't find config.sh."; exit 1)
    fi 2>/dev/null
    . ./config.sh
    ;;
esac
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting runtests (with variable substitutions)"
rm -f runtests

$spitshell >runtests <<!GROK!THIS!
$startsh -e
# runtests.SH
# 
!GROK!THIS!

## In the following, dollars and backticks do not need the extra backslash.
$spitshell >>runtests <<'!NO!SUBS!'

export PATH || (echo "OOPS, this isn't sh.  Desperation time.  I will feed myself to sh."; sh $0; kill $$)

case $# in
    0)
	echo "runtests tty_flag ..."
	exit 1
	;;
esac

case $1 in
    tty)
	tty=Y
	;;
    no-tty)
	tty=N
	;;
    choose)
	if (true </dev/tty) >/dev/null 2>&1; then
	    tty=Y
	else
	    tty=N
	fi
	;;
    *)
	echo "ttyflag should be one of tty, no-tty or choose"
	exit 1
	;;
esac

if test X"$TESTFILE" = X; then
    TESTFILE=TEST
fi

cd t

# If this is run under an old shell that doesn't automatically 
# update PWD, then we must update it.  Otherwise, t/io/fs.t gets 
# mixed up about what directory we are in.
case "$PWD" in
    '')  ;; # Do nothing if it isn't set at all.
    */t) ;; # Leave it alone if it's properly updated.
    *) PWD=${PWD}/t; export PWD ;;  # Otherwise, fix it.
esac

!NO!SUBS!

## In the following, dollars and backticks do need the extra backslash.
$spitshell >>runtests <<!GROK!THIS!
# The second branch is for testing without a tty or controlling terminal,
# see t/op/stat.t
if test \$tty = Y; then
    ./perl$_exe \$TESTFILE \$TEST_ARGS \$TEST_FILES </dev/tty
else
    PERL_SKIP_TTY_TEST=1 ./perl$_exe \$TESTFILE \$TEST_ARGS \$TEST_FILES
fi

echo "Ran tests" > rantests
!GROK!THIS!
$eunicefix runtests
chmod +x runtests
