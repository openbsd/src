#	$OpenBSD: Makefile,v 1.4 1998/07/23 21:10:24 millert Exp $

CFLAGS+=#-DTFILE=\"/dev/ttyp0\"
CFLAGS+=-D_CURSES_PRIVATE -I${.CURDIR}
CFLAGS+=-DCM_N -DCM_GT -DCM_B -DCM_D
LIB=	ocurses
SRCS=	addbytes.c addch.c addnstr.c box.c clear.c clrtobot.c clrtoeol.c \
	cr_put.c ctrace.c cur_hash.c curses.c delch.c deleteln.c delwin.c \
	erase.c fullname.c getch.c getstr.c id_subwins.c idlok.c initscr.c \
	insch.c insertln.c longname.c move.c mvwin.c newwin.c overlay.c \
	overwrite.c printw.c putchar.c refresh.c scanw.c scroll.c setterm.c \
	standout.c toucholap.c touchwin.c tscroll.c tstp.c tty.c unctrl.c
SRCS+=	termcap.c tgoto.c tputs.c
MAN=	ocurses.3

.if make(install)
SUBDIR+= PSD.doc
.endif

includes:
	-cd ${.CURDIR}; cmp -s curses.h ${DESTDIR}/usr/include/ocurses.h || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    curses.h ${DESTDIR}/usr/include/ocurses.h

# Link libtermcap to libocurses so we don't break people's Makefiles
afterinstall:
	-cd ${DESTDIR}/usr/lib; \
	for i in ${_LIBS}; do \
	    ln -f $$i `echo $$i | sed 's/ocurses/termcap/'`; \
	done

.include <bsd.lib.mk>
