# $OpenBSD: varfunction.sh,v 1.1 2003/12/15 05:28:40 otto Exp $
#
# Calling 
#
#	FOO=bar f
#
# where f is a ksh style function, should not set FOO in the current env.
# If f is a bourne style function, FOO should be set. Furthermore,
# the function should receive a correct value of FOO. Additionally,
# setting FOO in the function itself should not change the value in
# global environment.
#
# Inspired by PR 2450.
#
function k {
	if [ x$FOO != xbar ]; then
		echo 1
		return 1
	fi
	x=$(env | grep FOO)
	if [ "x$x" != "xFOO=bar" ]; then
		echo 2
		return 1;
	fi
	FOO=foo
	return 0
}

b () {
	if [ x$FOO != xbar ]; then
		echo 3
		return 1
	fi
	x=$(env | grep FOO)
	if [ "x$x" != "xFOO=bar" ]; then
		echo 4
		return 1;
	fi
	FOO=foo
	return 0
}

FOO=bar k
if [ $? != 0 ]; then
	exit 1
fi
if [ x$FOO != x ]; then
	exit 1
fi

FOO=bar b
if [ $? != 0 ]; then
	exit 1
fi
if [ x$FOO != xbar ]; then
	exit 1
fi

FOO=barbar

FOO=bar k
if [ $? != 0 ]; then
	exit 1
fi
if [ x$FOO != xbarbar ]; then
	exit 1
fi

FOO=bar b
if [ $? != 0 ]; then
	exit 1
fi
if [ x$FOO != xbar ]; then
	exit 1
fi

