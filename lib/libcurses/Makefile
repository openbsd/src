#	$OpenBSD: Makefile,v 1.25 1998/08/14 23:02:33 millert Exp $

# Uncomment this to enable tracing in libcurses
#CURSESTRACE=-DTRACE

# This is used to compile terminal info directly into the library
FALLBACK_LIST=

# XXX - should be defined elsewhere
AWK?=	/usr/bin/awk

LIB=    curses
SRCS=   access.c alloc_entry.c captoinfo.c codes.c comp_captab.c comp_error.c \
        comp_expand.c comp_hash.c comp_parse.c comp_scan.c define_key.c \
        expanded.c fallback.c hardscroll.c hashmap.c keyok.c lib_acs.c \
        lib_adabind.c lib_addch.c lib_addstr.c lib_baudrate.c \
        lib_beep.c lib_bkgd.c lib_box.c lib_chgat.c lib_clear.c \
        lib_clearok.c lib_clrbot.c lib_clreol.c lib_color.c \
        lib_colorset.c lib_cur_term.c lib_data.c lib_delch.c \
        lib_delwin.c lib_dft_fgbg.c lib_doupdate.c lib_echo.c \
        lib_endwin.c lib_erase.c lib_flash.c lib_freeall.c lib_gen.c \
        lib_getch.c lib_getstr.c lib_hline.c lib_immedok.c lib_inchstr.c \
        lib_initscr.c lib_insch.c lib_insdel.c lib_insstr.c lib_instr.c \
        lib_isendwin.c lib_kernel.c lib_keyname.c lib_leaveok.c \
        lib_longname.c lib_mouse.c lib_move.c lib_mvcur.c lib_mvwin.c \
        lib_napms.c lib_newterm.c lib_newwin.c lib_nl.c lib_options.c \
        lib_overlay.c lib_pad.c lib_print.c lib_printw.c lib_raw.c \
        lib_redrawln.c lib_refresh.c lib_restart.c lib_scanw.c \
        lib_screen.c lib_scroll.c lib_scrollok.c lib_scrreg.c \
        lib_set_term.c lib_setup.c lib_slk.c lib_slkatr_set.c \
        lib_slkatrof.c lib_slkatron.c lib_slkatrset.c lib_slkattr.c \
        lib_slkclear.c lib_slkcolor.c lib_slkinit.c lib_slklab.c \
        lib_slkrefr.c lib_slkset.c lib_slktouch.c lib_termcap.c \
        lib_ti.c lib_touch.c lib_tparm.c lib_tputs.c lib_trace.c \
        lib_traceatr.c lib_tracechr.c lib_tracedmp.c lib_tracemse.c \
        lib_tstp.c lib_twait.c lib_ungetch.c lib_vidattr.c lib_vline.c \
        lib_wattroff.c lib_wattron.c lib_window.c name_match.c names.c \
	nc_panel.c parse_entry.c read_bsd_terminfo.c read_entry.c \
	read_termcap.c resizeterm.c safe_sprintf.c sigaction.c softscroll.c \
	trace_buf.c tries.c unctrl.c wresize.c write_entry.c
CFLAGS+= -I. -I${.CURDIR} ${CURSESTRACE}
# XXX - missing stuff here
MAN=	curs_addch.3 curs_addchstr.3 curs_addstr.3 curs_attr.3 curs_beep.3 \
	curs_bkgd.3 curs_border.3 curs_clear.3 curs_color.3 curs_delch.3 \
	curs_deleteln.3 curs_getch.3 curs_getstr.3 curs_getyx.3 curs_inch.3 \
	curs_inchstr.3 curs_initscr.3 curs_inopts.3 curs_insch.3 curs_insstr.3 \
	curs_instr.3 curs_kernel.3 curs_mouse.3 curs_move.3 curs_outopts.3 \
	curs_overlay.3 curs_pad.3 curs_print.3 curs_printw.3 curs_refresh.3 \
	curs_scanw.3 curs_scr_dmp.3 curs_scroll.3 curs_slk.3 curs_termattrs.3 \
	curs_touch.3 curs_util.3 curs_window.3 curses.3 define_key.3 \
	dft_fgbg.3 keyok.3 wresize.3 termcap.3 terminfo.3 terminfo.5
