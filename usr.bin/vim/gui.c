/*	$OpenBSD: gui.c,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved			by Bram Moolenaar
 *								GUI/Motif support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

/* Structure containing all the GUI information */
Gui gui;

/* Set to TRUE after adding/removing menus to ensure they are updated */
int force_menu_update = FALSE;


static void gui_check_screen __ARGS((void));
static void gui_outstr __ARGS((char_u *, int));
static void gui_update_selection __ARGS((void));
static int gui_get_menu_cmd_modes __ARGS((char_u *, int, int *, int *));
static int gui_add_menu_path __ARGS((char_u *, int, void (*)(), char_u *, int));
static int gui_remove_menu __ARGS((GuiMenu **, char_u *, int));
static void gui_free_menu __ARGS((GuiMenu *));
static void gui_free_menu_string __ARGS((GuiMenu *, int));
static int gui_show_menus __ARGS((char_u *, int));
static void gui_show_menus_recursive __ARGS((GuiMenu *, int, int));
static char_u *gui_menu_name_skip __ARGS((char_u *name));
static void gui_create_initial_menus __ARGS((GuiMenu *, GuiMenu *));
static void gui_update_scrollbars __ARGS((void));
static void gui_update_horiz_scrollbar __ARGS((void));

/*
 * The Athena scrollbars can move the thumb to after the end of the scrollbar,
 * this makes the thumb indicate the part of the text that is shown.  Motif
 * can't do this.
 */
#ifdef USE_GUI_ATHENA
# define SCROLL_PAST_END
#endif

/*
 * gui_start -- Called when user wants to start the GUI.
 */
	void
gui_start()
{
	char_u	*old_term;

	old_term = strsave(term_strings[KS_NAME]);
	mch_setmouse(FALSE);					/* first switch mouse off */

	/* set_termname() will call gui_init() to start the GUI */
	termcapinit((char_u *)"builtin_gui");

	if (!gui.in_use)						/* failed to start GUI */
		termcapinit(old_term);

	vim_free(old_term);

	/*
	 * Quit the current process and continue in the child.
	 * Makes "gvim file" disconnect from the shell it was started in.
	 * Don't do this when Vim was started with "-f" or the 'f' flag is present
	 * in 'guioptions'.
	 */
	if (gui.in_use && gui.dofork &&
					  vim_strchr(p_guioptions, GO_FORG) == NULL && fork() > 0)
		exit(0);
}

/*
 * Call this when vim starts up, whether or not the GUI is started
 */
	void
gui_prepare(argc, argv)
	int		*argc;
	char	**argv;
{
	/* Menu items may be added before the GUI is started, so set this now */
	gui.root_menu = NULL;
	gui.in_use = FALSE;				/* No GUI yet (maybe later) */
	gui.starting = FALSE;			/* No GUI yet (maybe later) */
	gui.dofork = TRUE;				/* default is to use fork() */
	gui_mch_prepare(argc, argv);
}

