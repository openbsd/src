#	$OpenBSD: Makefile,v 1.12 2023/10/17 09:52:10 nicm Exp $

PROG=	tic
SRCS=	dump_entry.c tic.c tparm_type.c transform.c
CURSES=	${.CURDIR}/../../lib/libcurses
DPADD=	${LIBCURSES}
LDADD=	 -lcurses
LINKS=	${BINDIR}/tic ${BINDIR}/captoinfo ${BINDIR}/tic ${BINDIR}/infotocap
CFLAGS+= -I${CURSES} -I${.CURDIR} -I.
CLEANFILES+= termsort.h

dump_entry.o: termsort.h

termsort.h: ${.CURDIR}/MKtermsort.sh
	sh ${.CURDIR}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
