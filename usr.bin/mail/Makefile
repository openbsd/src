#	$OpenBSD: Makefile,v 1.7 1997/07/28 10:01:12 deraadt Exp $
#	$NetBSD: Makefile,v 1.8 1996/06/08 19:48:09 christos Exp $

PROG=	mail
SRCS=	version.c aux.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c \
	edit.c fio.c getname.c head.c v7.local.c lex.c list.c main.c names.c \
	popen.c quit.c send.c strings.c temp.c tty.c vars.c
SFILES=	mail.help mail.tildehelp
EFILES=	mail.rc
LINKS=	${BINDIR}/mail ${BINDIR}/Mail ${BINDIR}/mail ${BINDIR}/mailx
MLINKS=	mail.1 Mail.1 mail.1 mailx.1

beforeinstall:
	cd ${.CURDIR}/misc; ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
	    -m 444 ${SFILES} ${DESTDIR}/usr/share/misc

distribution:
	cd ${.CURDIR}/misc; ${INSTALL} ${INSTALL_COPY} -o root -g wheel \
	    -m 644 ${EFILES} ${DESTDIR}/etc

.if make(install)
SUBDIR+= USD.doc
.endif

.include <bsd.prog.mk>
