#	$OpenBSD: Makefile,v 1.7 1996/06/19 02:48:03 niklas Exp $

LIB=    curses
SRCS=	hardscroll.c lib_acs.c lib_delch.c lib_insstr.c lib_options.c \
	lib_set_term.c lib_addch.c lib_delwin.c lib_instr.c lib_overlay.c \
	lib_slk.c lib_addstr.c lib_doupdate.c lib_isendwin.c lib_pad.c \
	lib_touch.c lib_beep.c lib_endwin.c lib_kernel.c lib_printw.c \
	lib_tstp.c lib_bkgd.c lib_erase.c lib_longname.c lib_raw.c \
	lib_twait.c lib_box.c lib_getch.c lib_mouse.c lib_refresh.c \
	lib_unctrl.c lib_clear.c lib_getstr.c lib_move.c lib_resize.c \
	lib_vidattr.c lib_clrbot.c lib_inchstr.c lib_mvcur.c lib_scanw.c \
	lib_window.c lib_clreol.c lib_initscr.c lib_mvwin.c lib_screen.c \
	lib_color.c lib_insch.c lib_newterm.c lib_scroll.c lib_data.c \
	lib_insdel.c lib_keyname.c lib_newwin.c lib_scrreg.c
CFLAGS+= -I. -I${.CURDIR} -DTERMIOS -DEXTERN_TERMINFO
LDADD+=  -ltermlib
MAN=	curs_addch.3 curs_addchstr.3 curs_addstr.3 curs_attr.3 curs_beep.3 \
	curs_bkgd.3 curs_border.3 curs_clear.3 curs_color.3 curs_delch.3 \
	curs_deleteln.3 curs_getch.3 curs_getstr.3 curs_getyx.3 curs_inch.3 \
	curs_inchstr.3 curs_initscr.3 curs_inopts.3 curs_insch.3 curs_insstr.3 \
	curs_instr.3 curs_kernel.3 curs_mouse.3 curs_move.3 curs_outopts.3 \
	curs_overlay.3 curs_pad.3 curs_printw.3 curs_refresh.3 curs_resize.3 \
	curs_scanw.3 curs_scr_dmp.3 curs_scroll.3 curs_slk.3 curs_termattrs.3 \
	curs_touch.3 curs_util.3 curs_window.3 curses.3
MLINKS+=curses.3 ncurses.3
CLEANFILES+= lib_keyname.c keys.tries

beforedepend: keys.tries

includes:
	@cd ${.CURDIR}; for i in unctrl.h; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  install -c -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done
	@cd ${.CURDIR}; cmp -s curses.h ${DESTDIR}/usr/include/curses.h || \
	  install -c -m 444 -o $(BINOWN) -g $(BINGRP) curses.h \
	  ${DESTDIR}/usr/include

keys.tries: ${.CURDIR}/keys.list ${.CURDIR}/MKkeys.awk
	awk -f ${.CURDIR}/MKkeys.awk ${.CURDIR}/keys.list > keys.tries

lib_keyname.c: ${.CURDIR}/keys.list ${.CURDIR}/MKkeyname.awk
	awk -f ${.CURDIR}/MKkeyname.awk ${.CURDIR}/keys.list > lib_keyname.c

.include <bsd.lib.mk>

lib_options.o lib_options.so lib_options.po lib_options.ln: keys.tries