MLINKS+=curses.3 ncurses.3
MLINKS+=curs_addch.3 addch.3 curs_addch.3 waddch.3 curs_addch.3 mvaddch.3 \
	curs_addch.3 mvwaddch.3 curs_addch.3 echochar.3 curs_addch.3 \
	wechochar.3 curs_addchstr.3 addchstr.3 curs_addchstr.3 addchnstr.3 \
	curs_addchstr.3 waddchstr.3 curs_addchstr.3 waddchnstr.3 \
	curs_addchstr.3 mvaddchstr.3 curs_addchstr.3 mvaddchnstr.3 \
	curs_addchstr.3 mvwaddchstr.3 curs_addchstr.3 mvwaddchnstr.3 \
	curs_addstr.3 addstr.3 curs_addstr.3 addnstr.3 curs_addstr.3 waddstr.3 \
	curs_addstr.3 waddnstr.3 curs_addstr.3 mvaddstr.3 curs_addstr.3 \
	mvaddnstr.3 curs_addstr.3 mvwaddstr.3 curs_addstr.3 mvwaddnstr.3 \
	curs_attr.3 attroff.3 curs_attr.3 wattroff.3 curs_attr.3 attron.3 \
	curs_attr.3 wattron.3 curs_attr.3 attrset.3 curs_attr.3 wattrset.3 \
	curs_attr.3 standend.3 curs_attr.3 wstandend.3 curs_attr.3 standout.3 \
	curs_attr.3 wstandout.3 curs_beep.3 beep.3 curs_beep.3 flash.3 \
	curs_bkgd.3 bkgdset.3 curs_bkgd.3 wbkgdset.3 curs_bkgd.3 bkgd.3 \
	curs_bkgd.3 wbkgd.3 curs_border.3 border.3 curs_border.3 wborder.3 \
	curs_border.3 box.3 curs_border.3 hline.3 curs_border.3 whline.3 \
	curs_border.3 vline.3 curs_border.3 wvline.3 curs_clear.3 erase.3 \
	curs_clear.3 werase.3 curs_clear.3 clear.3 curs_clear.3 wclear.3 \
	curs_clear.3 clrtobot.3 curs_clear.3 wclrtobot.3 curs_clear.3 \
	clrtoeol.3 curs_clear.3 wclrtoeol.3 curs_color.3 start_color.3 \
	curs_color.3 init_pair.3 curs_color.3 init_color.3 curs_color.3 \
	has_colors.3 curs_color.3 can_change_color.3 curs_color.3 \
	color_content.3 curs_color.3 pair_content.3 curs_delch.3 delch.3 \
	curs_delch.3 wdelch.3 curs_delch.3 mvdelch.3 curs_delch.3 mvwdelch.3 \
	curs_deleteln.3 deleteln.3 curs_deleteln.3 wdeleteln.3 curs_deleteln.3 \
	insdelln.3 curs_deleteln.3 winsdelln.3 curs_deleteln.3 insertln.3 \
	curs_deleteln.3 winsertln.3 curs_getch.3 getch.3 curs_getch.3 wgetch.3 \
	curs_getch.3 mvgetch.3 curs_getch.3 mvwgetch.3 curs_getch.3 ungetch.3 \
	curs_getstr.3 getstr.3 curs_getstr.3 wgetstr.3 curs_getstr.3 \
	mvgetstr.3 curs_getstr.3 mvwgetstr.3 curs_getstr.3 wgetnstr.3 \
	curs_getyx.3 getyx.3 curs_getyx.3 getparyx.3 curs_getyx.3 getbegyx.3 \
	curs_getyx.3 getmaxyx.3 curs_inch.3 inch.3 curs_inch.3 winch.3 \
	curs_inch.3 mvinch.3 curs_inch.3 mvwinch.3 curs_inchstr.3 inchstr.3 \
	curs_inchstr.3 inchnstr.3 curs_inchstr.3 winchstr.3 curs_inchstr.3 \
	winchnstr.3 curs_inchstr.3 mvinchstr.3 curs_inchstr.3 mvinchnstr.3 \
	curs_inchstr.3 mvwinchstr.3 curs_inchstr.3 mvwinchnstr.3 \
	curs_initscr.3 initscr.3 curs_initscr.3 newterm.3 curs_initscr.3 \
	endwin.3 curs_initscr.3 isendwin.3 curs_initscr.3 set_term.3 \
	curs_initscr.3 delscreen.3 curs_inopts.3 cbreak.3 curs_inopts.3 \
	nocbreak.3 curs_inopts.3 echo.3 curs_inopts.3 noecho.3 curs_inopts.3 \
	halfdelay.3 curs_inopts.3 intrflush.3 curs_inopts.3 keypad.3 \
	curs_inopts.3 meta.3 curs_inopts.3 nodelay.3 curs_inopts.3 notimeout.3 \
	curs_inopts.3 raw.3 curs_inopts.3 noraw.3 curs_inopts.3 noqiflush.3 \
	curs_inopts.3 qiflush.3 curs_inopts.3 timeout.3 curs_inopts.3 \
	wtimeout.3 curs_inopts.3 typeahead.3 curs_insch.3 insch.3 curs_insch.3 \
	winsch.3 curs_insch.3 mvinsch.3 curs_insch.3 mvwinsch.3 curs_insstr.3 \
	insstr.3 curs_insstr.3 insnstr.3 curs_insstr.3 winsstr.3 curs_insstr.3 \
	winsnstr.3 curs_insstr.3 mvinsstr.3 curs_insstr.3 mvinsnstr.3 \
	curs_insstr.3 mvwinsstr.3 curs_insstr.3 mvwinsnstr.3 curs_instr.3 \
	instr.3 curs_instr.3 innstr.3 curs_instr.3 winstr.3 curs_instr.3 \
	winnstr.3 curs_instr.3 mvinstr.3 curs_instr.3 mvinnstr.3 curs_instr.3 \
	mvwinstr.3 curs_instr.3 mvwinnstr.3 curs_kernel.3 def_prog_mode.3 \
	curs_kernel.3 def_shell_mode.3 curs_kernel.3 reset_prog_mode.3 \
	curs_kernel.3 reset_shell_mode.3 curs_kernel.3 resetty.3 curs_kernel.3 \
	savetty.3 curs_kernel.3 getsyx.3 curs_kernel.3 setsyx.3 curs_kernel.3 \
	ripoffline.3 curs_kernel.3 curs_set.3 curs_kernel.3 napms.3 \
	curs_mouse.3 getmouse.3 curs_mouse.3 ungetmouse.3 curs_mouse.3 \
	mousemask.3 curs_move.3 move.3 curs_move.3 wmove.3 curs_outopts.3 \
	clearok.3 curs_outopts.3 idlok.3 curs_outopts.3 idcok.3 curs_outopts.3 \
	immedok.3 curs_outopts.3 leaveok.3 curs_outopts.3 setscrreg.3 \
	curs_outopts.3 wsetscrreg.3 curs_outopts.3 scrollok.3 curs_outopts.3 \
	nl.3 curs_outopts.3 nonl.3 curs_overlay.3 overlay.3 curs_overlay.3 \
	overwrite.3 curs_overlay.3 copywin.3 curs_pad.3 newpad.3 curs_pad.3 \
	subpad.3 curs_pad.3 prefresh.3 curs_pad.3 pnoutrefresh.3 curs_pad.3 \
	pechochar.3 curs_print.3 mcprint.3 curs_printw.3 printw.3 \
	curs_printw.3 wprintw.3 curs_printw.3 mvprintw.3 curs_printw.3 \
	mvwprintw.3 curs_printw.3 vwprintw.3 curs_refresh.3 refresh.3 \
	curs_refresh.3 wrefresh.3 curs_refresh.3 wnoutrefresh.3 curs_refresh.3 \
	doupdate.3 curs_refresh.3 redrawwin.3 curs_refresh.3 wredrawln.3 \
	curs_scanw.3 scanw.3 curs_scanw.3 wscanw.3 curs_scanw.3 mvscanw.3 \
	curs_scanw.3 mvwscanw.3 curs_scanw.3 vwscanw.3 curs_scr_dmp.3 \
	scr_dump.3 curs_scr_dmp.3 scr_restore.3 curs_scr_dmp.3 scr_init.3 \
	curs_scr_dmp.3 scr_set.3 curs_scroll.3 scroll.3 curs_scroll.3 srcl.3 \
	curs_scroll.3 wscrl.3 curs_slk.3 slk_init.3 curs_slk.3 slk_set.3 \
	curs_slk.3 slk_refresh.3 curs_slk.3 slk_noutrefresh.3 curs_slk.3 \
	slk_label.3 curs_slk.3 slk_clear.3 curs_slk.3 slk_restore.3 curs_slk.3 \
	slk_touch.3 curs_slk.3 slk_attron.3 curs_slk.3 slk_attrset.3 \
	curs_slk.3 slk_attroff.3 curs_termattrs.3 baudrate.3 curs_termattrs.3 \
	erasechar.3 curs_termattrs.3 has_ic.3 curs_termattrs.3 has_il.3 \
	curs_termattrs.3 killchar.3 curs_termattrs.3 longname.3 \
	curs_termattrs.3 termattrs.3 curs_termattrs.3 termname.3 curs_touch.3 \
	touchwin.3 curs_touch.3 touchline.3 curs_touch.3 untouchwin.3 \
	curs_touch.3 wtouchln.3 curs_touch.3 is_linetouched.3 curs_touch.3 \
	is_wintouched.3 curs_util.3 unctrl.3 curs_util.3 keyname.3 curs_util.3 \
	filter.3 curs_util.3 use_env.3 curs_util.3 putwin.3 curs_util.3 \
	getwin.3 curs_util.3 delay_output.3 curs_util.3 flushinp.3 \
	curs_window.3 newwin.3 curs_window.3 delwin.3 curs_window.3 mvwin.3 \
	curs_window.3 subwin.3 curs_window.3 derwin.3 curs_window.3 mvderwin.3 \
	curs_window.3 dupwin.3 curs_window.3 wsyncup.3 curs_window.3 syncok.3 \
	curs_window.3 wcursyncup.3 curs_window.3 wsyncdown.3
