# $OpenBSD: Makefile,v 1.3 2003/06/25 21:20:12 deraadt Exp $

PROG=	asn1_compile
NOMAN=
SRCS=	parse.c lex.l main.c hash.c symbol.c gen.c \
        gen_encode.c gen_decode.c gen_free.c gen_length.c gen_copy.c \
        gen_glue.c getarg.c

CFLAGS+=-I. -I$(.CURDIR) -DFALSE=0 -DTRUE=1 -DHAVE_ARC4RANDOM

CLEANFILES=	parse.[ch] lex.c

parse.h parse.c:	parse.y
	${YACC} -d -o parse.c ${.ALLSRC}

.include <bsd.prog.mk>
