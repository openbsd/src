# $OpenBSD: Makefile,v 1.4 2002/05/30 20:06:23 hin Exp $

KRBDEPTH=	../..
.include <${.CURDIR}/${KRBDEPTH}/Makefile.inc>

PROG=	asn1_compile
NOMAN=
SRCS=	parse.c lex.l main.c hash.c symbol.c gen.c \
        gen_encode.c gen_decode.c gen_free.c gen_length.c gen_copy.c \
        gen_glue.c

CLEANFILES=	parse.[ch] lex.[ch]

parse.h parse.c:	parse.y
	${YACC} -d -o parse.c ${.ALLSRC}
LDADD+=	${LIBROKEN}
CFLAGS+= -I${KRB5SRC}/lib/asn1 -I. -I${KRB5SRC}/lib/roken

test:
	echo ${LIBROKEN}

.PATH:	${KRB5SRC}/lib/asn1 ${KRB5SRC}/lib/roken

.include <bsd.prog.mk>
