#	$OpenBSD: Makefile,v 1.1 2019/12/22 07:43:33 jan Exp $

.PHONY: setup-ftpd

REGRESS_TARGETS =	run-ftpd-get
REGRESS_TARGETS +=	run-ftpd-ls
REGRESS_ROOT_TARGETS =	${REGRESS_TARGETS}
REGRESS_CLEANUP =	cleanup-ftpd
REGRESS_SETUP =		setup-ftpd
CLEANFILES =		ftpd.regress

TCPSERVER =		/usr/local/bin/tcpserver
FTPD ?=			/usr/libexec/ftpd

.if ! (make(clean) || make(cleandir) || make(obj))
FTPDIR != getent passwd ftp | cut -d: -f6
.endif

.if ! exists(${TCPSERVER})
regress:
	@echo package ucspi-tcp is required for this regress
	@echo SKIPPED
.endif

setup-ftpd:
	@echo '\n======== $@ ========'
	${SUDO} pkill tcpserver || true
	${SUDO} pkill ftpd || true
	# start ftpd
	${SUDO} ${TCPSERVER} 127.0.0.1 21 ${KTRACE} ${FTPD} -A & \
	    timeout=$$(($$(date +%s) + 5)); \
	    while fstat -p $$! | ! grep -q 'tcp .* 127.0.0.1:21$$'; \
		do test $$(date +%s) -lt $$timeout || exit 1; done
	# prepare test files
	${SUDO} dd if=/dev/random of="${FTPDIR}/ftpd.regress" count=1 bs=1m
	${SUDO} chown ftp:ftp "${FTPDIR}/ftpd.regress"

run-ftpd-get:
	@echo '\n======== $@ ========'
	ftp -a ftp://127.0.0.1/ftpd.regress
	cmp ${FTPDIR}/ftpd.regress ftpd.regress
	rm ftpd.regress

run-ftpd-ls:
	@echo '\n======== $@ ========'
	echo ls | ftp -a 127.0.0.1 | grep -q 'ftpd.regress'

cleanup-ftpd:
	${SUDO} pkill tcpserver || true
	${SUDO} pkill ftpd || true
	${SUDO} rm -f ${FTPDIR}/ftpd.regress

.include <bsd.regress.mk>
