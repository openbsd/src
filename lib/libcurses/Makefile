#	$OpenBSD: Makefile,v 1.74 2019/02/13 15:10:40 nicm Exp $

# Uncomment this to enable tracing in libcurses
#CURSESTRACE=-DTRACE

# This is used to compile terminal info directly into the library
FALLBACK_LIST=

# XXX - should be defined elsewhere
AWK?=	/usr/bin/awk

# Search in subdirs
.PATH:	${.CURDIR}/base ${.CURDIR}/tinfo ${.CURDIR}/trace ${.CURDIR}/tty \
	${.CURDIR}/widechar

LIB=    curses
SRCS=	codes.c comp_captab.c expanded.c fallback.c lib_gen.c lib_keyname.c \
	names.c unctrl.c
# base
SRCS+=	define_key.c key_defined.c keybound.c keyok.c legacy_coding.c \
	lib_addch.c lib_addstr.c lib_beep.c lib_bkgd.c lib_box.c lib_chgat.c \
	lib_clear.c lib_clearok.c lib_clrbot.c lib_clreol.c lib_color.c \
	lib_colorset.c lib_delch.c lib_delwin.c lib_dft_fgbg.c lib_echo.c \
	lib_endwin.c lib_erase.c lib_flash.c lib_freeall.c lib_getch.c \
	lib_getstr.c lib_hline.c lib_immedok.c lib_inchstr.c lib_initscr.c \
	lib_insnstr.c lib_insch.c lib_insdel.c lib_instr.c \
	lib_isendwin.c lib_leaveok.c lib_mouse.c lib_move.c lib_mvwin.c \
	lib_newterm.c lib_newwin.c lib_nl.c lib_overlay.c lib_pad.c \
	lib_printw.c lib_redrawln.c lib_refresh.c lib_restart.c \
	lib_scanw.c lib_screen.c lib_scroll.c lib_scrollok.c \
	lib_scrreg.c lib_set_term.c lib_slk.c lib_slkatr_set.c \
	lib_slkatrof.c lib_slkatron.c lib_slkatrset.c lib_slkattr.c \
	lib_slkclear.c lib_slkcolor.c lib_slkinit.c lib_slklab.c \
	lib_slkrefr.c lib_slkset.c lib_slktouch.c lib_touch.c \
	lib_ungetch.c lib_vline.c lib_wattroff.c lib_wattron.c \
	lib_winch.c lib_window.c nc_panel.c resizeterm.c safe_sprintf.c \
	tries.c use_window.c version.c vsscanf.c wresize.c
# tinfo
SRCS+=	access.c add_tries.c alloc_entry.c alloc_ttype.c captoinfo.c \
	comp_error.c comp_expand.c comp_hash.c comp_parse.c comp_scan.c \
	db_iterator.c doalloc.c entries.c free_ttype.c getenv_num.c \
	hashed_db.c home_terminfo.c init_keytry.c lib_acs.c lib_baudrate.c \
	lib_cur_term.c lib_data.c lib_has_cap.c lib_kernel.c lib_longname.c \
	lib_napms.c lib_options.c lib_print.c lib_raw.c lib_setup.c \
	lib_termcap.c lib_termname.c lib_tgoto.c lib_ti.c lib_tparm.c \
	lib_tputs.c lib_ttyflags.c name_match.c parse_entry.c \
	read_entry.c read_termcap.c setbuf.c strings.c \
	trim_sgr0.c use_screen.c write_entry.c
# trace
SRCS+=	lib_trace.c lib_traceatr.c lib_tracebits.c lib_tracechr.c \
	lib_tracedmp.c lib_tracemse.c trace_buf.c trace_tries.c trace_xnames.c \
	varargs.c visbuf.c
# tty
SRCS+=	hardscroll.c hashmap.c lib_mvcur.c lib_tstp.c lib_twait.c \
	lib_vidattr.c tty_update.c
# widechar
SRCS+=  charable.c lib_add_wch.c lib_box_set.c lib_cchar.c lib_erasewchar.c \
	lib_get_wch.c lib_get_wstr.c lib_hline_set.c lib_in_wch.c \
	lib_in_wchnstr.c lib_ins_wch.c lib_inwstr.c lib_key_name.c \
	lib_pecho_wchar.c lib_slk_wset.c lib_unget_wch.c lib_vid_attr.c \
	lib_vline_set.c lib_wacs.c lib_wunctrl.c

HOSTCFLAGS?=	${CFLAGS}
HOSTLDFLAGS?=	${LDFLAGS}
HOSTCFLAGS+=	-I. -I${.CURDIR} ${CURSESTRACE}
CFLAGS+=	-I. -I${.CURDIR} ${CURSESTRACE}

