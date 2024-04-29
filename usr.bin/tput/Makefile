#	$OpenBSD: Makefile,v 1.15 2024/04/29 13:23:19 naddy Exp $

PROG=	tput
SRCS=	clear_cmd.c reset_cmd.c tparm_type.c tput.c transform.c \
	tty_settings.c
CURSES=	${.CURDIR}/../../lib/libcurses
DPADD=	${LIBCURSES}
LDADD=	-lcurses
LINKS=	${BINDIR}/tput ${BINDIR}/clear
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
.PATH:  ${TIC}
CLEANFILES+= termsort.h
MAN=	tput.1 clear.1

termsort.h: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
