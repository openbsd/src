#	$OpenBSD: Makefile,v 1.11 2004/01/05 02:55:28 espie Exp $

PROG=	systrace
CFLAGS+=-I. -I${.CURDIR}
CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
	-Wno-uninitialized
CFLAGS+=-DYY_NO_UNPUT
SRCS=	filter.c intercept-translate.c intercept.c \
	openbsd-syscalls.c util.c \
	policy.c systrace-errno.h systrace-error.c \
	systrace-translate.c systrace.c alias.c register.c \
	parse.y lex.l
CLEANFILES+= parse.c lex.c y.tab.h

.include <bsd.prog.mk>

.depend: parse.c lex.c

