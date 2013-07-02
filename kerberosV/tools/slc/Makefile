# $OpenBSD: Makefile,v 1.2 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	slc
NOMAN=

SRCS=	roken.h \
	slc-gram.c \
	slc-gram.h \
	slc-lex.l

# libroken
SRCS+=	getarg.c \
	get_window_size.c \
	rand.c \
	strupr.c

slc-gram.c slc-gram.h: slc-gram.y
	${YACC} -d ${.ALLSRC} && mv y.tab.c slc-gram.c && mv y.tab.h slc-gram.h

# libvers
SRCS+=	print_version.c

CPPFLAGS=	-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I${KRB5DIR}/lib/vers -I.

CLEANFILES= slc-gram.c slc-lex.c slc-gram.h

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/sl ${KRB5DIR}/lib/vers ${KRB5DIR}/include ${.CURDIR}/../../include