MAN=	curs_addch.3 curs_addchstr.3 curs_addstr.3 curs_attr.3 \
	curs_beep.3 curs_bkgd.3 curs_bkgrnd.3 curs_border.3 curs_border_set.3 \
	curs_clear.3 curs_color.3 \
	curs_delch.3 curs_deleteln.3 \
	curs_extend.3 \
	curs_getch.3 curs_getstr.3 curs_getyx.3 \
	curs_inch.3 curs_inchstr.3 curs_initscr.3 curs_inopts.3 \
	curs_insch.3 curs_insstr.3 curs_instr.3 \
	curs_kernel.3 \
	curs_legacy.3 \
	curs_mouse.3 curs_move.3 \
	curs_opaque.3 curs_outopts.3 curs_overlay.3 \
	curs_pad.3 curs_print.3 curs_printw.3 \
	curs_refresh.3 \
	curs_scanw.3 curs_scr_dump.3 curs_scroll.3 curs_slk.3 \
	curs_termattrs.3 curs_touch.3 \
	curs_util.3 \
	curs_window.3 \
	curs_add_wch.3 curs_add_wchstr.3 curs_addwstr.3 curs_get_wch.3 \
	curs_get_wstr.3 curs_in_wch.3 curs_in_wchstr.3 curs_ins_wch.3 \
	curs_ins_wstr.3 curs_inwstr.3 \
	default_colors.3 define_key.3 \
	key_defined.3 keybound.3 keyok.3 legacy_coding.3 \
	curses.3 \
	resizeterm.3 \
	term.5 term.7 termcap.3 terminfo.3 terminfo.5 \
	wresize.3

GENERATED= init_keytry.h codes.c comp_captab.c expanded.c fallback.c \
	keys.list lib_gen.c lib_keyname.c names.c unctrl.c make_hash make_keys
CLEANFILES+= ${GENERATED}

BUILDFIRST = ${GENERATED}

includes:
	@cmp -s ${DESTDIR}/usr/include/ncurses.h ${.CURDIR}/curses.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/curses.h ${DESTDIR}/usr/include/ncurses.h
	@cd ${.CURDIR}; for i in unctrl.h term.h termcap.h; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

keys.list: ${.CURDIR}/tinfo/MKkeys_list.sh
	sh ${.CURDIR}/tinfo/MKkeys_list.sh ${.CURDIR}/Caps | sort > ${.TARGET}

fallback.c: ${.CURDIR}/tinfo/MKfallback.sh
	sh ${.CURDIR}/tinfo/MKfallback.sh $(FALLBACK_LIST) > ${.TARGET}

lib_gen.c: ${.CURDIR}/base/MKlib_gen.sh
	sh ${.CURDIR}/base/MKlib_gen.sh "${CC} -E -P -I${.CURDIR}" \
		"${AWK}" generated < ${.CURDIR}/curses.h > lib_gen.c

init_keytry.h: make_keys keys.list
	./make_keys keys.list > ${.TARGET}

make_keys: ${.CURDIR}/tinfo/make_keys.c ${.CURDIR}/curses.priv.h names.c
	${HOSTCC} ${LDSTATIC} ${HOSTCFLAGS} ${HOSTLDFLAGS} \
		-o ${.TARGET} ${.CURDIR}/tinfo/make_keys.c ${LDADD}

make_hash: ${.CURDIR}/tinfo/comp_hash.c ${.CURDIR}/curses.priv.h \
		${.CURDIR}/hashsize.h
	${HOSTCC} ${LDSTATIC} ${HOSTCFLAGS} -DMAIN_PROGRAM ${HOSTLDFLAGS} \
		-o ${.TARGET} ${.CURDIR}/tinfo/comp_hash.c ${LDADD}

expanded.c: ${.CURDIR}/term.h ${.CURDIR}/curses.priv.h \
		${.CURDIR}/ncurses_cfg.h ${.CURDIR}/tty/MKexpanded.sh
	sh ${.CURDIR}/tty/MKexpanded.sh "${CC} -E -P" ${CPPFLAGS} > ${.TARGET}

comp_captab.c: make_hash
	sh ${.CURDIR}/tinfo/MKcaptab.sh ${AWK} 1 \
		${.CURDIR}/tinfo/MKcaptab.awk ${.CURDIR}/Caps > ${.TARGET}

lib_keyname.c: keys.list ${.CURDIR}/base/MKkeyname.awk
	${AWK} -f ${.CURDIR}/base/MKkeyname.awk bigstrings=1 \
		keys.list > ${.TARGET}

names.c: ${.CURDIR}/tinfo/MKnames.awk
	${AWK} -f ${.CURDIR}/tinfo/MKnames.awk \
		${.CURDIR}/Caps > names.c
codes.c: ${.CURDIR}/tinfo/MKcodes.awk
	${AWK} -f ${.CURDIR}/tinfo/MKcodes.awk \
		bigstrings=1 ${.CURDIR}/Caps > codes.c

unctrl.c: ${.CURDIR}/base/MKunctrl.awk
	echo | ${AWK} -f ${.CURDIR}/base/MKunctrl.awk bigstrings=1 > ${.TARGET}

.include <bsd.own.mk>

# Link libtermlib, libtermcap to libcurses so we don't break people's Makefiles
afterinstall:
	-cd ${DESTDIR}${LIBDIR}; \
	for i in ${_LIBS}; do \
	    ln -f $$i `echo $$i | sed 's/curses/termlib/'`; \
	    ln -f $$i `echo $$i | sed 's/curses/termcap/'`; \
	    ln -f $$i `echo $$i | sed 's/curses/ncurses/'`; \
	    ln -f $$i `echo $$i | sed 's/curses/ncursesw/'`; \
	done

.include <bsd.lib.mk>
