#	$OpenBSD: Makefile,v 1.13 2004/07/09 21:53:55 mickey Exp $

PROG=	systrace
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../sys
CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
	-Wno-uninitialized
CFLAGS+=-DYY_NO_UNPUT
DPADD+=	${LIBEVENT}
LDADD+=	-levent

SRCS=	cradle.c filter.c intercept-translate.c intercept.c \
	openbsd-syscalls.c util.c \
	policy.c systrace-errno.h systrace-error.c \
	systrace-translate.c systrace.c alias.c register.c \
	parse.y lex.l
CLEANFILES+= parse.c lex.c y.tab.h

.include <bsd.prog.mk>

.depend: parse.c lex.c

