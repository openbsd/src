#	from: @(#)Makefile	8.2 (Berkeley) 1/25/94
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:45:37 deraadt Exp $

PROG=	mail
CFLAGS+=-DUSE_OLD_TTY
SRCS=	version.c aux.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c edit.c fio.c \
	getname.c head.c v7.local.c lex.c list.c main.c names.c popen.c \
	quit.c send.c strings.c temp.c tty.c vars.c
SFILES=	mail.help mail.tildehelp
EFILES=	mail.rc
LINKS=	${BINDIR}/mail ${BINDIR}/Mail ${BINDIR}/mail ${BINDIR}/mailx
MLINKS=	mail.1 Mail.1 mail.1 mailx.1

beforeinstall:
	cd ${.CURDIR}/misc; install -c -o ${BINOWN} -g ${BINGRP} \
	    -m 444 ${SFILES} ${DESTDIR}/usr/share/misc
	cd ${.CURDIR}/misc; install -c -o root -g wheel \
	    -m 644 ${EFILES} ${DESTDIR}/etc

.if make(install)
SUBDIR+= USD.doc
.endif

.include <bsd.prog.mk>
