#	$OpenBSD: Makefile,v 1.6 2022/07/11 03:11:49 daniel Exp $

PROG=	infocmp
SRCS=	infocmp.c dump_entry.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES= ${.CURDIR}/../../lib/libcurses
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
CLEANFILES+= termsort.c
.PATH:  ${TIC}

dump_entry.o: termsort.c

termsort.c: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
