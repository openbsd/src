#	$Id: Makefile,v 1.2 1996/10/12 01:13:26 deraadt Exp $

PROG=	compile_et
SRCS=	compile_et.c error_table.y #et_lex.lex.l
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../com_err
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES=error_table.c y.tab.h et_lex.lex.c
CC=${HOSTCC}

beforedepend:	et_lex.lex.c
error_table.o:	et_lex.lex.c

install:

.include <bsd.prog.mk>
