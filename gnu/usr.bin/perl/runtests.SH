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

export PATH || (echo "OOPS, this isn't sh.  Desperation time.  I will feed myself to sh."; sh \$0; kill \$\$)

case \$# in
    0)
	echo "runtests tty_flag ..."
	exit 1
	;;
esac

case \$1 in
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

if test X"\$TESTFILE" = X; then
    TESTFILE=TEST
fi

if test X"\$PERL" = X; then
    PERL=./perl$_exe
    export PERL
fi

cd t

# The second branch is for testing without a tty or controlling terminal,
# see t/op/stat.t
if test \$tty = Y; then
    \$PERL \$TESTFILE \$TEST_ARGS \$TEST_FILES </dev/tty
else
    PERL_SKIP_TTY_TEST=1 \$PERL \$TESTFILE \$TEST_ARGS \$TEST_FILES
fi

echo "Ran tests" > rantests
!GROK!THIS!
$eunicefix runtests
chmod +x runtests
