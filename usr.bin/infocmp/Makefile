#	$OpenBSD: Makefile,v 1.4 2017/06/16 10:25:54 espie Exp $

PROG=	infocmp
SRCS=	infocmp.c dump_entry.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES= ${.CURDIR}/../../lib/libcurses
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
CLEANFILES+= termsort.c infocmp.1
.PATH:  ${TIC}

beforedepend: termsort.c

dump_entry.o: termsort.c

termsort.c: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

infocmp.1: infocmp.1tbl
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
