#	$OpenBSD: Makefile,v 1.7 1998/07/24 00:10:48 millert Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	edit

OSRCS=	chared.c common.c el.c emacs.c fcns.c hist.c key.c map.c parse.c \
	prompt.c read.c refresh.c search.c sig.c term.c tty.c vi.c help.c

MAN=	editline.3 editrc.5

MLINKS=	editline.3 el_init.3 editline.3 el_end.3 editline.3 el_reset.3 \
	editline.3 el_gets.3 editline.3 el_getc.3 editline.3 el_push.3 \
	editline.3 el_parse.3 editline.3 el_set.3 editline.3 el_source.3 \
	editline.3 el_resize.3 editline.3 el_line.3 \
	editline.3 el_insertstr.3 editline.3 el_deletestr.3 \
	editline.3 history_init.3 editline.3 history_end.3 editline.3 history.3

# For speed and debugging
#SRCS=   ${OSRCS} tokenizer.c history.c
# For protection
SRCS=	editline.c tokenizer.c history.c

CLEANFILES+=common.h emacs.h fcns.h help.h vi.h help.c fcns.c editline.c
CFLAGS+=-I. -I${.CURDIR} 
CFLAGS+=#-DDEBUG_TTY -DDEBUG_KEY -DDEBUG_READ -DDEBUG -DDEBUG_REFRESH
CFLAGS+=#-DDEBUG_PASTE

AHDR=vi.h emacs.h common.h 
ASRC=${.CURDIR}/vi.c ${.CURDIR}/emacs.c ${.CURDIR}/common.c

vi.h: vi.c makelist
	sh ${.CURDIR}/makelist -h ${.CURDIR}/vi.c > ${.TARGET}

emacs.h: emacs.c makelist
	sh ${.CURDIR}/makelist -h ${.CURDIR}/emacs.c > ${.TARGET}

common.h: common.c makelist
	sh ${.CURDIR}/makelist -h ${.CURDIR}/common.c > ${.TARGET}

fcns.h: ${AHDR} makelist
	sh ${.CURDIR}/makelist -fh ${AHDR} > ${.TARGET}

fcns.c: ${AHDR} fcns.h makelist
	sh ${.CURDIR}/makelist -fc ${AHDR} > ${.TARGET}

help.c: ${ASRC} makelist 
	sh ${.CURDIR}/makelist -bc ${ASRC} > ${.TARGET}

help.h: ${ASRC} makelist
	sh ${.CURDIR}/makelist -bh ${ASRC} > ${.TARGET}

editline.c: ${OSRCS}
	sh ${.CURDIR}/makelist -e ${.ALLSRC:T} > ${.TARGET}

.depend: vi.h emacs.h common.h fcns.h help.h help.c
all: vi.h emacs.h common.h fcns.h help.h help.c


test:	libedit.a test.o 
	${CC} ${CFLAGS} ${.ALLSRC} -o ${.TARGET} libedit.a ${LDADD} -lcurses

includes:
	-cd ${.CURDIR}; cmp -s histedit.h ${DESTDIR}/usr/include/histedit.h > \
	    /dev/null 2>&1 || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 histedit.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>
