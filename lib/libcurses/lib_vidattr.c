
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
 *	vidputs(newmode, outc)
 *
 *	newmode is taken to be the logical 'or' of the symbols in curses.h
 *	representing graphic renditions.  The terminal is set to be in all of
 *	the given modes, if possible.
 *
 *	if the new attribute is normal
 *		if exit-alt-char-set exists
 *			emit it
 *		emit exit-attribute-mode
 *	else if set-attributes exists
 *		use it to set exactly what you want
 *	else
 *		if exit-attribute-mode exists
 *			turn off everything
 *		else
 *			turn off those which can be turned off and aren't in
 *			newmode.
 *		turn on each mode which should be on and isn't, one by one
 *
 *	NOTE that this algorithm won't achieve the desired mix of attributes
 *	in some cases, but those are probably just those cases in which it is
 *	actually impossible, anyway, so...
 *
 * 	NOTE that we cannot assume that there's no interaction between color
 *	and other attribute resets.  So each time we reset color (or other
 *	attributes) we'll have to be prepared to restore the other.
 */

#include <curses.priv.h>
#include <term.h>

MODULE_ID("Id: lib_vidattr.c,v 1.14 1997/05/06 16:02:43 tom Exp $")

#define doPut(mode) TPUTS_TRACE(#mode); tputs(mode, 1, outc)

#define TurnOn(mask,mode) \
	if ((turn_on & mask) && mode) { doPut(mode); }

#define TurnOff(mask,mode) \
	if ((turn_off & mask) && mode) { doPut(mode); turn_off &= ~mask; }

int vidputs(attr_t newmode, int  (*outc)(int))
{
static attr_t previous_attr = A_NORMAL;
attr_t turn_on, turn_off;
int pair, current_pair;

	T((T_CALLED("vidputs(%s)"), _traceattr(newmode)));

	/* this allows us to go on whether or not newterm() has been called */
	if (SP)
		previous_attr = SP->_current_attr;

	T(("previous attribute was %s", _traceattr(previous_attr)));

	if (newmode == previous_attr)
		returnCode(OK);

	turn_off = (~newmode & previous_attr) & ALL_BUT_COLOR;
	turn_on  = (newmode & ~previous_attr) & ALL_BUT_COLOR;

	pair = PAIR_NUMBER(newmode);
	current_pair = PAIR_NUMBER(previous_attr);

	/* if there is no current screen, assume we *can* do color */
	if ((!SP || SP->_coloron) && pair == 0) {
		T(("old pair = %d -- new pair = %d", current_pair, pair));
		if (pair != current_pair) {
			_nc_do_color(pair, outc);
			previous_attr &= ~A_COLOR;
		}
	}

	if (newmode == A_NORMAL) {
		if((previous_attr & A_ALTCHARSET) && exit_alt_charset_mode) {
			doPut(exit_alt_charset_mode);
			previous_attr &= ~A_ALTCHARSET;
		}
		if (previous_attr) {
			doPut(exit_attribute_mode);
			previous_attr &= ~A_COLOR;
		}

	} else if (set_attributes) {
		if (turn_on || turn_off) {
			TPUTS_TRACE("set_attributes");
			tputs(tparm(set_attributes,
				(newmode & A_STANDOUT) != 0,
				(newmode & A_UNDERLINE) != 0,
				(newmode & A_REVERSE) != 0,
				(newmode & A_BLINK) != 0,
				(newmode & A_DIM) != 0,
				(newmode & A_BOLD) != 0,
				(newmode & A_INVIS) != 0,
				(newmode & A_PROTECT) != 0,
				(newmode & A_ALTCHARSET) != 0), 1, outc);
			previous_attr &= ~A_COLOR;
		}
	} else {

		T(("turning %s off", _traceattr(turn_off)));

		TurnOff(A_ALTCHARSET,  exit_alt_charset_mode);
		TurnOff(A_UNDERLINE,   exit_underline_mode);
		TurnOff(A_STANDOUT,    exit_standout_mode);

		if (turn_off && exit_attribute_mode) {
			doPut(exit_attribute_mode);
			turn_on  |= (newmode & (chtype)(~A_COLOR));
			previous_attr &= ~A_COLOR;
		}

		T(("turning %s on", _traceattr(turn_on)));

		TurnOn (A_ALTCHARSET, enter_alt_charset_mode);
		TurnOn (A_BLINK,      enter_blink_mode);
		TurnOn (A_BOLD,       enter_bold_mode);
		TurnOn (A_DIM,        enter_dim_mode);
		TurnOn (A_REVERSE,    enter_reverse_mode);
		TurnOn (A_STANDOUT,   enter_standout_mode);
		TurnOn (A_PROTECT,    enter_protected_mode);
		TurnOn (A_INVIS,      enter_secure_mode);
		TurnOn (A_UNDERLINE,  enter_underline_mode);
		TurnOn (A_HORIZONTAL, enter_horizontal_hl_mode);
		TurnOn (A_LEFT,       enter_left_hl_mode);
		TurnOn (A_LOW,        enter_low_hl_mode);
		TurnOn (A_RIGHT,      enter_right_hl_mode);
		TurnOn (A_TOP,        enter_top_hl_mode);
		TurnOn (A_VERTICAL,   enter_vertical_hl_mode);
	}

	/* if there is no current screen, assume we *can* do color */
	if ((!SP || SP->_coloron) && pair != 0) {
		current_pair = PAIR_NUMBER(previous_attr);
		T(("old pair = %d -- new pair = %d", current_pair, pair));
		if (pair != current_pair) {
			_nc_do_color(pair, outc);
		}
	}

	if (SP)
		SP->_current_attr = newmode;
	else
		previous_attr = newmode;

	returnCode(OK);
}

#ifdef EXTERN_TERMINFO
#undef vidattr
#endif

int vidattr(attr_t newmode)
{
	T((T_CALLED("vidattr(%s)"), _traceattr(newmode)));

	returnCode(vidputs(newmode, _nc_outch));
}

attr_t termattrs(void)
{
	int attrs = A_NORMAL;

	if (enter_alt_charset_mode)
		attrs |= A_ALTCHARSET;

	if (enter_blink_mode)
		attrs |= A_BLINK;

	if (enter_bold_mode)
		attrs |= A_BOLD;

	if (enter_dim_mode)
		attrs |= A_DIM;

	if (enter_reverse_mode)
		attrs |= A_REVERSE;

	if (enter_standout_mode)
		attrs |= A_STANDOUT;

	if (enter_protected_mode)
		attrs |= A_PROTECT;

	if (enter_secure_mode)
		attrs |= A_INVIS;

	if (enter_underline_mode)
		attrs |= A_UNDERLINE;

	if (SP->_coloron)
		attrs |= A_COLOR;

	return(attrs);
}

