#	$OpenBSD: Makefile,v 1.2 2000/02/19 12:07:17 hin Exp $

PROG=	compile_et
SRCS=	compile_et.c error_table.c et_lex.lex.l
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../lib/libcom_err
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES=error_table.h error_table.c y.tab.h et_lex.lex.c

error_table.h error_table.c:	error_table.y
	 ${YACC} -d -o error_table.c ${.CURDIR}/error_table.y

beforedepend:	et_lex.lex.c
error_table.o:	et_lex.lex.c

.include <bsd.prog.mk>
