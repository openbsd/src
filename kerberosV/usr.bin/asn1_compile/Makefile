# $OpenBSD: Makefile,v 1.3 2002/02/06 09:13:34 hin Exp $

KRBDEPTH=	../..
.include <${.CURDIR}/${KRBDEPTH}/Makefile.inc>

PROG=	asn1_compile
NOMAN=
SRCS=	parse.c lex.l main.c hash.c symbol.c gen.c \
        gen_encode.c gen_decode.c gen_free.c gen_length.c gen_copy.c \
        gen_glue.c getprogname.c setprogname.c

CLEANFILES=	parse.[ch] lex.[ch]

parse.h parse.c:	parse.y
	${YACC} -d -o parse.c ${.ALLSRC}
LDADD+=	${LIBROKEN}
CFLAGS+= -I${KRB5SRC}/lib/asn1 -I. -I${KRB5SRC}/lib/roken

test:
	echo ${LIBROKEN}

.PATH:	${KRB5SRC}/lib/asn1 ${KRB5SRC}/lib/roken

.include <bsd.prog.mk>
