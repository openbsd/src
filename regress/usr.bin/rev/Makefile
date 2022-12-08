#	$OpenBSD: Makefile,v 1.2 2022/12/08 12:01:04 anton Exp $

CLEANFILES = out.ascii.txt out.utf8.txt

REGRESS_TARGETS+=	rev
rev:
	LC_ALL=C rev ${.CURDIR}/in.txt > out.ascii.txt
	diff -u ${.CURDIR}/exp.ascii.txt out.ascii.txt
	LC_ALL=en_US.UTF-8 rev ${.CURDIR}/in.txt > out.utf8.txt
	diff -u ${.CURDIR}/exp.utf8.txt out.utf8.txt

.include <bsd.regress.mk>
