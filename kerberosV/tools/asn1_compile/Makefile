# $OpenBSD: Makefile,v 1.2 2013/07/02 19:00:10 ajacoutot Exp $

PROG=	asn1_compile
NOMAN=

SRCS=	\
	asn1parse.c \
	asn1parse.h \
	gen.c \
	gen_copy.c \
	gen_decode.c \
	gen_encode.c \
	gen_free.c \
	gen_glue.c \
	gen_length.c \
	gen_seq.c \
	gen_template.c \
	hash.c \
	lex.l \
	main.c \
	roken.h \
	symbol.c

asn1parse.c asn1parse.h: asn1parse.y
	${YACC} -d ${.ALLSRC} && mv y.tab.c asn1parse.c && mv y.tab.h asn1parse.h

# libroken
SRCS+=	ecalloc.c \
	emalloc.c \
	estrdup.c \
	getarg.c \
	get_window_size.c \
	rand.c \
	strupr.c

# libvers
SRCS+=	print_version.c

CPPFLAGS=	-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/com_err -I.

CLEANFILES= lex.c asn1parse.c asn1parse.h

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/asn1 ${KRB5DIR}/include ${.CURDIR}/../../include ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/vers
