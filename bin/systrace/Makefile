#	$OpenBSD: Makefile,v 1.7 2002/07/19 14:38:57 itojun Exp $

PROG=	systrace
CFLAGS+=-I.
CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
	-Wno-uninitialized -Werror
CFLAGS+=-DYY_NO_UNPUT
SRCS=	filter.c intercept-translate.c intercept.c \
	openbsd-syscalls.c util.c \
	policy.c systrace-errno.h systrace-error.c \
	systrace-translate.c systrace.c alias.c register.c \
	parse.y lex.l
CLEANFILES+= parse.c lex.c y.tab.h

.include <bsd.prog.mk>

.depend: parse.c lex.c

