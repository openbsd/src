#	$OpenBSD: Makefile,v 1.1 2016/03/01 16:12:11 schwarze Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.
#
# Run "cd /usr/src/lib/libedit && make obj && make depend" first.

REGRESS_TARGETS = gets

PROG =		test_gets
CPPFLAGS +=	-DWIDECHAR
CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
MAN =
CLEANFILES +=	out_gets.utf8 out_gets.ascii

gets:
	LC_ALL=en_US.UTF-8 ./test_gets < ${.CURDIR}/inp_gets.txt > out_gets.utf8
	diff -au ${.CURDIR}/exp_gets.utf8 out_gets.utf8
	LC_ALL=C ./test_gets < ${.CURDIR}/inp_gets.txt > out_gets.ascii
	diff -u ${.CURDIR}/exp_gets.ascii out_gets.ascii

.include <bsd.regress.mk>
