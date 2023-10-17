#	$OpenBSD: Makefile,v 1.7 2023/10/17 09:52:10 nicm Exp $

PROG=	infocmp
SRCS=	dump_entry.c infocmp.c
CURSES=	${.CURDIR}/../../lib/libcurses
DPADD=	${LIBCURSES}
LDADD=	-lcurses
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
.PATH:  ${TIC}
CLEANFILES+= termsort.h

dump_entry.o: termsort.h

termsort.h: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
