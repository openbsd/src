#	$OpenBSD: Makefile,v 1.2 1998/08/12 20:09:17 art Exp $

PROG=	tic
SRCS=	dump_entry.c tic.c
MAN=	tic.1 captoinfo.1tbl
LINKS=	${BINDIR}/tic ${BINDIR}/infotocap ${BINDIR}/tic ${BINDIR}/captoinfo
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES=	${.CURDIR}/../../lib/libcurses
CFLAGS+= -I${CURSES} -I${.CURDIR} -I.
CLEANFILES+= termsort.c

beforedepend: termsort.c

termsort.c: MKtermsort.sh
	sh ${.CURDIR}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
