# $OpenBSD: Makefile,v 1.2 2002/01/01 23:00:51 art Exp $

REGRESSTARGETS=t1 t2 t3 t4 t5 t6 t7

t1: t1.out
	@cmp -s ${.CURDIR}/testsuite.md5 t1.out || \
	    echo 'MD5 test suite output does not match expected output'

t2: t2.out
	@cmp -s ${.CURDIR}/testsuite.sha1 t2.out || \
	    echo 'SHA1 test suite output does not match expected output'

t3: t3.out
	@cmp -s ${.CURDIR}/testsuite.rmd160 t3.out || \
	    echo 'RMD160 test suite output does not match expected output'

t4:
	@if md5 -s "" /dev/null >/dev/null 2>&1; then \
	    echo "FAILED: md5 should not allow a string on the command line along with a filename"; \
	fi

t5: t5.out
	@cmp -s ${.CURDIR}/pipe_test.md5 t5.out || \
	    echo '"echo hi | md5 -p" does not match expected output'

t6:
	@if echo hi | md5 -p /dev/null >/dev/null 2>&1; then \
	    echo "FAILED: md5 should not allow a filename on the command line in -p mode"; \
	fi

t7:
	@if [ `md5 /dev/null /dev/null | wc -l` != 2 ]; then \
	    echo "FAILED: md5 doesn't handle multiple files"; \
	fi

t1.out:
	md5 -x >$@

t2.out:
	sha1 -x >$@

t3.out:
	rmd160 -x >$@

t5.out:
	echo hi | md5 -p >$@

.PHONY: t1 t2 t3 t4 t5 t6 t7 regress
CLEANFILES+=t1.out t2.out t3.out t5.out

.include <bsd.regress.mk>
