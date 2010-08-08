# $OpenBSD: Makefile,v 1.5 2010/08/08 20:15:32 david Exp $

REGRESS_TARGETS=cksum md5 sha1 sha256 sum md5_string_vs_files md5_pipe
REGRESS_TARGETS+=md5_pipe_vs_file md5_multiple_files

cksum:
	cksum -x | cmp -s ${.CURDIR}/testsuite.cksum /dev/stdin

md5:
	md5 -x | cmp -s ${.CURDIR}/testsuite.md5 /dev/stdin

sha1:
	sha1 -x | cmp -s ${.CURDIR}/testsuite.sha1 /dev/stdin

sha256:
	sha256 -x | cmp -s ${.CURDIR}/testsuite.sha256 /dev/stdin

sum:
	sum -x | cmp -s ${.CURDIR}/testsuite.sum /dev/stdin

#	shouldn't allow files on the command line along with '-s'
md5_string_vs_files:
	if md5 -s "" /dev/null >/dev/null 2>&1; then \
		false ; \
	fi

md5_pipe:
	echo hi | md5 -p | cmp -s ${.CURDIR}/pipe_test.md5 /dev/stdin

#	shouldn't allow files on the command line along with '-p'
md5_pipe_vs_file:
	if echo hi | md5 -p /dev/null >/dev/null 2>&1; then \
		false ; \
	fi

#	should allow multiple files
md5_multiple_files:
	if [ `md5 /dev/null /dev/null | wc -l` != 2 ]; then \
		false ; \
	fi

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