MLINKS+= terminfo.3 setupterm.3 terminfo.3 setterm.3 terminfo.3 set_curterm.3 \
	terminfo.3 del_curterm.3 terminfo.3 restartterm.3 terminfo.3 tparm.3 \
	terminfo.3 tparam.3 terminfo.3 tputs.3 terminfo.3 putp.3 \
	terminfo.3 vidputs.3 terminfo.3 vidattr.3 terminfo.3 mvcur.3 \
	terminfo.3 tigetflag.3 terminfo.3 tigetnum.3 terminfo.3 tigetstr.3
MLINKS+= termcap.3 tgetent.3 termcap.3 tgetflag.3 termcap.3 tgetnum.3 \
	termcap.3 tgetstr.3 termcap.3 tgoto.3 termcap.3 tputs.3

CLEANFILES+= nomacros.h codes.c comp_captab.c expanded.c fallback.c lib_gen.c \
	lib_keyname.c names.c unctrl.c keys.tries make_hash make_keys

beforedepend: nomacros.h codes.c comp_captab.c expanded.c fallback.c lib_gen.c \
	lib_keyname.c names.c unctrl.c keys.tries make_hash make_keys

includes:
	@cmp -s ${DESTDIR}/usr/include/ncurses.h ${.CURDIR}/curses.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/curses.h ${DESTDIR}/usr/include/ncurses.h
	@cd ${.CURDIR}; for i in unctrl.h term.h; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