static struct default_menu
{
	char	*name;			/* name of menu item */
	int		mode;			/* mode where menu is valid */
	char	*command;		/* resulting command */
} default_menus[] = 
{
	/* Help menu.  Some reason Motif is happier if this is added first. */
	{"Help.Overview  <F1>",	MENU_NORMAL_MODE,	":help\r"},
	{"Help.Overview  <F1>",	MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:help\r"},
	{"Help.How to\\.\\.\\.",MENU_NORMAL_MODE,	":help how_to\r"},
	{"Help.How to\\.\\.\\.",MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:help how_to\r"},
	{"Help.GUI",			MENU_NORMAL_MODE,	":help gui\r"},
	{"Help.GUI",			MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:help gui\r"},
	{"Help.Version",		MENU_NORMAL_MODE,	":version\r"},
	{"Help.Version",		MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:version\r"},
	{"Help.Credits",		MENU_NORMAL_MODE,	":help credits\r"},
	{"Help.Credits",		MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:help credits\r"},
	{"Help.Copying",		MENU_NORMAL_MODE,	":help uganda\r"},
	{"Help.Copying",		MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:help uganda\r"},

	/* File menu */
	{"File.Save       :w",	MENU_NORMAL_MODE,	":w\r"},
	{"File.Save       :w",	MENU_INSERT_MODE,	"\017:w\r"},

	{"File.Close      :q",	MENU_NORMAL_MODE,	":q\r"},
	{"File.Close      :q",	MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:q\r"},

	{"File.Quit       :qa",	MENU_NORMAL_MODE,	":qa\r"},
	{"File.Quit       :qa",	MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:qa\r"},

	{"File.Save-Quit  :wqa",MENU_NORMAL_MODE,	":wqa\r"},
	{"File.Save-Quit  :wqa",MENU_VISUAL_MODE|MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\033:wqa\r"},

	/* Edit menu */
	{"Edit.Undo",			MENU_NORMAL_MODE,	"u"},
	{"Edit.Redo",			MENU_NORMAL_MODE,	"\022"},

	{"Edit.Cut",			MENU_VISUAL_MODE,	"x"},
	{"Edit.Copy",			MENU_VISUAL_MODE,	"y"},
	{"Edit.Put Before",		MENU_NORMAL_MODE,	"[p"},
	{"Edit.Put Before",		MENU_INSERT_MODE,	"\017[p"},
	{"Edit.Put After",		MENU_NORMAL_MODE,	"]p"},
	{"Edit.Put After",		MENU_INSERT_MODE,	"\017]p"},
	{"Edit.Paste",			MENU_NORMAL_MODE,	"i\022*\033"},	/* CTRL-R * */
	{"Edit.Paste",			MENU_INSERT_MODE|MENU_CMDLINE_MODE,
												"\022*"},	/* CTRL-R * */
	{NULL,					0,					NULL}
};

/*
 * This is the call which starts the GUI.
 */
	void
gui_init()
{
	char_u	*env_str;
	int		i;

	gui.dying = FALSE;
	gui.in_focus = FALSE;
	gui.dragged_sb = SB_NONE;
	gui.dragged_wp = NULL;
	gui.col = gui.num_cols = 0;
	gui.row = gui.num_rows = 0;

	/* Initialise gui.cursor_row: */
	INVALIDATE_CURSOR();
	gui.scroll_region_top = 0;
	gui.scroll_region_bot = Rows - 1;
	gui.highlight_mask = HL_NORMAL;
	gui.char_width = 0;
	gui.char_height = 0;
	gui.char_ascent = 0;
	gui.border_width = 0;

	gui.selection.owned = FALSE;
	gui.selection.start.lnum = 0;
	gui.selection.start.col = 0;
	gui.selection.end.lnum = 0;
	gui.selection.end.col = 0;
	gui.selection.state = SELECT_CLEARED;

	gui.root_menu = NULL;
	gui.menu_is_active = TRUE;		/* default: include menu */

	gui.scrollbar_width = SB_DEFAULT_WIDTH;
	gui.menu_height = MENU_DEFAULT_HEIGHT;
	for (i = 0; i < 3; i++)
		gui.new_sb[i] = FALSE;

	gui.prev_wrap = -1;

	for (i = 0; default_menus[i].name != NULL; ++i)
		gui_add_menu_path((char_u *)default_menus[i].name,
									default_menus[i].mode, gui_menu_cb,
						  (char_u *)default_menus[i].command, TRUE);

	/*
	 * Switch on the mouse by default.
	 * This can be changed in the .gvimrc.
	 */
	set_string_option((char_u *)"mouse", -1, (char_u *)"a", TRUE);

	/*
	 * Get system wide defaults for gvim (Unix only)
	 */
#ifdef HAVE_CONFIG_H
	do_source(sys_gvimrc_fname, FALSE);
#endif

	/*
	 * Try to read GUI initialization commands from the following places:
	 * - environment variable GVIMINIT
	 * - the user gvimrc file (~/.gvimrc for Unix)
	 * The first that exists is used, the rest is ignored.
	 */
	if ((env_str = vim_getenv((char_u *)"GVIMINIT")) != NULL && *env_str != NUL)
	{
		sourcing_name = (char_u *)"GVIMINIT";
		do_cmdline(env_str, TRUE, TRUE);
		sourcing_name = NULL;
	}
	else
		do_source((char_u *)USR_GVIMRC_FILE, FALSE);

	/*
	 * Read initialization commands from ".gvimrc" in current directory.  This
	 * is only done if the 'exrc' option is set.  Because of security reasons
	 * we disallow shell and write commands now, except for unix if the file is
	 * owned by the user or 'secure' option has been reset in environment of
	 * global ".gvimrc".  Only do this if GVIMRC_FILE is not the same as
	 * USR_GVIMRC_FILE or sys_gvimrc_fname.
	 */
	if (p_exrc)
	{
#ifdef UNIX
		{
			struct stat s;

			/* if ".gvimrc" file is not owned by user, set 'secure' mode */
			if (stat(GVIMRC_FILE, &s) || s.st_uid != getuid())
				secure = p_secure;
		}
#else
		secure = p_secure;
#endif

		i = FAIL;
		if (fullpathcmp((char_u *)USR_GVIMRC_FILE,
											(char_u *)GVIMRC_FILE) != FPC_SAME
#ifdef HAVE_CONFIG_H
				&& fullpathcmp(sys_gvimrc_fname,
											(char_u *)GVIMRC_FILE) != FPC_SAME
#endif
				)
			i = do_source((char_u *)GVIMRC_FILE, FALSE);
	}

	/*
	 * Actually start the GUI itself.
	 */
	gui.in_use = TRUE;		/* Must be set after menus have been set up */
	if (gui_mch_init() == FAIL)
	{
		gui.in_use = FALSE;
		return;
	}

	maketitle();

	gui_create_initial_menus(gui.root_menu, NULL);
}

	void
gui_exit()
{
	gui.in_use = FALSE;
	gui_mch_exit();
}

/*
 * Set the font. Uses the 'font' option. The first font name that works is
 * used. If none is found, use the default font.
 */
	int
gui_init_font()
{
#define FONTLEN 100
	char_u	*font_list;
	char_u	font_name[FONTLEN];

	if (!gui.in_use)
		return FAIL;

	for (font_list = p_guifont; *font_list != NUL; )
	{
		/* Isolate one font name */
		(void)copy_option_part(&font_list, font_name, FONTLEN, ",");
		if (gui_mch_init_font(font_name) == OK)
			return OK;
	}

	/*
	 * Couldn't load any font in 'font', tell gui_mch_init_font() to try and
	 * find a font we can load.
	 */
  	return gui_mch_init_font(NULL);
}

	void
gui_set_cursor(row, col)
	int		row;
	int		col;
{
	gui.row = row;
	gui.col = col;
}

/*
 * gui_check_screen - check if the cursor is on the screen.
 */
	static void
gui_check_screen()
{
	if (gui.row >= Rows)
		gui.row = Rows - 1;
	if (gui.col >= Columns)
		gui.col = Columns - 1;
	if (gui.cursor_row >= Rows || gui.cursor_col >= Columns)
		INVALIDATE_CURSOR();
}

	void
gui_update_cursor()
{
	gui_check_screen();
	if (gui.row != gui.cursor_row || gui.col != gui.cursor_col)
	{
		gui_undraw_cursor();
		gui.cursor_row = gui.row;
		gui.cursor_col = gui.col;
		gui_mch_draw_cursor();
	}
}

/*
 * Should be called after the GUI window has been resized.  Its arguments are
 * the new width and height of the window in pixels.
 */
	void
gui_resize_window(pixel_width, pixel_height)
	int		pixel_width;
	int		pixel_height;
{
	gui.num_cols = (pixel_width - 2 * gui.border_offset) / gui.char_width;
	gui.num_rows = (pixel_height - 2 * gui.border_offset) / gui.char_height;

	gui_reset_scroll_region();
	/*
	 * At the "more" prompt there is no redraw, put the cursor at the last
	 * line here (why does it have to be one row too low???).
	 */
	if (State == ASKMORE)
		gui.row = gui.num_rows;

	if (gui.num_rows != screen_Rows || gui.num_cols != screen_Columns)
		set_winsize(0, 0, FALSE);
	gui_update_cursor();
}

/*
 * Make scroll region cover whole screen.
 */
	void
gui_reset_scroll_region()
{
	gui.scroll_region_top = 0;
	gui.scroll_region_bot = gui.num_rows - 1;
}

	void
gui_start_highlight(mask)
	long_u	mask;
{
	gui.highlight_mask |= mask;
}

	void
gui_stop_highlight(mask)
    long_u	mask;
{
	gui.highlight_mask &= ~mask;
}

	void
gui_write(s, len)
	char_u	*s;
	int		len;
{
	char_u	*p;
	int		arg1 = 0, arg2 = 0;

/* #define DEBUG_GUI_WRITE */
#ifdef DEBUG_GUI_WRITE
	{
		int i;
		char_u *str;

		printf("gui_write(%d):\n    ", len);
		for (i = 0; i < len; i++)
			if (s[i] == ESC)
			{
				if (i != 0)
					printf("\n    ");
				printf("<ESC>");
			}
			else
			{
				str = transchar(s[i]);
				if (str[0] && str[1])
					printf("<%s>", (char *)str);
				else
					printf("%s", (char *)str);
			}
		printf("\n");
	}
#endif
	while (len)
	{
		if (s[0] == '\n')
		{
			len--;
			s++;
			gui.col = 0;
			if (gui.row < gui.scroll_region_bot)
				gui.row++;
			else
				gui_mch_delete_lines(gui.scroll_region_top, 1);
		}
		else if (s[0] == '\r')
		{
			len--;
			s++;
			gui.col = 0;
		}
		else if (s[0] == Ctrl('G'))		/* Beep */
		{
			gui_mch_beep();
			len--;
			s++;
		}
		else if (s[0] == ESC && s[1] == '|')
		{
			p = s + 2;
			if (isdigit(*p))
			{
				arg1 = getdigits(&p);
				if (p > s + len)
					break;
				if (*p == ';')
				{
					++p;
					arg2 = getdigits(&p);
					if (p > s + len)
						break;
				}
			}
			switch (*p)
			{
				case 'C':		/* Clear screen */
					gui_mch_clear_block(0, 0, Rows - 1, Columns - 1);
					break;
				case 'M':		/* Move cursor */
					gui_set_cursor(arg1, arg2);
					break;
				case 'R':		/* Set scroll region */
					if (arg1 < arg2)
					{
						gui.scroll_region_top = arg1;
						gui.scroll_region_bot = arg2;
					}
					else
					{
						gui.scroll_region_top = arg2;
						gui.scroll_region_bot = arg1;
					}
					break;
				case 'd':		/* Delete line */
					gui_mch_delete_lines(gui.row, 1);
					break;
				case 'D':		/* Delete lines */
					gui_mch_delete_lines(gui.row, arg1);
					break;
				case 'i':		/* Insert line */
					gui_mch_insert_lines(gui.row, 1);
					break;
				case 'I':		/* Insert lines */
					gui_mch_insert_lines(gui.row, arg1);
					break;
				case '$':		/* Clear to end-of-line */
					gui_mch_clear_block(gui.row, gui.col, gui.row, Columns - 1);
					break;
				case 'h':		/* Turn on highlighting */
					gui_start_highlight(arg1);
					break;
				case 'H':		/* Turn off highlighting */
					gui_stop_highlight(arg1);
					break;
				case 'f':		/* flash the window (visual bell) */
					gui_mch_flash();
					break;
				default:
					p = s + 1;	/* Skip the ESC */
					break;
			}
			len -= ++p - s;
			s = p;
		}
		else if (s[0] < 0x20)			/* Ctrl character, shouldn't happen */
		{
			/*
			 * For some reason vim sends me a ^M after hitting return on the
			 * ':' line.  Make sure we ignore this here.
			 */
			len--;		/* Skip this char */
			s++;
		}
		else
		{
			p = s;
			while (len && *p >= 0x20)
			{
				len--;
				p++;
			}
			gui_outstr(s, p - s);
			s = p;
		}
	}
	gui_update_cursor();
	gui_update_scrollbars();
	gui_update_horiz_scrollbar();

	/* 
	 * We need to make sure this is cleared since Athena doesn't tell us when
	 * he is done dragging.
	 */
	gui.dragged_sb = SB_NONE;

	if (vim_strchr(p_guioptions, GO_ASEL) != NULL)
		gui_update_selection();
	gui_mch_flush();				/* In case vim decides to take a nap */
}

	static void
gui_outstr(s, len)
	char_u	*s;
	int		len;
{
	int		this_len;

	if (len == 0)
		return;

	if (len < 0)
		len = STRLEN(s);

	while (gui.col + len > Columns)
	{
		this_len = Columns - gui.col;
		gui_mch_outstr_nowrap(s, this_len, TRUE, FALSE);
		s += this_len;
		len -= this_len;
	}
	gui_mch_outstr_nowrap(s, len, TRUE, FALSE);
}

/*
 * Un-draw the cursor.  Actually this just redraws the character at the given
 * position.
 */
	void
gui_undraw_cursor()
{
	if (IS_CURSOR_VALID())
		gui_redraw_block(gui.cursor_row, gui.cursor_col, gui.cursor_row,
															gui.cursor_col);
}

	void
gui_redraw(x, y, w, h)
	int		x;
	int		y;
	int		w;
	int		h;
{
	int		row1, col1, row2, col2;

	row1 = Y_2_ROW(y);
	col1 = X_2_COL(x);
	row2 = Y_2_ROW(y + h - 1);
	col2 = X_2_COL(x + w - 1);

	gui_redraw_block(row1, col1, row2, col2);

	/* We may need to redraw the cursor */
	gui_update_cursor();
}

	void
gui_redraw_block(row1, col1, row2, col2)
	int		row1;
	int		col1;
	int		row2;
	int		col2;
{
	int		old_row, old_col;
	long_u	old_hl_mask;
	char_u	*screenp, *attrp, first_attr;
	int		idx, len;

	/* Don't try to draw outside the window! */
	/* Check everything, strange values may be caused by big border width */
	col1 = check_col(col1);
	col2 = check_col(col2);
	row1 = check_row(row1);
	row2 = check_row(row2);

	/* Don't try to update when NextScreen is not valid */
	if (!screen_cleared || NextScreen == NULL)
		return;

	/* Remember where our cursor was */
	old_row = gui.row;
	old_col = gui.col;
	old_hl_mask = gui.highlight_mask;

	for (gui.row = row1; gui.row <= row2; gui.row++)
	{
		gui.col = col1;
		screenp = LinePointers[gui.row] + gui.col;
		attrp = screenp + Columns;
		len = col2 - col1 + 1;
		while (len > 0)
		{
			switch (attrp[0])
			{
				case CHAR_INVERT:
					gui.highlight_mask = HL_INVERSE;
					break;
				case CHAR_UNDERL:
					gui.highlight_mask = HL_UNDERLINE;
					break;
				case CHAR_BOLD:
					gui.highlight_mask = HL_BOLD;
					break;
				case CHAR_STDOUT:
					gui.highlight_mask = HL_STANDOUT;
					break;
				case CHAR_ITALIC:
					gui.highlight_mask = HL_ITAL;
					break;
				case CHAR_NORMAL:
				default:
					gui.highlight_mask = HL_NORMAL;
					break;
			}
			first_attr = attrp[0];
			for (idx = 0; len > 0 && attrp[idx] == first_attr; idx++)
				--len;
			gui_mch_outstr_nowrap(screenp, idx, FALSE, FALSE);
			screenp += idx;
			attrp += idx;
		}
	}

	/* Put the cursor back where it was */
	gui.row = old_row;
	gui.col = old_col;
	gui.highlight_mask = old_hl_mask;
}

/*
 * Check bounds for column number
 */
	int
check_col(col)
	int		col;
{
	if (col < 0)
		return 0;
	if (col >= (int)Columns)
		return (int)Columns - 1;
	return col;
}

/*
 * Check bounds for row number
 */
	int
check_row(row)
	int		row;
{
	if (row < 0)
		return 0;
	if (row >= (int)Rows)
		return (int)Rows - 1;
	return row;
}

/*
 * Generic mouse support function.  Add a mouse event to the input buffer with
 * the given properties.
 *	button			--- may be any of MOUSE_LEFT, MOUSE_MIDDLE, MOUSE_RIGHT,
 *						MOUSE_DRAG, or MOUSE_RELEASE.
 *	x, y			--- Coordinates of mouse in pixels.
 *	repeated_click	--- TRUE if this click comes only a short time after a
 *						previous click.
 *	modifiers		--- Bit field which may be any of the following modifiers
 *						or'ed together: MOUSE_SHIFT | MOUSE_CTRL | MOUSE_ALT.
 * This function will ignore drag events where the mouse has not moved to a new
 * character.
 */
	void
gui_send_mouse_event(button, x, y, repeated_click, modifiers)
	int		button;
	int		x;
	int		y;
	int		repeated_click;
	int_u	modifiers;
{
	static int		prev_row = 0, prev_col = 0;
	static int		prev_button = -1;
	static linenr_t prev_topline = 0;
	static int		num_clicks = 1;
	char_u			string[6];
	int				row, col;

	row = Y_2_ROW(y);
	col = X_2_COL(x);

	/*
	 * If we are dragging and the mouse hasn't moved far enough to be on a
	 * different character, then don't send an event to vim.
	 */
	if (button == MOUSE_DRAG && row == prev_row && col == prev_col)
		return;

	/*
	 * If topline has changed (window scrolled) since the last click, reset
	 * repeated_click, because we don't want starting Visual mode when
	 * clicking on a different character in the text.
	 */
	if (curwin->w_topline != prev_topline)
		repeated_click = FALSE;

	string[0] = CSI;	/* this sequence is recognized by check_termcode() */
	string[1] = KS_MOUSE;
	string[2] = K_FILLER;
	if (button != MOUSE_DRAG && button != MOUSE_RELEASE)
	{
		if (repeated_click)
		{
			/*
			 * Handle multiple clicks.  They only count if the mouse is still
			 * pointing at the same character.
			 */
			if (button != prev_button || row != prev_row || col != prev_col)
				num_clicks = 1;
			else if (++num_clicks > 4)
				num_clicks = 1;
		}
		else
			num_clicks = 1;
		prev_button = button;
		prev_topline = curwin->w_topline;

		string[3] = (char_u)(button | 0x20);
		SET_NUM_MOUSE_CLICKS(string[3], num_clicks);
	}
	else
		string[3] = (char_u)button;

	string[3] |= modifiers;
	string[4] = (char_u)(col + ' ' + 1);
	string[5] = (char_u)(row + ' ' + 1);
	add_to_input_buf(string, 6);

	prev_row = row;
	prev_col = col;
}

/*
 * Selection stuff, for cutting and pasting text to other windows.
 */

/*
 * Check whether the VIsual area has changed, and if so try to become the owner
 * of the selection, and free any old converted selection we may still have
 * lying around.  If the VIsual mode has ended, make a copy of what was
 * selected so we can still give it to others.  Will probably have to make sure
 * this is called whenever VIsual mode is ended.
 */
	static void
gui_update_selection()
{
	/* If visual mode is only due to a redo command ("."), then ignore it */
	if (redo_VIsual_busy)
		return;
	if (!VIsual_active)
	{
		gui_mch_clear_selection();
		gui.selection.start = gui.selection.end = VIsual;
	}
	else if (lt(VIsual, curwin->w_cursor))
	{
		if (!equal(gui.selection.start, VIsual) ||
			!equal(gui.selection.end, curwin->w_cursor))
		{
			gui_mch_clear_selection();
			gui.selection.start = VIsual;
			gui.selection.end = curwin->w_cursor;
			gui_free_selection();
			gui_own_selection();
		}
	}
	else
	{
		if (!equal(gui.selection.start, curwin->w_cursor) ||
			!equal(gui.selection.end, VIsual))
		{
			gui_mch_clear_selection();
			gui.selection.start = curwin->w_cursor;
			gui.selection.end = VIsual;
			gui_free_selection();
			gui_own_selection();
		}
	}
}

	void
gui_own_selection()
{
	/*
	 * Also want to check somehow that we are reading from the keyboard rather
	 * than a mapping etc.
	 */
	if (!gui.selection.owned && gui_mch_own_selection())
	{
		gui_free_selection();
		gui.selection.owned = TRUE;
	}
}

	void
gui_lose_selection()
{
	gui_free_selection();
	gui.selection.owned = FALSE;
	gui_mch_lose_selection();
}

	void
gui_copy_selection()
{
	if (VIsual_active)
	{
		if (vim_strchr(p_guioptions, GO_ASEL) == NULL)
			gui_update_selection();
		gui_own_selection();
		if (gui.selection.owned)
			gui_get_selection();
	}
}

	void
gui_auto_select()
{
	if (vim_strchr(p_guioptions, GO_ASEL) != NULL)
		gui_copy_selection();
}

/*
 * Menu stuff.
 */

	void
gui_menu_cb(menu)
	GuiMenu	*menu;
{
	char_u	bytes[3 + sizeof(long_u)];

	bytes[0] = CSI;
	bytes[1] = KS_MENU;
	bytes[2] = K_FILLER;
	add_long_to_buf((long_u)menu, bytes + 3);
	add_to_input_buf(bytes, 3 + sizeof(long_u));
}

/*
 * Return the index into the menu->strings or menu->noremap arrays for the
 * current state.  Returns MENU_INDEX_INVALID if there is no mapping for the
 * given menu in the current mode.
 */
	int
gui_get_menu_index(menu, state)
	GuiMenu	*menu;
	int		state;
{
	int		idx;

	if (VIsual_active)
		idx = MENU_INDEX_VISUAL;
	else if ((state & NORMAL))
		idx = MENU_INDEX_NORMAL;
	else if ((state & INSERT))
		idx = MENU_INDEX_INSERT;
	else if ((state & CMDLINE))
		idx = MENU_INDEX_CMDLINE;
	else
		idx = MENU_INDEX_INVALID;

	if (idx != MENU_INDEX_INVALID && menu->strings[idx] == NULL)
		idx = MENU_INDEX_INVALID;
	return idx;
}

/*
 * Return the modes specified by the given menu command (eg :menu! returns
 * MENU_CMDLINE_MODE | MENU_INSERT_MODE).  If noremap is not NULL, then the
 * flag it points to is set according to whether the command is a "nore"
 * command.  If unmenu is not NULL, then the flag it points to is set
 * according to whether the command is an "unmenu" command.
 */
	static int
gui_get_menu_cmd_modes(cmd, force, noremap, unmenu)
	char_u	*cmd;
	int		force;		/* Was there a "!" after the command? */
	int		*noremap;
	int		*unmenu;
{
	int		modes = 0x0;

	if (*cmd == 'n' && cmd[1] != 'o')	/* nmenu, nnoremenu */
	{
		modes |= MENU_NORMAL_MODE;
		cmd++;
	}
	else if (*cmd == 'v')				/* vmenu, vnoremenu */
	{
		modes |= MENU_VISUAL_MODE;
		cmd++;
	}
	else if (*cmd == 'i')				/* imenu, inoremenu */
	{
		modes |= MENU_INSERT_MODE;
		cmd++;
	}
	else if (*cmd == 'c')				/* cmenu, cnoremenu */
	{
		modes |= MENU_CMDLINE_MODE;
		cmd++;
	}
	else if (force)					/* menu!, noremenu! */
		modes |= MENU_INSERT_MODE | MENU_CMDLINE_MODE;
	else							/* menu, noremenu */
		modes |= MENU_NORMAL_MODE | MENU_VISUAL_MODE;

	if (noremap != NULL)
		*noremap = (*cmd == 'n');
	if (unmenu != NULL)
		*unmenu = (*cmd == 'u');
	return modes;
}

/*
 * Do the :menu commands.
 */
	void
gui_do_menu(cmd, arg, force)
	char_u	*cmd;
	char_u	*arg;
	int		force;
{
	char_u	*menu_path;
	int		modes;
	char_u	*map_to;
	int		noremap;
	int		unmenu;
	char_u	*map_buf;

	modes = gui_get_menu_cmd_modes(cmd, force, &noremap, &unmenu);
	menu_path = arg;
	if (*menu_path == NUL)
	{
		gui_show_menus(menu_path, modes);
		return;
	}
	while (*arg && !vim_iswhite(*arg))
	{
		if ((*arg == '\\' || *arg == Ctrl('V')) && arg[1] != NUL)
			arg++;
		arg++;
	}
	if (*arg != NUL)
		*arg++ = NUL;
	arg = skipwhite(arg);
	map_to = arg;
	if (*map_to == NUL && !unmenu)
	{
		gui_show_menus(menu_path, modes);
		return;
	}
	else if (*map_to != NUL && unmenu)
	{
		EMSG("Trailing characters");
		return;
	}
	if (unmenu)
	{
		if (STRCMP(menu_path, "*") == 0)		/* meaning: remove all menus */
			menu_path = (char_u *)"";
		gui_remove_menu(&gui.root_menu, menu_path, modes);
	}
	else
	{
		/* Replace special key codes */
		map_to = replace_termcodes(map_to, &map_buf, FALSE);
		gui_add_menu_path(menu_path, modes, gui_menu_cb, map_to, noremap);
		vim_free(map_buf);
	}
}

/*
 * Add the menu with the given name to the menu hierarchy
 */
	static int
gui_add_menu_path(path_name, modes, call_back, call_data, noremap)
	char_u	*path_name;
	int		modes;
	void	(*call_back)();
	char_u	*call_data;
	int		noremap;
{
	GuiMenu	**menup;
	GuiMenu	*menu = NULL;
	GuiMenu	*parent;
	char_u	*p;
	char_u	*name;
	int		i;

	/* Make a copy so we can stuff around with it, since it could be const */
	path_name = strsave(path_name);
	if (path_name == NULL)
		return FAIL;
	menup = &gui.root_menu;
	parent = NULL;
	name = path_name;
	while (*name)
	{
		/* Get name of this element in the menu hierarchy */
		p = gui_menu_name_skip(name);

		/* See if it's already there */
		menu = *menup;
		while (menu != NULL)
		{
			if (STRCMP(name, menu->name) == 0)
			{
				if (*p == NUL && menu->children != NULL)
				{
					EMSG("Menu path must not lead to a sub-menu");
					vim_free(path_name);
					return FAIL;
				}
				else if (*p != NUL && menu->children == NULL)
				{
					EMSG("Part of menu-item path is not sub-menu");
					vim_free(path_name);
					return FAIL;
				}
				break;
			}
			menup = &menu->next;
			menu = menu->next;
		}
		if (menu == NULL)
		{
			if (*p == NUL && parent == NULL)
			{
				EMSG("Must not add menu items directly to menu bar");
				vim_free(path_name);
				return FAIL;
			}

			/* Not already there, so lets add it */
			menu = (GuiMenu *)alloc(sizeof(GuiMenu));
			if (menu == NULL)
			{
				vim_free(path_name);
				return FAIL;
			}
			menu->modes = modes;
			menu->name = strsave(name);
			menu->cb = NULL;
			for (i = 0; i < 4; i++)
			{
				menu->strings[i] = NULL;
				menu->noremap[i] = FALSE;
			}
			menu->children = NULL;
			menu->next = NULL;
			if (gui.in_use)	 /* Otherwise it will be added when GUI starts */
			{
				if (*p == NUL)
				{
					/* Real menu item, not sub-menu */
					gui_mch_add_menu_item(menu, parent);

					/* Want to update menus now even if mode not changed */
					force_menu_update = TRUE;
				}
				else
				{
					/* Sub-menu (not at end of path yet) */
					gui_mch_add_menu(menu, parent);
				}
			}
			*menup = menu;
		}
		else
		{
			/*
			 * If this menu option was previously only available in other
			 * modes, then make sure it's available for this one now
			 */
			menu->modes |= modes;
		}

		menup = &menu->children;
		parent = menu;
		name = p;
	}
	vim_free(path_name);

	if (menu != NULL)
	{
		menu->cb = call_back;
		p = (call_data == NULL) ? NULL : strsave(call_data);

		/* May match more than one of these */
		if (modes & MENU_NORMAL_MODE)
		{
			gui_free_menu_string(menu, MENU_INDEX_NORMAL);
			menu->strings[MENU_INDEX_NORMAL] = p;
			menu->noremap[MENU_INDEX_NORMAL] = noremap;
		}
		if (modes & MENU_VISUAL_MODE)
		{
			gui_free_menu_string(menu, MENU_INDEX_VISUAL);
			menu->strings[MENU_INDEX_VISUAL] = p;
			menu->noremap[MENU_INDEX_VISUAL] = noremap;
		}
		if (modes & MENU_INSERT_MODE)
		{
			gui_free_menu_string(menu, MENU_INDEX_INSERT);
			menu->strings[MENU_INDEX_INSERT] = p;
			menu->noremap[MENU_INDEX_INSERT] = noremap;
		}
		if (modes & MENU_CMDLINE_MODE)
		{
			gui_free_menu_string(menu, MENU_INDEX_CMDLINE);
			menu->strings[MENU_INDEX_CMDLINE] = p;
			menu->noremap[MENU_INDEX_CMDLINE] = noremap;
		}
	}
	return OK;
}

/*
 * Remove the (sub)menu with the given name from the menu hierarchy
 * Called recursively.
 */
	static int
gui_remove_menu(menup, name, modes)
	GuiMenu	**menup;
	char_u	*name;
	int		modes;
{
	GuiMenu	*menu;
	GuiMenu	*child;
	char_u	*p;

	if (*menup == NULL)
		return OK;			/* Got to bottom of hierarchy */

	/* Get name of this element in the menu hierarchy */
	p = gui_menu_name_skip(name);

	/* Find the menu */
	menu = *menup;
	while (menu != NULL)
	{
		if (*name == NUL || STRCMP(name, menu->name) == 0)
		{
			if (*p != NUL && menu->children == NULL)
			{
				EMSG("Part of menu-item path is not sub-menu");
				return FAIL;
			}
			if ((menu->modes & modes) != 0x0)
			{
				if (gui_remove_menu(&menu->children, p, modes) == FAIL)
					return FAIL;
			}
			else if (*name != NUL)
			{
				EMSG("Menu only exists in another mode");
				return FAIL;
			}

			/*
			 * When name is empty, we are removing all menu items for the given
			 * modes, so keep looping, otherwise we are just removing the named
			 * menu item (which has been found) so break here.
			 */
			if (*name != NUL)
				break;

			/* Remove the menu item for the given mode[s] */
			menu->modes &= ~modes;

			if (menu->modes == 0x0)
			{
				/* The menu item is no longer valid in ANY mode, so delete it */
				*menup = menu->next;
				gui_free_menu(menu);
			}
			else
				menup = &menu->next;
		}
		else
			menup = &menu->next;
		menu = *menup;
	}
	if (*name != NUL)
	{
		if (menu == NULL)
		{
			EMSG("No menu of that name");
			return FAIL;
		}

		/* Recalculate modes for menu based on the new updated children */
		menu->modes = 0x0;
		for (child = menu->children; child != NULL; child = child->next)
			menu->modes |= child->modes;
		if (menu->modes == 0x0)
		{
			/* The menu item is no longer valid in ANY mode, so delete it */
			*menup = menu->next;
			gui_free_menu(menu);
		}
	}

	return OK;
}

/*
 * Free the given menu structure
 */
	static void
gui_free_menu(menu)
	GuiMenu	*menu;
{
	int		i;

	gui_mch_destroy_menu(menu);		/* Free machine specific menu structures */
	vim_free(menu->name);
	for (i = 0; i < 4; i++)
		gui_free_menu_string(menu, i);
	vim_free(menu);

	/* Want to update menus now even if mode not changed */
	force_menu_update = TRUE;
}

/*
 * Free the menu->string with the given index.
 */
	static void
gui_free_menu_string(menu, idx)
	GuiMenu	*menu;
	int		idx;
{
	int		count = 0;
	int		i;

	for (i = 0; i < 4; i++)
		if (menu->strings[i] == menu->strings[idx])
			count++;
	if (count == 1)
		vim_free(menu->strings[idx]);
	menu->strings[idx] = NULL;
}

/*
 * Show the mapping associated with a menu item or hierarchy in a sub-menu.
 */
	static int
gui_show_menus(path_name, modes)
	char_u	*path_name;
	int		modes;
{
	char_u	*p;
	char_u	*name;
	GuiMenu	*menu;
	GuiMenu	*parent = NULL;

	menu = gui.root_menu;
	name = path_name = strsave(path_name);
	if (path_name == NULL)
		return FAIL;

	/* First, find the (sub)menu with the given name */
	while (*name)
	{
		p = gui_menu_name_skip(name);
		while (menu != NULL)
		{
			if (STRCMP(name, menu->name) == 0)
			{
				/* Found menu */
				if (*p != NUL && menu->children == NULL)
				{
					EMSG("Part of menu-item path is not sub-menu");
					vim_free(path_name);
					return FAIL;
				}
				else if ((menu->modes & modes) == 0x0)
				{
					EMSG("Menu only exists in another mode");
					vim_free(path_name);
					return FAIL;
				}
				break;
			}
			menu = menu->next;
		}
		if (menu == NULL)
		{
			EMSG("No menu of that name");
			vim_free(path_name);
			return FAIL;
		}
		name = p;
		parent = menu;
		menu = menu->children;
	}

	/* Now we have found the matching menu, and we list the mappings */
	set_highlight('t');		/* Highlight title */
	start_highlight();
	MSG_OUTSTR("\n--- Menus ---");
	stop_highlight();

	gui_show_menus_recursive(parent, modes, 0);
	return OK;
}

/*
 * Recursively show the mappings associated with the menus under the given one
 */
	static void
gui_show_menus_recursive(menu, modes, depth)
	GuiMenu	*menu;
	int		modes;
	int		depth;
{
	int		i;
	int		bit;

	if (menu != NULL && (menu->modes & modes) == 0x0)
		return;

	if (menu != NULL)
	{
		msg_outchar('\n');
		if (got_int)			/* "q" hit for "--more--" */
			return;
		for (i = 0; i < depth; i++)
			MSG_OUTSTR("  ");
		set_highlight('d');			/* Same as for directories!? */
		start_highlight();
		msg_outstr(menu->name);
		stop_highlight();
	}

	if (menu != NULL && menu->children == NULL)
	{
		for (bit = 0; bit < 4; bit++)
			if ((menu->modes & modes & (1 << bit)) != 0)
			{
				msg_outchar('\n');
				if (got_int)			/* "q" hit for "--more--" */
					return;
				for (i = 0; i < depth + 2; i++)
					MSG_OUTSTR("  ");
				msg_outchar("nvic"[bit]);
				if (menu->noremap[bit])
					msg_outchar('*');
				else
					msg_outchar(' ');
				MSG_OUTSTR("  ");
				msg_outtrans_special(menu->strings[bit], TRUE);
			}
	}
	else
	{
		if (menu == NULL)
		{
			menu = gui.root_menu;
			depth--;
		}
		else
			menu = menu->children;
		for (; menu != NULL; menu = menu->next)
			gui_show_menus_recursive(menu, modes, depth + 1);
	}
}

/*
 * Used when expanding menu names.
 */
static GuiMenu	*expand_menu = NULL;
static int		expand_modes = 0x0;

/*
 * Work out what to complete when doing command line completion of menu names.
 */
	char_u *
gui_set_context_in_menu_cmd(cmd, arg, force)
	char_u	*cmd;
	char_u	*arg;
	int		force;
{
	char_u	*after_dot;
	char_u	*p;
	char_u	*path_name = NULL;
	char_u	*name;
	int		unmenu;
	GuiMenu	*menu;

	expand_context = EXPAND_UNSUCCESSFUL;

	after_dot = arg;
	for (p = arg; *p && !vim_iswhite(*p); ++p)
	{
		if ((*p == '\\' || *p == Ctrl('V')) && p[1] != NUL)
			p++;
		else if (*p == '.')
			after_dot = p + 1;
	}
	if (*p == NUL)				/* Complete the menu name */
	{
		/*
		 * With :unmenu, you only want to match menus for the appropriate mode.
		 * With :menu though you might want to add a menu with the same name as
		 * one in another mode, so match menus fom other modes too.
		 */
		expand_modes = gui_get_menu_cmd_modes(cmd, force, NULL, &unmenu);
		if (!unmenu)
			expand_modes = MENU_ALL_MODES;

		menu = gui.root_menu;
		if (after_dot != arg)
		{
			path_name = alloc(after_dot - arg);
			if (path_name == NULL)
				return NULL;
			STRNCPY(path_name, arg, after_dot - arg - 1);
			path_name[after_dot - arg - 1] = NUL;
		}
		name = path_name;
		while (name != NULL && *name)
		{
			p = gui_menu_name_skip(name);
			while (menu != NULL)
			{
				if (STRCMP(name, menu->name) == 0)
				{
					/* Found menu */
					if ((*p != NUL && menu->children == NULL)
						|| ((menu->modes & expand_modes) == 0x0))
					{
						/*
						 * Menu path continues, but we have reached a leaf.
						 * Or menu exists only in another mode.
						 */
						vim_free(path_name);
						return NULL;
					}
					break;
				}
				menu = menu->next;
			}
			if (menu == NULL)
			{
				/* No menu found with the name we were looking for */
				vim_free(path_name);
				return NULL;
			}
			name = p;
			menu = menu->children;
		}

		expand_context = EXPAND_MENUS;
		expand_pattern = after_dot;
		expand_menu = menu;
	}
	else						/* We're in the mapping part */
		expand_context = EXPAND_NOTHING;
	return NULL;
}

/*
 * Expand the menu names.
 */
	int
gui_ExpandMenuNames(prog, num_file, file)
	regexp	*prog;
	int		*num_file;
	char_u	***file;
{
	GuiMenu	*menu;
	int		round;
	int		count;

	/*
	 * round == 1: Count the matches.
	 * round == 2: Save the matches into the array.
	 */
	for (round = 1; round <= 2; ++round)
	{
		count = 0;
		for (menu = expand_menu; menu != NULL; menu = menu->next)
			if ((menu->modes & expand_modes) != 0x0
				&& vim_regexec(prog, menu->name, TRUE))
			{
				if (round == 1)
					count++;
				else
					(*file)[count++] = strsave_escaped(menu->name,
													   (char_u *)" \t\\.");
			}
		if (round == 1)
		{
			*num_file = count;
			if (count == 0 || (*file = (char_u **)
						 alloc((unsigned)(count * sizeof(char_u *)))) == NULL)
				return FAIL;
		}
	}
	return OK;
}

/*
 * Skip over this element of the menu path and return the start of the next
 * element.  Any \ and ^Vs are removed from the current element.
 */
	static char_u *
gui_menu_name_skip(name)
	char_u	*name;
{
	char_u	*p;

	for (p = name; *p && *p != '.'; p++)
		if (*p == '\\' || *p == Ctrl('V'))
		{
			STRCPY(p, p + 1);
			if (*p == NUL)
				break;
		}
	if (*p)
		*p++ = NUL;
	return p;
}

/*
 * After we have started the GUI, then we can create any menus that have been
 * defined.  This is done once here.  gui_add_menu_path() may have already been
 * called to define these menus, and may be called again.  This function calls
 * itself recursively.	Should be called at the top level with:
 * gui_create_initial_menus(gui.root_menu, NULL);
 */
	static void
gui_create_initial_menus(menu, parent)
	GuiMenu	*menu;
	GuiMenu	*parent;
{
	while (menu)
	{
		if (menu->children != NULL)
		{
			gui_mch_add_menu(menu, parent);
			gui_create_initial_menus(menu->children, menu);
		}
		else
			gui_mch_add_menu_item(menu, parent);
		menu = menu->next;
	}
}


/*
 * Set which components are present.
 * If "oldval" is not NULL, "oldval" is the previous value, the new * value is
 * in p_guioptions.
 */
	void
gui_init_which_components(oldval)
	char_u	*oldval;
{
	char_u	*p;
	int		i;
	int		grey_old, grey_new;
	char_u	*temp;

	if (oldval != NULL)
	{
		/*
		 * Check if the menu's go from grey to non-grey or vise versa.
		 */
		grey_old = (vim_strchr(oldval, GO_GREY) != NULL);
		grey_new = (vim_strchr(p_guioptions, GO_GREY) != NULL);
		if (grey_old != grey_new)
		{
			temp = p_guioptions;
			p_guioptions = oldval;
			gui_x11_update_menus(MENU_ALL_MODES);
			p_guioptions = temp;
		}
	}

	gui.menu_is_active = FALSE;
	for (i = 0; i < 3; i++)
		gui.which_scrollbars[i] = FALSE;
	for (p = p_guioptions; *p; p++)
		switch (*p)
		{
			case GO_LEFT:
				gui.which_scrollbars[SB_LEFT] = TRUE;
				break;
			case GO_RIGHT:
				gui.which_scrollbars[SB_RIGHT] = TRUE;
				break;
			case GO_BOT:
				gui.which_scrollbars[SB_BOTTOM] = TRUE;
				break;
			case GO_MENUS:
				gui.menu_is_active = TRUE;
				break;
			case GO_GREY:
				/* make menu's have grey items, ignored here */
				break;
			default:
				/* Should give error message for internal error */
				break;
		}
	if (gui.in_use)
		gui_mch_create_which_components();
}


/*
 * Vertical scrollbar stuff:
 */

	static void
gui_update_scrollbars()
{
	WIN		*wp;
	int		worst_update = SB_UPDATE_NOTHING;
	int		val, size, max;
	int		which_sb;
	int		cmdline_height;

	/*
	 * Don't want to update a scrollbar while we're dragging it.  But if we
	 * have both a left and right scrollbar, and we drag one of them, we still
	 * need to update the other one.
	 */
	if ((gui.dragged_sb == SB_LEFT || gui.dragged_sb == SB_RIGHT) &&
			(!gui.which_scrollbars[SB_LEFT] || !gui.which_scrollbars[SB_RIGHT]))
		return;

	if (gui.dragged_sb == SB_LEFT || gui.dragged_sb == SB_RIGHT)
	{
		/*
		 * If we have two scrollbars and one of them is being dragged, just
		 * copy the scrollbar position from the dragged one to the other one.
		 */
		which_sb = SB_LEFT + SB_RIGHT - gui.dragged_sb;
		if (gui.dragged_wp != NULL)
			gui.dragged_wp->w_scrollbar.update[which_sb] = SB_UPDATE_VALUE;
		else
			gui.cmdline_sb.update[which_sb] = SB_UPDATE_VALUE;

		gui_mch_update_scrollbars(SB_UPDATE_VALUE, which_sb);
		return;
	}

	/* Return straight away if there is neither a left nor right scrollbar */
	if (!gui.which_scrollbars[SB_LEFT] && !gui.which_scrollbars[SB_RIGHT])
		return;

	cmdline_height = Rows;
	for (wp = firstwin; wp; wp = wp->w_next)
	{
		cmdline_height -= wp->w_height + wp->w_status_height;
		if (wp->w_buffer == NULL)		/* just in case */
			continue;
#ifdef SCROLL_PAST_END
		max = wp->w_buffer->b_ml.ml_line_count;
#else
		max = wp->w_buffer->b_ml.ml_line_count + wp->w_height - 1;
#endif
		if (max < 1)					/* empty buffer */
			max = 1;
		val = wp->w_topline;
		size = wp->w_height;
#ifdef SCROLL_PAST_END
		if (val > max)					/* just in case */
			val = max;
#else
		if (size > max)					/* just in case */
			size = max;
		if (val > max - size + 1)
		{
			val = max - size + 1;
			if (val < 1)				/* minimal value is 1 */
				val = 1;
		}
#endif
		if (size < 1 || wp->w_botline - 1 > max)
		{
			/*
			 * This can happen during changing files.  Just don't update the
			 * scrollbar for now.
			 */
		}
		else if (wp->w_scrollbar.height == 0)
		{
			/* Must be a new window */
			wp->w_scrollbar.update[SB_LEFT] = SB_UPDATE_CREATE;
			wp->w_scrollbar.update[SB_RIGHT] = SB_UPDATE_CREATE;
			wp->w_scrollbar.value = val;
			wp->w_scrollbar.size = size;
			wp->w_scrollbar.max = max;
			wp->w_scrollbar.top = wp->w_winpos;
			wp->w_scrollbar.height = wp->w_height;
			wp->w_scrollbar.status_height = wp->w_status_height;
			gui.num_scrollbars++;
			worst_update = SB_UPDATE_CREATE;
		}
		else if (wp->w_scrollbar.top != wp->w_winpos
			|| wp->w_scrollbar.height != wp->w_height
			|| wp->w_scrollbar.status_height != wp->w_status_height)
		{
			/* Height of scrollbar has changed */
			wp->w_scrollbar.update[SB_LEFT] = SB_UPDATE_HEIGHT;
			wp->w_scrollbar.update[SB_RIGHT] = SB_UPDATE_HEIGHT;
			wp->w_scrollbar.value = val;
			wp->w_scrollbar.size = size;
			wp->w_scrollbar.max = max;
			wp->w_scrollbar.top = wp->w_winpos;
			wp->w_scrollbar.height = wp->w_height;
			wp->w_scrollbar.status_height = wp->w_status_height;
			if (worst_update < SB_UPDATE_HEIGHT)
				worst_update = SB_UPDATE_HEIGHT;
		}
		else if (wp->w_scrollbar.value != val
			|| wp->w_scrollbar.size != size
			|| wp->w_scrollbar.max != max)
		{
			/* Thumb of scrollbar has moved */
			wp->w_scrollbar.update[SB_LEFT] = SB_UPDATE_VALUE;
			wp->w_scrollbar.update[SB_RIGHT] = SB_UPDATE_VALUE;
			wp->w_scrollbar.value = val;
			wp->w_scrollbar.size = size;
			wp->w_scrollbar.max = max;
			if (worst_update < SB_UPDATE_VALUE)
				worst_update = SB_UPDATE_VALUE;
		}

		/*
		 * We may have just created the left scrollbar say, when we already had
		 * the right one.
		 */
		if (gui.new_sb[SB_LEFT])
			wp->w_scrollbar.update[SB_LEFT] = SB_UPDATE_CREATE;
		if (gui.new_sb[SB_RIGHT])
			wp->w_scrollbar.update[SB_RIGHT] = SB_UPDATE_CREATE;
	}
	if (cmdline_height < 1)
		cmdline_height = 1;             /* Shouldn't happen, but just in case */

	/* Check the command line scrollbar */
	if (gui.cmdline_sb.height != cmdline_height
		|| gui.cmdline_sb.status_height != lastwin->w_status_height)
	{
		/* Height of scrollbar has changed */
		gui.cmdline_sb.update[SB_LEFT] = SB_UPDATE_HEIGHT;
		gui.cmdline_sb.update[SB_RIGHT] = SB_UPDATE_HEIGHT;
		gui.cmdline_sb.value = 0;
		gui.cmdline_sb.size = 1;			/* No thumb */
		gui.cmdline_sb.max = 0;
		gui.cmdline_sb.top = Rows - cmdline_height;
		gui.cmdline_sb.height = cmdline_height;
		gui.cmdline_sb.status_height = lastwin->w_status_height;
		if (worst_update < SB_UPDATE_HEIGHT)
			worst_update = SB_UPDATE_HEIGHT;
	}

	/*
	 * If we have just created the left or right scrollbar-box, then we need to
	 * update the height of the command line scrollbar (it will already be
	 * created).
	 */
	if (gui.new_sb[SB_LEFT])
	{
		gui.cmdline_sb.update[SB_LEFT] = SB_UPDATE_HEIGHT;
		worst_update = SB_UPDATE_CREATE;
		gui.new_sb[SB_LEFT] = FALSE;
	}
	if (gui.new_sb[SB_RIGHT])
	{
		gui.cmdline_sb.update[SB_RIGHT] = SB_UPDATE_HEIGHT;
		worst_update = SB_UPDATE_CREATE;
		gui.new_sb[SB_RIGHT] = FALSE;
	}

	if (worst_update != SB_UPDATE_NOTHING)
	{
		if (gui.which_scrollbars[SB_LEFT] && gui.dragged_sb != SB_LEFT)
			gui_mch_update_scrollbars(worst_update, SB_LEFT);
		if (gui.which_scrollbars[SB_RIGHT] && gui.dragged_sb != SB_RIGHT)
			gui_mch_update_scrollbars(worst_update, SB_RIGHT);
	}
}

/*
 * Scroll a window according to the values set in the globals current_scrollbar
 * and scrollbar_value.  Return TRUE if the cursor in the current window moved
 * or FALSE otherwise.
 */
	int
gui_do_scroll()
{
	WIN		*wp, *old_wp;
	int		i;
	FPOS	old_cursor;

	for (wp = firstwin, i = 0; i < current_scrollbar; i++)
	{
		if (wp == NULL)
			break;
		wp = wp->w_next;
	}
	if (wp != NULL)
	{
		old_cursor = curwin->w_cursor;
		old_wp = curwin;
		curwin = wp;
		curbuf = wp->w_buffer;
		i = (long)scrollbar_value - (long)wp->w_topline;
		if (i < 0)
			scrolldown(-i);
		else if (i > 0)
			scrollup(i);
		if (p_so)
			cursor_correct();
		coladvance(curwin->w_curswant);

		curwin = old_wp;
		curbuf = old_wp->w_buffer;

		if (wp == curwin)
			cursupdate();			/* fix window for 'so' */
		wp->w_redr_type = VALID;
		updateWindow(wp);		/* update window, status line, and cmdline */

		return !equal(curwin->w_cursor, old_cursor);
	}
	else
	{
		/* Command-line scrollbar, unimplemented */
		return FALSE;
	}
}


/*
 * Horizontal scrollbar stuff:
 */

	static void
gui_update_horiz_scrollbar()
{
	int		value, size, max;

	if (!gui.which_scrollbars[SB_BOTTOM])
		return;

	if (gui.dragged_sb == SB_BOTTOM)
		return;

	if (curwin->w_p_wrap && gui.prev_wrap)
		return;
			
	/*
	 * It is possible for the cursor to be invalid if we're in the middle of
	 * something (like changing files).  If so, don't do anything for now.
	 */
	if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		return;

	if (curwin->w_p_wrap)
	{
		value = 0;
		size = Columns;
#ifdef SCROLL_PAST_END
		max = 0;
#else
		max = Columns - 1;
#endif
	}
	else
	{
		value = curwin->w_leftcol;
		size = Columns;
#ifdef SCROLL_PAST_END
		max = gui_get_max_horiz_scroll();
#else
		max = gui_get_max_horiz_scroll() + Columns - 1;
#endif
	}
	gui_mch_update_horiz_scrollbar(value, size, max + 1);
	gui.prev_wrap = curwin->w_p_wrap;
}

/*
 * Determine the maximum value for scrolling right.
 */
	int
gui_get_max_horiz_scroll()
{
	int		max = 0;
	char_u	*p;

	p = ml_get_curline();
	if (p[0] != NUL)
		while (p[1] != NUL)				/* Don't count last character */
			max += chartabsize(*p++, (colnr_t)max);
	return max;
}

/*
 * Do a horizontal scroll.  Return TRUE if the cursor moved, or FALSE otherwise
 */
	int
gui_do_horiz_scroll()
{
	char_u	*p;
	int		i;
	int		vcol;
	int		ret_val = FALSE;

	/* no wrapping, no scrolling */
	if (curwin->w_p_wrap)
		return FALSE;

	curwin->w_leftcol = scrollbar_value;

	i = 0;
	vcol = 0;
	p = ml_get_curline();
	while (p[i] && i < curwin->w_cursor.col && vcol < curwin->w_leftcol)
		vcol += chartabsize(p[i++], (colnr_t)vcol);
	if (vcol < curwin->w_leftcol)
	{
		/*
		 * Cursor is on a character that is at least partly off the left hand
		 * side of the screen.
		 */
		while (p[i] && vcol < curwin->w_leftcol)
			vcol += chartabsize(p[i++], (colnr_t)vcol);
		curwin->w_cursor.col = i;
		curwin->w_set_curswant = TRUE;
		ret_val = TRUE;
	}

	while (p[i] && i <= curwin->w_cursor.col
									&& vcol <= curwin->w_leftcol + Columns)
		vcol += chartabsize(p[i++], (colnr_t)vcol);
	if (vcol > curwin->w_leftcol + Columns)
	{
		/*
		 * Cursor is on a character that is at least partly off the right hand
		 * side of the screen.
		 */
		if (i < 2)
			i = 0;
		else
			i -= 2;
		curwin->w_cursor.col = i;
		curwin->w_set_curswant = TRUE;
		ret_val = TRUE;
	}
	updateScreen(NOT_VALID);
	return ret_val;
}
