# $OpenBSD: Makefile,v 1.1.1.1 2017/06/08 17:29:33 bluhm Exp $

# Start with a clean /var/account/acct accounting file and turn on
# process accounting with accton(8).  Each test executes a command
# with a unique name and checks the flags in the lastcomm(1) output.
# Run tests with fork, su, core, xsig, pledge, trap accounting.

PROG=		crash
CLEANFILES=	bin-* stamp-*

.if make (regress) || make (all)
.BEGIN:
	@echo
	rm -f stamp-rotate
.endif

# Rotate accouting files and keep statistics, from /etc/daily.
stamp-rotate:
	@echo '\n======== $@ ========'
	${SUDO} touch /var/account/acct
	-${SUDO} mv -f /var/account/acct.2 /var/account/acct.3
	-${SUDO} mv -f /var/account/acct.1 /var/account/acct.2
	-${SUDO} mv -f /var/account/acct.0 /var/account/acct.1
	${SUDO} cp -f /var/account/acct /var/account/acct.0
	${SUDO} sa -sq
	${SUDO} accton /var/account/acct
	date >$@

TARGETS+=	fork
run-regress-fork:
	@echo '\n======== $@ ========'
	# Create shell program, fork a sub shell, and check the -F flag.
	cp -f /bin/sh bin-fork
	./bin-fork -c '( : ) &'
	lastcomm bin-fork | grep -q ' -F '

TARGETS+=	su
run-regress-su:
	@echo '\n======== $@ ========'
	# Create shell program, run as super user, and check the -S flag.
	cp -f /bin/sh bin-su
	${SUDO} ./bin-su -c ':'
	lastcomm bin-su | grep -q ' -S '

TARGETS+=	core
run-regress-core:
	@echo '\n======== $@ ========'
	# Create shell program, abort sub shell, and check the -DX flag.
	cp -f /bin/sh bin-core
	rm -f bin-core.core
	ulimit -c 100000; ./bin-core -c '( : ) & kill -SEGV $$!'
	lastcomm bin-core | grep -q ' -FDX '

TARGETS+=	xsig
run-regress-xsig:
	@echo '\n======== $@ ========'
	# Create shell program, kill sub shell, and check the -X flag.
	cp -f /bin/sh bin-xsig
	./bin-xsig -c '( : ) & kill -KILL $$!'
	lastcomm bin-xsig | grep -q ' -FX '

TARGETS+=	pledge
run-regress-pledge:
	@echo '\n======== $@ ========'
	# Create perl program, kill sub shell, and check the -X flag.
	cp -f /usr/bin/perl bin-pledge
	ulimit -c 0; ! ./bin-pledge -MOpenBSD::Pledge -e\
	    'pledge("stdio") or die $$!; chdir("/")'
	lastcomm bin-pledge | grep -q ' -S*XP '

TARGETS+=	trap
run-regress-trap: ${PROG}
	@echo '\n======== $@ ========'
	# Create perl program, kill sub shell, and check the -X flag.
	cp -f ${PROG} bin-trap
	./bin-trap
	lastcomm bin-trap | grep -q ' -S*T '

REGRESS_TARGETS=	${TARGETS:S/^/run-regress-/}
${REGRESS_TARGETS}:	stamp-rotate

.include <bsd.regress.mk>
