#	$OpenBSD: Makefile,v 1.3 2011/06/23 22:46:12 schwarze Exp $

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

termsort.c: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

infocmp.1: infocmp.1tbl
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
