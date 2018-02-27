# $OpenBSD: Makefile,v 1.3 2018/02/27 07:58:29 mpi Exp $

# Start with a clean /var/account/acct accounting file and turn on
# process accounting with accton(8).  Each test executes a command
# with a unique name and checks the flags in the lastcomm(1) output.
# Run tests with fork, su, core, xsig, pledge, trap accounting.

PROG=		crash
CLEANFILES=	regress-* stamp-*

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
	cp -f /bin/sh regress-fork
	./regress-fork -c '( : ) &'
	lastcomm regress-fork | grep -q ' -F '

TARGETS+=	core
run-regress-core:
	@echo '\n======== $@ ========'
	# Create shell program, abort sub shell, and check the -DX flag.
	cp -f /bin/sh regress-core
	rm -f regress-core.core
	ulimit -c 100000; ./regress-core -c '( : ) & kill -SEGV $$!; wait'
	lastcomm regress-core | grep -q ' -FDX '

TARGETS+=	xsig
run-regress-xsig:
	@echo '\n======== $@ ========'
	# Create shell program, kill sub shell, and check the -X flag.
	cp -f /bin/sh regress-xsig
	./regress-xsig -c '( : ) & kill -KILL $$!; wait'
	lastcomm regress-xsig | grep -q ' -FX '

TARGETS+=	pledge
run-regress-pledge:
	@echo '\n======== $@ ========'
	# Create perl program, violate pledge, and check the -P flag.
	cp -f /usr/bin/perl regress-pledge
	ulimit -c 0; ! ./regress-pledge -MOpenBSD::Pledge -e\
	    'pledge("stdio") or die $$!; chdir("/")'
	lastcomm regress-pledge | grep -q ' -S*XP '

TARGETS+=	trap
run-regress-trap: ${PROG}
	@echo '\n======== $@ ========'
	# Build crashing program, run SIGSEGV handler, and check the -T flag.
	cp -f ${PROG} regress-trap
	./regress-trap
	lastcomm regress-trap | grep -q ' -S*T '

REGRESS_TARGETS=	${TARGETS:S/^/run-regress-/}
${REGRESS_TARGETS}:	stamp-rotate

.include <bsd.regress.mk>
