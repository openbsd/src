#	$OpenBSD: Makefile,v 1.13 2023/10/17 09:52:11 nicm Exp $

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

termsort.h: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
