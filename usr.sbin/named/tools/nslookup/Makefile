#	from: @(#)Makefile	5.23 (Berkeley) 6/24/90
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:47:55 deraadt Exp $

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c
OBJS+=	commands.o
MAN=	nslookup.8
LFLAGS=	-I
CFLAGS+=-I${.CURDIR}
LDADD=	-ll
DPADD=	${LIBL}
CLEANFILES+=commands.c lex.yy.c lex.yy.o

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/nslookup.help \
	    ${DESTDIR}/usr/share/misc

.include "../../../Makefile.inc"
.include <bsd.prog.mk>
