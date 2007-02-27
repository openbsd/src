#	$OpenBSD: Makefile,v 1.4 2007/02/27 21:07:40 miod Exp $

PROG=	tic
SRCS=	dump_entry.c tic.c
MAN=	tic.1 captoinfo.1tbl infotocap.1
LINKS=	${BINDIR}/tic ${BINDIR}/infotocap ${BINDIR}/tic ${BINDIR}/captoinfo
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES=	${.CURDIR}/../../lib/libcurses
CFLAGS+= -I${CURSES} -I${.CURDIR} -I.
CLEANFILES+= termsort.c

.if (${MACHINE_ARCH} == sh)
COPTS+=	-O1
.endif

beforedepend: termsort.c

termsort.c: MKtermsort.sh
	sh ${.CURDIR}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
