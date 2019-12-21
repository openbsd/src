#	$OpenBSD: Makefile,v 1.10 2019/12/21 21:40:01 espie Exp $

PROG=	tic
SRCS=	dump_entry.c tic.c
MAN=	tic.1 captoinfo.1 infotocap.1
LINKS=	${BINDIR}/tic ${BINDIR}/infotocap ${BINDIR}/tic ${BINDIR}/captoinfo
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES=	${.CURDIR}/../../lib/libcurses
CFLAGS+= -I${CURSES} -I${.CURDIR} -I.
CLEANFILES+= termsort.c captoinfo.1

# Work around gcc optimization bug on sh
.if (${MACHINE_ARCH} == sh)
COPTS+=	-O0
.endif

dump_entry.o: termsort.c

termsort.c: MKtermsort.sh
	sh ${.CURDIR}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

captoinfo.1: captoinfo.1tbl
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
