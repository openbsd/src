#	$OpenBSD: Makefile,v 1.3 2002/06/04 19:38:10 provos Exp $

PROG=	systrace
CFLAGS+= -I.
SRCS=	filter.c intercept-translate.c intercept.c \
	openbsd-syscalls.c util.c \
	policy.c systrace-errno.h systrace-error.c \
	systrace-translate.c systrace.c \
	parse.c lex.l
CLEANFILES+= parse.c parse.h

parse.c: parse.y
	${YACC} -d -o parse.c ${.CURDIR}/parse.y

.include <bsd.prog.mk>
