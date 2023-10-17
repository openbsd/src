#	$OpenBSD: Makefile,v 1.13 2023/10/17 09:52:11 nicm Exp $

PROG=	tset
SRCS=	reset_cmd.c transform.c tset.c tty_settings.c
CURSES=	${.CURDIR}/../../lib/libcurses
DPADD=	${LIBCURSES}
LDADD=	-lcurses
LINKS=	${BINDIR}/tset ${BINDIR}/reset
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
.PATH:  ${TIC}
CLEANFILES+= termsort.h

.include <bsd.prog.mk>
