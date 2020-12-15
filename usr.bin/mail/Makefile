#	$OpenBSD: Makefile,v 1.13 2020/12/15 00:50:01 daniel Exp $

PROG=	mail
SRCS=	version.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c \
	edit.c fio.c head.c v7.local.c lex.c list.c main.c names.c \
	popen.c quit.c send.c strings.c temp.c tty.c util.c vars.c
SFILES=	mail.help mail.tildehelp
EFILES=	mail.rc
LINKS=	${BINDIR}/mail ${BINDIR}/Mail ${BINDIR}/mail ${BINDIR}/mailx

distribution:
	cd ${.CURDIR}/misc; ${INSTALL} ${INSTALL_COPY} -o root -g wheel \
	    -m 644 ${EFILES} ${DESTDIR}/etc
	cd ${.CURDIR}/misc; ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
	    -m 444 ${SFILES} ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
