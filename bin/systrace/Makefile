#	$OpenBSD: Makefile,v 1.2 2002/06/04 17:31:04 provos Exp $

PROG=	systrace
CFLAGS+= -I.
SRCS=	filter.c intercept-translate.c intercept.c \
	openbsd-syscalls.c util.c \
	policy.c systrace-errno.h systrace-error.c \
	systrace-translate.c systrace.c \
	parse.c lex.l
CLEANFILES+= parse.c parse.h
NOMAN=

parse.c: parse.y
	${YACC} -d -o parse.c ${.CURDIR}/parse.y

.include <bsd.prog.mk>