fallback.c: ${.CURDIR}/term.h ${.CURDIR}/curses.priv.h ${.CURDIR}/MKfallback.sh
	sh ${.CURDIR}/MKfallback.sh $(FALLBACK_LIST) > ${.TARGET}

lib_gen.c: ${.CURDIR}/curses.h ${.CURDIR}/MKlib_gen.sh
	sh ${.CURDIR}/MKlib_gen.sh "${CPP} -I${.CURDIR}" "${AWK}" < ${.CURDIR}/curses.h > lib_gen.c

nomacros.h: ${.CURDIR}/MKlib_gen.sh lib_gen.c
	fgrep undef lib_gen.c > ${.TARGET}

keys.tries: make_keys ${.CURDIR}/keys.list
	./make_keys ${.CURDIR}/keys.list > ${.TARGET}

make_keys: ${.CURDIR}/make_keys.c names.c
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.CURDIR}/make_keys.c ${LDADD}

make_hash: ${.CURDIR}/comp_hash.c ${.CURDIR}/hashsize.h
	${CC} ${CFLAGS} -DMAIN_PROGRAM ${LDFLAGS} -o ${.TARGET} ${.CURDIR}/comp_hash.c ${LDADD}

expanded.c: ${.CURDIR}/term.h ${.CURDIR}/curses.priv.h ${.CURDIR}/MKexpanded.sh
	sh ${.CURDIR}/MKexpanded.sh "${CPP}" ${CPPFLAGS}

comp_captab.c: make_hash ${.CURDIR}/ncurses_cfg.h ${.CURDIR}/tic.h ${.CURDIR}/term.h
	sh ${.CURDIR}/MKcaptab.awk ${AWK} ${.CURDIR}/Caps > ${.TARGET}

lib_keyname.c: ${.CURDIR}/ncurses_cfg.h ${.CURDIR}/curses.h ${.CURDIR}/term.h  ${.CURDIR}/keys.list ${.CURDIR}/MKkeyname.awk
	${AWK} -f ${.CURDIR}/MKkeyname.awk ${.CURDIR}/keys.list > ${.TARGET}

names.c codes.c: ${.CURDIR}/MKnames.awk
	${AWK} -f ${.CURDIR}/MKnames.awk ${.CURDIR}/Caps
	cat namehdr boolnames boolfnames numnames numfnames strnames strfnames nameftr > names.c
	cat namehdr boolcodes numcodes strcodes codeftr > codes.c
	-rm -f namehdr nameftr codeftr boolnames boolfnames boolcodes numnames numfnames numcodes strnames strfnames strcodes

unctrl.c: ${.CURDIR}/MKunctrl.awk
	echo | ${AWK} -f ${.CURDIR}/MKunctrl.awk > ${.TARGET}

# Link libtermlib to libcurses so we don't break people's Makefiles
afterinstall:
	-cd ${DESTDIR}/usr/lib; \
	for i in ${_LIBS}; do \
	    ln -f $$i `echo $$i | sed 's/curses/termlib/'`; \
	done

.include <bsd.lib.mk>

lib_options.o lib_options.so lib_options.po lib_options.ln: keys.tries
