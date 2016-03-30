#	$OpenBSD: Makefile,v 1.10 2016/03/30 06:38:46 jmc Exp $

PROG=	tset
SRCS=	tset.c dump_entry.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES= ${.CURDIR}/../../lib/libcurses
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I. -DHAVE_GETTTYNAM -DHAVE_TTYENT_H
CLEANFILES+= termsort.c
.PATH:  ${TIC}
LINKS=	${BINDIR}/tset ${BINDIR}/reset

beforedepend: termsort.c

termsort.c: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
