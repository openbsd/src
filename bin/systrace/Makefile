#	$OpenBSD: Makefile,v 1.8 2002/10/16 15:01:08 itojun Exp $

PROG=	systrace
BINDIR?=        /bin
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

