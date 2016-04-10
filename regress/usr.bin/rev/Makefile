#	$OpenBSD: Makefile,v 1.1 2016/04/10 15:00:03 schwarze Exp $

CLEANFILES = out.ascii.txt out.utf8.txt

regress:
	LC_ALL=C rev ${.CURDIR}/in.txt > out.ascii.txt
	diff -u ${.CURDIR}/exp.ascii.txt out.ascii.txt
	LC_ALL=en_US.UTF-8 rev ${.CURDIR}/in.txt > out.utf8.txt
	diff -u ${.CURDIR}/exp.utf8.txt out.utf8.txt

.include <bsd.regress.mk>
