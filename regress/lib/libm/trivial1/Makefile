
PERL=perl
REGRESSTARGETS=test-fp

test-fp: test-fp.pl
	${PERL} ${.CURDIR}/$@.pl | diff -u - ${.CURDIR}/test-fp.out

.include <bsd.regress.mk>
