#	$OpenBSD: Makefile,v 1.3 2016/09/18 21:06:46 bluhm Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.

REGRESS_TARGETS = gets

PROG =		test_gets
CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
MAN =
CLEANFILES +=	out_gets.utf8 out_gets.ascii

depend:
	cd ${.CURDIR}/../../../../lib/libedit && make depend
${PROG}: depend
regress: ${PROG}

gets:
	LC_ALL=en_US.UTF-8 ./test_gets < ${.CURDIR}/inp_gets.txt > out_gets.utf8
	diff -au ${.CURDIR}/exp_gets.utf8 out_gets.utf8
	LC_ALL=C ./test_gets < ${.CURDIR}/inp_gets.txt > out_gets.ascii
	diff -u ${.CURDIR}/exp_gets.ascii out_gets.ascii

.include <bsd.regress.mk>
