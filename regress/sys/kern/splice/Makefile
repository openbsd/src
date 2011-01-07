#	$OpenBSD: Makefile,v 1.1 2011/01/07 22:06:08 bluhm Exp $

# The following ports must be installed for the args-* regression test:
# p5-IO-Socket-INET6  object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6          Perl defines relating to AF_INET6 sockets

ERRS !=			ls error-*.pl
ARGS !=			ls args-*.pl
TARGETS ?=		${ERRS} ${ARGS}
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
CLEANFILES =		*.log ktrace.out

# Fill out these variables if you want to test socket splicing with
# the relay process running on a remote machine.  You have to specify
# a local and remote ip address for the tcp connections.  To control
# the remote machine you need a hostname for ssh to log in.  All the
# test files must be in the same directory local and remote.
#LOCAL_ADDR ?=
#REMOTE_ADDR ?=
#REMOTE_SSH ?=

# The error tests try to splice unsuitable sockets and check the
# kernel error code.

.for e in ${ERRS}
run-regress-$e: $e
	time perl $e
.endfor

# The arg tests take a perl hash with arguments controlling the
# test parameters.  Generally they consist of client, relay, server.
# The relay.pl test fork these three processes locally.  The remote.pl
# test has local client and server but the relay process is running
# on a remote machine reachable with ssh.  For echo.pl test the
# relay is an echo process and the client and server process share
# the same tcp connection.  All tests can run with a regular userland
# copy relay or with a kernel socket splicing relay.

.for a in ${ARGS}
run-regress-$a: $a
	time perl relay.pl copy $a
	time perl relay.pl splice $a
.if REMOTE_SSH
	time perl remote.pl copy ${LOCAL_ADDR} ${REMOTE_ADDR} ${REMOTE_SSH} $a
	time perl remote.pl splice ${LOCAL_ADDR} ${REMOTE_ADDR} ${REMOTE_SSH} $a
.endif
	time perl echo.pl copy $a
	time perl echo.pl splice $a
.endfor

.include <bsd.regress.mk>
