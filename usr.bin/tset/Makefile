#	$OpenBSD: Makefile,v 1.8 2009/05/06 19:12:45 millert Exp $

PROG=	tset
SRCS=	tset.c dump_entry.c termcap.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES= ${.CURDIR}/../../lib/libcurses
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I. -DHAVE_GETTTYNAM -DHAVE_TTYENT_H
CLEANFILES+= termsort.c
.PATH:  ${TIC}
LINKS=	${BINDIR}/tset ${BINDIR}/reset
MLINKS=	tset.1 reset.1

beforedepend: termsort.c

termsort.c: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
