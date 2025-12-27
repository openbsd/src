#	$OpenBSD: Makefile,v 1.1 2025/12/27 22:12:56 kn Exp $
# Test sysctl(8) -f behaviour with single-line sysctl.conf(5) files.

SYSCTL ?=		/sbin/sysctl

# Different combinations of variable, comment and space in one line.
REGRESS_TARGETS =		var var-com var-spc var-spc-com \
				com \
				spc spc-com spc-var
REGRESS_EXPECTED_FAILURES =	spc-var

${REGRESS_TARGETS}:
	${SYSCTL} -f ${.CURDIR}/${.TARGET}.conf 2>&1 | ! grep ^sysctl

.include <bsd.regress.mk>
