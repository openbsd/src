#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:50 tholo Exp $

PROG=	compile_et
SRCS=	compile_et.c error_table.y #et_lex.lex.l
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../com_err
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES=error_table.c y.tab.h et_lex.lex.c

beforedepend:	et_lex.lex.c
error_table.o:	et_lex.lex.c

install:

.include <bsd.prog.mk>
