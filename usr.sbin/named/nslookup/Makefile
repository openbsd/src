#	$OpenBSD: Makefile,v 1.5 2002/05/11 00:20:20 espie Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/02 15:29:56 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:48 vixie Exp 

.PATH:	${.CURDIR}/../nslookup \
	${.CURDIR}/../man

PROG=	nslookup
SRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c commands.l
CFLAGS+= ${INCLUDE} ${CONFIG}
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES+=	commands.c lex.yy.c lex.yy.o
MAN=	nslookup.8

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../nslookup/nslookup.help \
		${DESTDIR}/usr/share/misc/nslookup.help

.include <bsd.prog.mk>
.include "../Makefile.inc"
.include "../../Makefile.inc"
