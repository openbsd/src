#	$OpenBSD: Makefile,v 1.9 2003/06/24 16:06:45 mickey Exp $

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

