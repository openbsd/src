#	$OpenBSD: Makefile,v 1.7 1998/12/02 15:45:43 espie Exp $

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
MAN=	ocurses.3 otermcap.3

.if make(install)
SUBDIR+= PSD.doc
.endif

includes:
	-cd ${.CURDIR}; cmp -s curses.h ${DESTDIR}/usr/include/ocurses.h || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    curses.h ${DESTDIR}/usr/include/ocurses.h

# Link libotermcap to libocurses for programs that need full compatibility
# with the old termcap behavior
afterinstall:
	-cd ${DESTDIR}/usr/lib; \
	for i in ${_LIBS}; do \
	    ln -f $$i `echo $$i | sed 's/ocurses/otermcap/'`; \
	done

.include <bsd.lib.mk>
