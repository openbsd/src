#	$OpenBSD: Makefile,v 1.10 2014/12/10 03:12:38 daniel Exp $
#	$NetBSD: Makefile,v 1.2 1995/02/16 19:38:45 cgd Exp $

PROG=	re
SRCS=	main.c split.c debug.c regcomp.c regerror.c regexec.c regfree.c
.PATH: ${.CURDIR}/../../../../lib/libc/regex
CLEANFILES += t_exhaust

CFLAGS+= -I${.CURDIR}/../../../../lib/libc/regex -DREDEBUG -DPOSIX_MISTAKE

TESTS=	${.CURDIR}/tests

REGRESS_TARGETS = do-reg do-reg-long do-reg-backref
# cannot run with large limits
#REGRESS_TARGETS += do-t_exhaust

do-reg: ${PROG}
	./re < ${TESTS}
do-reg-long: ${PROG}
	./re -el < ${TESTS}
do-reg-backref: ${PROG}
	./re -er < ${TESTS}
#do-t_exhaust: t_exhaust
#	./t_exhaust

.include <bsd.regress.mk>
