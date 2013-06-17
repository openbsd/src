# $OpenBSD: Makefile,v 1.1 2013/06/17 19:11:46 robert Exp $

PROG=	compile_et

NOMAN=

SRCS=	roken.h compile_et.c parse.c parse.h lex.l

# libroken
SRCS+=	getarg.c \
	get_window_size.c \
	rand.c \
	strupr.c

# libvers
SRCS+=	print_version.c

parse.c parse.h: parse.y
	${YACC} -d ${.ALLSRC} && mv y.tab.c parse.c && mv y.tab.h parse.h

CFLAGS+= -I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/com_err -I.

CLEANFILES+=	parse.[ch]*

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/com_err ${KRB5DIR}/include ${.CURDIR}/../../include ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/vers
