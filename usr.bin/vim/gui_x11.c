/*	$OpenBSD: gui_x11.c,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved			by Bram Moolenaar
 *								GUI/Motif support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "ops.h"

#define VIM_NAME		"vim"
#define VIM_CLASS		"Vim"

/* Default resource values */
#define DFLT_FONT				"7x13"
#define DFLT_MENU_BG_COLOR		"gray77"
#define DFLT_MENU_FG_COLOR		"black"
#define DFLT_SCROLL_BG_COLOR	"gray60"
#define DFLT_SCROLL_FG_COLOR	"gray77"

Widget vimShell = (Widget)NULL;

static XtAppContext app_context;
static Atom   Atom_WM_DELETE_WINDOW;

static Pixel gui_x11_get_color            __ARGS((char_u *));
static void  gui_x11_set_color            __ARGS((GC, Pixel));
static void  gui_x11_set_font             __ARGS((GC, Font));
static void  gui_x11_check_copy_area      __ARGS((void));
static void  gui_x11_update_menus_recurse __ARGS((GuiMenu *, int));

static void  gui_x11_request_selection_cb __ARGS((Widget, XtPointer,
												  Atom *, Atom *, XtPointer,
												  long_u *, int *));

static Boolean  gui_x11_convert_selection_cb __ARGS((Widget, Atom *, Atom *, 
		 										     Atom *, XtPointer *,
												     long_u *, int *));

static void  gui_x11_lose_ownership_cb __ARGS((Widget, Atom *));

static void  gui_x11_wm_protocol_handler __ARGS((Widget, XtPointer,
											XEvent *, Boolean *));

static void gui_x11_invert_area         __ARGS((int, int, int, int));
static void gui_x11_yank_selection      __ARGS((int, int, int, int));
static void gui_x11_get_word_boundaries __ARGS((GuiSelection *, int, int));
static int  gui_x11_get_line_end        __ARGS((int));
static void gui_x11_update_selection    __ARGS((GuiSelection *, int, int, int,
												int));

#define char_class(c)	(c <= ' ' ? ' ' : iswordchar(c))

#define invert_rectangle(r, c, nr, nc)								\
			XFillRectangle(gui.dpy, gui.wid, gui.invert_gc,			\
					FILL_X(c), FILL_Y(r), (nc) * gui.char_width,	\
					(nr) * gui.char_height)


static struct
{
	KeySym	key_sym;
	char_u	vim_code0;
	char_u	vim_code1;
} special_keys[] =
{
	{XK_Up,			'k', 'u'},
	{XK_Down,		'k', 'd'},
	{XK_Left,		'k', 'l'},
	{XK_Right,		'k', 'r'},

	{XK_F1,			'k', '1'},
	{XK_F2,			'k', '2'},
	{XK_F3,			'k', '3'},
	{XK_F4,			'k', '4'},
	{XK_F5,			'k', '5'},
	{XK_F6,			'k', '6'},
	{XK_F7,			'k', '7'},
	{XK_F8,			'k', '8'},
	{XK_F9,			'k', '9'},
	{XK_F10,		'k', ';'},

	{XK_F11,		'F', '1'},
	{XK_F12,		'F', '2'},
	{XK_F13,		'F', '3'},
	{XK_F14,		'F', '4'},
	{XK_F15,		'F', '5'},
	{XK_F16,		'F', '6'},
	{XK_F17,		'F', '7'},
	{XK_F18,		'F', '8'},
	{XK_F19,		'F', '9'},
	{XK_F20,		'F', 'A'},

	{XK_F21,		'F', 'B'},
	{XK_F22,		'F', 'C'},
	{XK_F23,		'F', 'D'},
	{XK_F24,		'F', 'E'},
	{XK_F25,		'F', 'F'},
	{XK_F26,		'F', 'G'},
	{XK_F27,		'F', 'H'},
	{XK_F28,		'F', 'I'},
	{XK_F29,		'F', 'J'},
	{XK_F30,		'F', 'K'},

	{XK_F31,		'F', 'L'},
	{XK_F32,		'F', 'M'},
	{XK_F33,		'F', 'N'},
	{XK_F34,		'F', 'O'},
	{XK_F35,		'F', 'P'},		/* keysymdef.h defines up to F35 */

	{XK_Help,		'%', '1'},
	{XK_Undo,		'&', '8'},
	{XK_BackSpace,	'k', 'b'},
	{XK_Insert,		'k', 'I'},
	{XK_Delete,		'k', 'D'},
	{XK_Home,		'k', 'h'},
	{XK_End,		'@', '7'},
	{XK_Prior,		'k', 'P'},
	{XK_Next,		'k', 'N'},
	{XK_Print,		'%', '9'},

	/* Keypad keys: */
#ifdef XK_KP_Left
	{XK_KP_Left,	'k', 'l'},
	{XK_KP_Right,	'k', 'r'},
	{XK_KP_Up,		'k', 'u'},
	{XK_KP_Down,	'k', 'd'},
	{XK_KP_Insert,	'k', 'I'},
	{XK_KP_Delete,	'k', 'D'},
	{XK_KP_Home,	'k', 'h'},
	{XK_KP_End,		'@', '7'},
	{XK_KP_Prior,	'k', 'P'},
	{XK_KP_Next,	'k', 'N'},
#endif

	/* End of list marker: */
	{(KeySym)0,		0, 0}
};

#define XtNboldColor		"boldColor"
#define XtCBoldColor		"BoldColor"
#define XtNitalicColor		"italicColor"
#define XtCItalicColor		"ItalicColor"
#define XtNunderlineColor	"underlineColor"
#define XtCUnderlineColor	"UnderlineColor"
#define XtNcursorColor		"cursorColor"
#define XtCCursorColor		"CursorColor"
#define XtNboldFont			"boldFont"
#define XtCBoldFont			"BoldFont"
#define XtNitalicFont		"italicFont"
#define XtCItalicFont		"ItalicFont"
#define XtNboldItalicFont	"boldItalicFont"
#define XtCBoldItalicFont	"BoldItalicFont"
#define XtNscrollbarWidth	"scrollbarWidth"
#define XtCScrollbarWidth	"ScrollbarWidth"
#define XtNmenuHeight		"menuHeight"
#define XtCMenuHeight		"MenuHeight"

/* Resources for setting the foreground and background colors of menus */
#define XtNmenuBackground	"menuBackground"
#define XtCMenuBackground	"MenuBackground"
#define XtNmenuForeground	"menuForeground"
#define XtCMenuForeground	"MenuForeground"

/* Resources for setting the foreground and background colors of scrollbars */
#define XtNscrollBackground	"scrollBackground"
#define XtCScrollBackground	"ScrollBackground"
#define XtNscrollForeground	"scrollForeground"
#define XtCScrollForeground	"ScrollForeground"

/*
 * X Resources:
 */
static XtResource vim_resources[] =
{
	{
		XtNforeground,
		XtCForeground,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, norm_pixel),
		XtRString,
		XtDefaultForeground
	},
	{
		XtNbackground,
		XtCBackground,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, back_pixel),
		XtRString,
		XtDefaultBackground
	},
	{
		XtNboldColor,
		XtCBoldColor,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, bold_pixel),
		XtRString,
		XtDefaultForeground
	},
	{
		XtNitalicColor,
		XtCItalicColor,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, ital_pixel),
		XtRString,
		XtDefaultForeground
	},
	{
		XtNunderlineColor,
		XtCUnderlineColor,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, underline_pixel),
		XtRString,
		XtDefaultForeground
	},
	{
		XtNcursorColor,
		XtCCursorColor,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, cursor_pixel),
		XtRString,
		XtDefaultForeground
	},
	{
		XtNfont,
		XtCFont,
		XtRString,
		sizeof(String *),
		XtOffsetOf(Gui, dflt_font),
		XtRImmediate,
		XtDefaultFont
	},
	{
		XtNboldFont,
		XtCBoldFont,
		XtRString,
		sizeof(String *),
		XtOffsetOf(Gui, dflt_bold_fn),
		XtRImmediate,
		""
	},
	{
		XtNitalicFont,
		XtCItalicFont,
		XtRString,
		sizeof(String *),
		XtOffsetOf(Gui, dflt_ital_fn),
		XtRImmediate,
		""
	},
	{
		XtNboldItalicFont,
		XtCBoldItalicFont,
		XtRString,
		sizeof(String *),
		XtOffsetOf(Gui, dflt_boldital_fn),
		XtRImmediate,
		""
	},
	{
		XtNgeometry,
		XtCGeometry,
		XtRString,
		sizeof(String *),
		XtOffsetOf(Gui, geom),
		XtRImmediate,
		""
	},
	{
		XtNreverseVideo,
		XtCReverseVideo,
		XtRBool,
		sizeof(Bool),
		XtOffsetOf(Gui, rev_video),
		XtRImmediate,
		(XtPointer) False
	},
	{
		XtNborderWidth,
		XtCBorderWidth,
		XtRInt,
		sizeof(int),
		XtOffsetOf(Gui, border_width),
		XtRImmediate,
		(XtPointer) 2
	},
	{
		XtNscrollbarWidth,
		XtCScrollbarWidth,
		XtRInt,
		sizeof(int),
		XtOffsetOf(Gui, scrollbar_width),
		XtRImmediate,
		(XtPointer) SB_DEFAULT_WIDTH 
	},
	{
		XtNmenuHeight,
		XtCMenuHeight,
		XtRInt,
		sizeof(int),
		XtOffsetOf(Gui, menu_height),
		XtRImmediate,
		(XtPointer) MENU_DEFAULT_HEIGHT
	},
	{
		XtNmenuForeground,
		XtCMenuForeground,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, menu_fg_pixel),
		XtRString,
		DFLT_MENU_FG_COLOR
	},
	{
		XtNmenuBackground,
		XtCMenuBackground,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, menu_bg_pixel),
		XtRString,
		DFLT_MENU_BG_COLOR
	},
	{
		XtNscrollForeground,
		XtCScrollForeground,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, scroll_fg_pixel),
		XtRString,
		DFLT_SCROLL_FG_COLOR
	},
	{
		XtNscrollBackground,
		XtCScrollBackground,
		XtRPixel,
		sizeof(Pixel),
		XtOffsetOf(Gui, scroll_bg_pixel),
		XtRString,
		DFLT_SCROLL_BG_COLOR
	},
};

/*
 * This table holds all the X GUI command line options allowed.  This includes
 * the standard ones so that we can skip them when vim is started without the
 * GUI (but the GUI might start up later).
 * When changing this, also update doc/vim_gui.txt and the usage message!!!
 */
static XrmOptionDescRec cmdline_options[] =
{
	/* We handle these options ourselves */
	{"-bg",				".background",		XrmoptionSepArg,	NULL},
	{"-background",		".background",		XrmoptionSepArg,	NULL},
	{"-fg",				".foreground",		XrmoptionSepArg,	NULL},
	{"-foreground",		".foreground",		XrmoptionSepArg,	NULL},
	{"-bold",			".boldColor",		XrmoptionSepArg,	NULL},
	{"-italic",			".italicColor",		XrmoptionSepArg,	NULL},
	{"-ul",				".underlineColor",	XrmoptionSepArg,	NULL},
	{"-underline",		".underlineColor",	XrmoptionSepArg,	NULL},
	{"-cursor",			".cursorColor",		XrmoptionSepArg,	NULL},
	{"-fn",				".font",			XrmoptionSepArg,	NULL},
	{"-font",			".font",			XrmoptionSepArg,	NULL},
	{"-boldfont",		".boldFont",		XrmoptionSepArg,	NULL},
	{"-italicfont",		".italicFont",		XrmoptionSepArg,	NULL},
	{"-geom",			".geometry",		XrmoptionSepArg,	NULL},
	{"-geometry",		".geometry",		XrmoptionSepArg,	NULL},
	{"-reverse",		"*reverseVideo",	XrmoptionNoArg,		"True"},
	{"-rv",				"*reverseVideo",	XrmoptionNoArg,		"True"},
	{"+reverse",		"*reverseVideo",	XrmoptionNoArg,		"False"},
	{"+rv",				"*reverseVideo",	XrmoptionNoArg,		"False"},
	{"-display",		".display",			XrmoptionSepArg,	NULL},
	{"-iconic",			"*iconic",			XrmoptionNoArg,		"True"},
	{"-name",			".name",			XrmoptionSepArg,	NULL},
	{"-bw",				".borderWidth",		XrmoptionSepArg,	NULL},
	{"-borderwidth",	".borderWidth",		XrmoptionSepArg,	NULL},
	{"-sw",				".scrollbarWidth",	XrmoptionSepArg,	NULL},
	{"-scrollbarwidth",	".scrollbarWidth",	XrmoptionSepArg,	NULL},
	{"-mh",				".menuHeight",		XrmoptionSepArg,	NULL},
	{"-menuheight",		".menuHeight",		XrmoptionSepArg,	NULL},
	{"-xrm",			NULL,				XrmoptionResArg,	NULL}
};

static int gui_argc = 0;
static char **gui_argv = NULL;

/*
 * Call-back routines.
 */

	void
gui_x11_timer_cb(timed_out, interval_id)
	XtPointer	timed_out;
	XtIntervalId *interval_id;
{
	*((int *)timed_out) = TRUE;
}

	void
gui_x11_visibility_cb(w, dud, event, bool)
	Widget		w;
	XtPointer	dud;
	XEvent		*event;
	Boolean		*bool;
{
	if (event->type != VisibilityNotify)
		return;

	gui.visibility = event->xvisibility.state;

	/*
	 * When we do an XCopyArea(), and the window is partially obscured, we want
	 * to receive an event to tell us whether it worked or not.
	 */
	XSetGraphicsExposures(gui.dpy, gui.text_gc,
		gui.visibility != VisibilityUnobscured);
}

	void
gui_x11_expose_cb(w, dud, event, bool)
	Widget		w;
	XtPointer	dud;
	XEvent		*event;
	Boolean		*bool;
{
	XExposeEvent	*gevent;

	if (event->type != Expose)
		return;

	gevent = (XExposeEvent *)event;
	gui_redraw(gevent->x, gevent->y, gevent->width, gevent->height);

	/* Clear the border areas if needed */
	if (gevent->x < FILL_X(0))
		XClearArea(gui.dpy, gui.wid, 0, 0, FILL_X(0), 0, False);
	if (gevent->y < FILL_Y(0))
		XClearArea(gui.dpy, gui.wid, 0, 0, 0, FILL_Y(0), False);
	if (gevent->x > FILL_X(Columns))
		XClearArea(gui.dpy, gui.wid, FILL_X(Columns), 0, 0, 0, False);
	if (gevent->y > FILL_Y(Rows))
		XClearArea(gui.dpy, gui.wid, 0, FILL_Y(Rows), 0, 0, False);

	if (gui.selection.state != SELECT_CLEARED)
		gui_x11_redraw_selection(gevent->x, gevent->y, gevent->width,
				gevent->height);
}

	void
gui_x11_resize_window_cb(w, dud, event, bool)
	Widget		w;
	XtPointer	dud;
	XEvent		*event;
	Boolean		*bool;
{
	if (event->type != ConfigureNotify)
		return;

	gui_resize_window(event->xconfigure.width, event->xconfigure.height);

	/* Make sure the border strips on the right and bottom get cleared. */
	XClearArea(gui.dpy, gui.wid,    FILL_X(Columns), 0, 0, 0, False);
	XClearArea(gui.dpy, gui.wid, 0, FILL_Y(Rows),       0, 0, False);
}

	void
gui_x11_focus_change_cb(w, data, event, bool)
	Widget		w;
	XtPointer	data;
	XEvent		*event;
	Boolean		*bool;
{
	if (event->type == FocusIn)
		gui.in_focus = TRUE;
	else
		gui.in_focus = FALSE;
	if (gui.row == gui.cursor_row && gui.col == gui.cursor_col)
		INVALIDATE_CURSOR();
	gui_update_cursor();
}

	void
gui_x11_key_hit_cb(w, dud, event, bool)
	Widget		w;
	XtPointer	dud;
	XEvent		*event;
	Boolean		*bool;
{
	XKeyPressedEvent	*ev_press;
	char_u	string[3], string2[3];
	KeySym	key_sym;
	int		num, i;

	ev_press = (XKeyPressedEvent *)event;

	num = XLookupString(ev_press, (char *)string, sizeof(string),
			&key_sym, NULL);
	
	if (key_sym == XK_space)
		string[0] = ' ';		/* Otherwise Ctrl-Space doesn't work */

	/* Check for Alt/Meta key (Mod1Mask) */
	if (num == 1 && (ev_press->state & Mod1Mask))
	{
		/* 
		 * Before we set the 8th bit, check to make sure the user doesn't
		 * already have a mapping defined for this sequence. We determine this
		 * by checking to see if the input would be the same without the
		 * Alt/Meta key.
		 */
		ev_press->state &= ~Mod1Mask;
		if (XLookupString(ev_press, (char *)string2, sizeof(string2),
				&key_sym, NULL) == 1 && string[0] == string2[0])
			string[0] |= 0x80;
		ev_press->state |= Mod1Mask;
	}

#if 0
	if (num == 1 && string[0] == CSI) /* this doesn't work yet */
	{
		string[1] = CSI;
		string[2] = CSI;
		num = 3;
	}
#endif

	/* Check for special keys, making sure BS and DEL are recognised. */
	if (num == 0 || key_sym == XK_BackSpace || key_sym == XK_Delete)
	{
		for (i = 0; special_keys[i].key_sym != (KeySym)0; i++)
		{
			if (special_keys[i].key_sym == key_sym)
			{
				string[0] = CSI;
				string[1] = special_keys[i].vim_code0;
				string[2] = special_keys[i].vim_code1;
				num = 3;
			}
		}
	}

	/* Unrecognised key */
	if (num == 0)
		return;

	/* Special keys (and a few others) may have modifiers */
	if (num == 3 || key_sym == XK_space || key_sym == XK_Tab
		|| key_sym == XK_Return || key_sym == XK_Linefeed
		|| key_sym == XK_Escape)
	{
		string2[0] = CSI;
		string2[1] = KS_MODIFIER;
		string2[2] = 0;
		if (ev_press->state & ShiftMask)
			string2[2] |= MOD_MASK_SHIFT;
		if (ev_press->state & ControlMask)
			string2[2] |= MOD_MASK_CTRL;
		if (ev_press->state & Mod1Mask)
			string2[2] |= MOD_MASK_ALT;
		if (string2[2] != 0)
			add_to_input_buf(string2, 3);
	}
	if (num == 1 && string[0] == Ctrl('C'))
	{
		trash_input_buf();
		got_int = TRUE;
	}
	add_to_input_buf(string, num);
}

	void
gui_x11_mouse_cb(w, dud, event, bool)
	Widget		w;
	XtPointer	dud;
	XEvent		*event;
	Boolean		*bool;
{
	static XtIntervalId timer = (XtIntervalId)0;
	static int	timed_out = TRUE;

	int			button;
	int			repeated_click = FALSE;
	int			x, y;
	int_u		x_modifiers;
	int_u		vim_modifiers;
	int			checkfor;

	if (event->type == MotionNotify)
	{
		x = event->xmotion.x;
		y = event->xmotion.y;
		button = MOUSE_DRAG;
		x_modifiers = event->xmotion.state;
	}
	else
	{
		x = event->xbutton.x;
		y = event->xbutton.y;
		if (event->type == ButtonPress)
		{
			/* Handle multiple clicks */
			if (!timed_out)
			{
				XtRemoveTimeOut(timer);
				repeated_click = TRUE;
			}
			timed_out = FALSE;
			timer = XtAppAddTimeOut(app_context, (long_u)p_mouset,
						gui_x11_timer_cb, &timed_out);
			switch (event->xbutton.button)
			{
				case Button1:	button = MOUSE_LEFT;	break;
				case Button2:	button = MOUSE_MIDDLE;	break;
				case Button3:	button = MOUSE_RIGHT;	break;
				default:
					return;		/* Unknown button */
			}
		}
		else if (event->type == ButtonRelease)
			button = MOUSE_RELEASE;
		else
			return;		/* Unknown mouse event type */

		x_modifiers = event->xbutton.state;
	}

	vim_modifiers = 0x0;
	if (x_modifiers & ShiftMask)
		vim_modifiers |= MOUSE_SHIFT;
	if (x_modifiers & ControlMask)
		vim_modifiers |= MOUSE_CTRL;
	if (x_modifiers & Mod1Mask)		/* Alt or Meta key */
		vim_modifiers |= MOUSE_ALT;

	/* If an x11 selection is in progress, finish it */
	if (gui.selection.state == SELECT_IN_PROGRESS)
	{
		gui_x11_process_selection(button, x, y, repeated_click, vim_modifiers);
		return;
	}

	/* Determine which mouse settings to look for based on the current mode */
	switch (State)
	{
		case NORMAL_BUSY:
		case NORMAL:		checkfor = MOUSE_NORMAL;	break;
		case VISUAL:		checkfor = MOUSE_VISUAL;	break;
		case REPLACE:
		case INSERT:		checkfor = MOUSE_INSERT;	break;
		case HITRETURN:		checkfor = MOUSE_RETURN;	break;

			/*
			 * On the command line, use the X11 selection on all lines but the
			 * command line.
			 */
		case CMDLINE:		
			if (Y_2_ROW(y) < cmdline_row)
				checkfor = ' ';
			else
				checkfor = MOUSE_COMMAND;
			break;

		default:
			checkfor = ' ';
			break;
	};
	/*
	 * Allow selection of text in the command line in "normal" modes.
	 */
	if ((State == NORMAL || State == NORMAL_BUSY ||
									   State == INSERT || State == REPLACE) &&
											Y_2_ROW(y) >= gui.num_rows - p_ch)
		checkfor = ' ';

	/*
	 * If the mouse settings say to not use the mouse, use the X11 selection.
	 * But if Visual is active, assume that only the Visual area will be
	 * selected.
	 */
	if (!mouse_has(checkfor) && !VIsual_active)
	{
		/* If the selection is done, allow the right button to extend it */
		if (gui.selection.state == SELECT_DONE && button == MOUSE_RIGHT)
		{
			gui_x11_process_selection(button, x, y, repeated_click,
					vim_modifiers);
			return;
		}

		/* Allow the left button to start the selection */
		else if (button == MOUSE_LEFT)
		{
			gui_x11_start_selection(button, x, y, repeated_click,
					vim_modifiers);
			return;
		}
	}

	if (gui.selection.state != SELECT_CLEARED)
		gui_mch_clear_selection();
	gui_send_mouse_event(button, x, y, repeated_click, vim_modifiers);
}

/*
 * End of call-back routines
 */

/*
 * Parse the GUI related command-line arguments.  Any arguments used are
 * deleted from argv, and *argc is decremented accordingly.  This is called
 * when vim is started, whether or not the GUI has been started.
 */
	void
gui_mch_prepare(argc, argv)
	int		*argc;
	char	**argv;
{
	int		arg;
	int		i;

	/*
	 * Move all the entries in argv which are relevant to X into gui_argv.
	 */
	gui_argc = 0;
	gui_argv = (char **)lalloc(*argc * sizeof(char *), FALSE);
	if (gui_argv == NULL)
		return;
	gui_argv[gui_argc++] = argv[0];
	arg = 1;
	while (arg < *argc)
	{
		/* Look for argv[arg] in cmdline_options[] table */
		for (i = 0; i < XtNumber(cmdline_options); i++)
			if (strcmp(argv[arg], cmdline_options[i].option) == 0)
				break;

		if (i < XtNumber(cmdline_options))
		{
			/* Found match in table, so move it into gui_argv */
			gui_argv[gui_argc++] = argv[arg];
			if (--*argc > arg)
			{
				vim_memmove(&argv[arg], &argv[arg + 1], (*argc - arg)
													* sizeof(char *));
				if (cmdline_options[i].argKind != XrmoptionNoArg)
				{
					/* Move the options argument as well */
					gui_argv[gui_argc++] = argv[arg];
					if (--*argc > arg)
						vim_memmove(&argv[arg], &argv[arg + 1], (*argc - arg)
															* sizeof(char *));
				}
			}
		}
		else
			arg++;
	}
}

/*
 * Initialise the X GUI.  Create all the windows, set up all the call-backs
 * etc.
 */
	int
gui_mch_init()
{
	Widget		AppShell;
	long_u		gc_mask;
	XGCValues	gc_vals;
	Pixel		tmp_pixel;
	int			x, y, mask;
	unsigned	w, h;

	XtToolkitInitialize();
	app_context = XtCreateApplicationContext();
	gui.dpy = XtOpenDisplay(app_context, 0, VIM_NAME, VIM_CLASS,
		cmdline_options, XtNumber(cmdline_options),
#ifndef XtSpecificationRelease
		(Cardinal*)&gui_argc, gui_argv);
#else
#if XtSpecificationRelease == 4
		(Cardinal*)&gui_argc, gui_argv);
#else
		&gui_argc, gui_argv);
#endif
#endif
	
	vim_free(gui_argv);

	if (gui.dpy == NULL)
	{
		x = full_screen;
		full_screen = FALSE;			/* use fprintf() */
		EMSG("cannot open display");
		full_screen = x;
		return FAIL;
	}

	/* Uncomment this to enable synchronous mode for debugging */
	/* XSynchronize(gui.dpy, True); */

	/*
	 * So converters work.
	 */
	XtInitializeWidgetClass(applicationShellWidgetClass);
	XtInitializeWidgetClass(topLevelShellWidgetClass);

	/*
	 * The applicationShell is created as an unrealized
	 * parent for multiple topLevelShells.	The topLevelShells
	 * are created as popup children of the applicationShell.
	 * This is a recommendation of Paul Asente & Ralph Swick in
	 * _X_Window_System_Toolkit_ p. 677.
	 */
	AppShell = XtVaAppCreateShell(VIM_NAME, VIM_CLASS,
			applicationShellWidgetClass, gui.dpy,
			NULL);

	/*
	 * Get the application resources
	 */
	XtVaGetApplicationResources(AppShell, &gui,
		vim_resources, XtNumber(vim_resources), NULL);

	/*
	 * Check validity of resources
	 */
	if (gui.border_width < 0)
		gui.border_width = 0;

	/* For reverse video, swap foreground and background colours */
	if (gui.rev_video)
	{
		tmp_pixel = gui.norm_pixel;
		gui.norm_pixel = gui.back_pixel;
		gui.back_pixel = tmp_pixel;
	}

	/* Create shell widget to put vim in */
	vimShell = XtVaCreatePopupShell("VIM", 
		topLevelShellWidgetClass, AppShell,
		XtNborderWidth, 0,
		NULL);

	/*
	 * Check that none of the colours are the same as the background color
	 */
	if (gui.norm_pixel == gui.back_pixel)
	{
		gui.norm_pixel = gui_x11_get_color((char_u *)"White");
		if (gui.norm_pixel == gui.back_pixel)
			gui.norm_pixel = gui_x11_get_color((char_u *)"Black");
	}
	if (gui.bold_pixel == gui.back_pixel)
		gui.bold_pixel = gui.norm_pixel;
	if (gui.ital_pixel == gui.back_pixel)
		gui.ital_pixel = gui.norm_pixel;
	if (gui.underline_pixel == gui.back_pixel)
		gui.underline_pixel = gui.norm_pixel;
	if (gui.cursor_pixel == gui.back_pixel)
		gui.cursor_pixel = gui.norm_pixel;

	/*
	 * Set up the GCs.	The font attributes will be set in gui_init_font().
	 */

	gc_mask = GCForeground | GCBackground;
	gc_vals.foreground = gui.norm_pixel;
	gc_vals.background = gui.back_pixel;
	gui.text_gc = XCreateGC(gui.dpy, DefaultRootWindow(gui.dpy), gc_mask,
			&gc_vals);

	gc_vals.foreground = gui.back_pixel;
	gc_vals.background = gui.norm_pixel;
	gui.back_gc = XCreateGC(gui.dpy, DefaultRootWindow(gui.dpy), gc_mask,
			&gc_vals);

	gc_mask |= GCFunction;
	gc_vals.foreground = gui.norm_pixel ^ gui.back_pixel;
	gc_vals.background = gui.norm_pixel ^ gui.back_pixel;
	gc_vals.function   = GXxor;
	gui.invert_gc = XCreateGC(gui.dpy, DefaultRootWindow(gui.dpy), gc_mask,
			&gc_vals);

	gui.visibility = VisibilityUnobscured;
	gui.selection.atom = XInternAtom(gui.dpy, "VIM_SELECTION", False);

	/*
	 * Set up the fonts.  This call will also set the window to the correct
	 * size for the used font.
	 */
	gui.norm_font = NULL;
	gui.bold_font = NULL;
	gui.ital_font = NULL;
	gui.boldital_font = NULL;
	if (gui_init_font() == FAIL)
		return FAIL;

	/* Now adapt the supplied(?) geometry-settings */
	/* Added by Kjetil Jacobsen <kjetilja@stud.cs.uit.no> */
	if (gui.geom != NULL && *gui.geom != NUL)
	{
		mask = XParseGeometry((char *)gui.geom, &x, &y, &w, &h);
		if (mask & WidthValue)
			Columns = w;
		if (mask & HeightValue)
			Rows = h;
		/*
		 * Set the (x,y) position of the main window only if specified in the
		 * users geometry, so we get good defaults when they don't. This needs
		 * to be done before the shell is popped up.
		 */
		if (mask & (XValue|YValue))
			XtVaSetValues(vimShell, XtNgeometry, gui.geom, NULL);
	}
	gui.num_cols = Columns;
	gui.num_rows = Rows;
	gui_reset_scroll_region();

	gui_mch_create_widgets();

	/* Configure the desired scrollbars */
	gui_init_which_components(NULL);

	/* Actually open the window */
	XtPopup(vimShell, XtGrabNone);

	gui_mch_set_winsize();

	gui.wid = gui_mch_get_wid();

	/* Add a callback for the Close item on the window managers menu */
	Atom_WM_DELETE_WINDOW = XInternAtom(gui.dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(gui.dpy, XtWindow(vimShell), &Atom_WM_DELETE_WINDOW, 1);
	XtAddEventHandler(vimShell, NoEventMask, True, gui_x11_wm_protocol_handler,
			                                                 NULL);

#ifdef ENABLE_EDITRES
	/*
	 * Enable editres protocol (see editres(1))
	 * Usually will need to add -lXmu to the linker line as well.
	 */
	{
		extern void _XEditResCheckMessages();
		XtAddEventHandler(vimShell, 0, True, _XEditResCheckMessages,
				(XtPointer)NULL);
	}
#endif

	return OK;
}

	void
gui_mch_exit()
{
	XtCloseDisplay(gui.dpy);
}

/*
 * While unmanaging and re-managing scrollbars etc, we don't want the resize
 * callback to be called, so this function is used to disable this callback,
 * and then re-enable it afterwards.  XSync() makes sure we don't register the
 * callback before the server has finished processing any resize events we have
 * created, otherwise we can get in a loop where the window flashes between two
 * sizes, since resize_window_cb() calls set_winsize().
 */
	void
gui_x11_use_resize_callback(widget, enabled)
	Widget	widget;
	int		enabled;
{
	if (enabled)
	{
		XSync(gui.dpy, False);
		XtAddEventHandler(widget, StructureNotifyMask, FALSE,
			gui_x11_resize_window_cb, (XtPointer)0);
		XtAddEventHandler(widget, ExposureMask, FALSE, gui_x11_expose_cb,
			(XtPointer)0);
	}
	else
	{
		XtRemoveEventHandler(widget, StructureNotifyMask, FALSE,
			gui_x11_resize_window_cb, (XtPointer)0);
		XtRemoveEventHandler(widget, ExposureMask, FALSE, gui_x11_expose_cb,
			(XtPointer)0);
		XSync(gui.dpy, False);
	}
}

/*
 * Initialise vim to use the font with the given name.	Return FAIL if the font
 * could not be loaded, OK otherwise.
 */
	int
gui_mch_init_font(font_name)
	char_u		*font_name;
{
	XFontStruct	*font = NULL;

	if (font_name == NULL)
	{
		/*
		 * If none of the fonts in 'font' could be loaded, try the one set in
		 * the X resource, and finally just try using DFLT_FONT, which will
		 * hopefully always be there.
		 */
		font = XLoadQueryFont(gui.dpy, (char *)gui.dflt_font);
		if (font == NULL)
			font_name = (char_u *)DFLT_FONT;
		font_name = (char_u *)DFLT_FONT;
	}
	if (font == NULL)
		font = XLoadQueryFont(gui.dpy, (char *)font_name);
	if (font == NULL)
		return FAIL;
	if (font->max_bounds.width != font->min_bounds.width)
	{
		sprintf((char *)IObuff, "Font \"%s\" is not fixed-width",
														   (char *)font_name);
		emsg(IObuff);
		XFreeFont(gui.dpy, font);
		return FAIL;
	}
	if (gui.norm_font != NULL)
		XFreeFont(gui.dpy, gui.norm_font);
	gui.norm_font = font;
	gui.char_width = font->max_bounds.width;
	gui.char_height = font->ascent + font->descent;
	gui.char_ascent = font->ascent;

#ifdef DEBUG
	printf("Font Information for '%s':\n", font_name);
	printf("  w = %d, h = %d, ascent = %d, descent = %d\n", gui.char_width,
		   gui.char_height, gui.char_ascent, font->descent);
	printf("  max ascent = %d, max descent = %d, max h = %d\n",
		   font->max_bounds.ascent, font->max_bounds.descent,
		   font->max_bounds.ascent + font->max_bounds.descent);
	printf("  min lbearing = %d, min rbearing = %d\n",
		   font->min_bounds.lbearing, font->min_bounds.rbearing);
	printf("  max lbearing = %d, max rbearing = %d\n",
		   font->max_bounds.lbearing, font->max_bounds.rbearing);
	printf("  leftink = %d, rightink = %d\n",
		   (font->min_bounds.lbearing < 0),
		   (font->max_bounds.rbearing > gui.char_width));
	printf("\n");
#endif

	/*
	 * Try to load other fonts for bold, italic, and bold-italic.
	 * We should also try to work out what font to use for these when they are
	 * not specified by X resources, but we don't yet.
	 */
	if (gui.dflt_bold_fn != NULL && *gui.dflt_bold_fn != NUL)
		gui.bold_font = XLoadQueryFont(gui.dpy, (char *)gui.dflt_bold_fn);
	if (gui.dflt_ital_fn != NULL && *gui.dflt_ital_fn != NUL)
		gui.ital_font = XLoadQueryFont(gui.dpy, (char *)gui.dflt_ital_fn);
	if (gui.dflt_boldital_fn != NULL && *gui.dflt_boldital_fn != NUL)
		gui.boldital_font = XLoadQueryFont(gui.dpy, (char *)gui.dflt_boldital_fn);

	/* Must set the appropriate fonts for all the GCs */
	gui_x11_set_font(gui.text_gc, gui.norm_font->fid);
	gui_x11_set_font(gui.back_gc, gui.norm_font->fid);

	/*
	 * Set the window sizes if the window is already visible.
	 */
	if (vimShell != (Widget)NULL && XtIsRealized(vimShell))
	{
		WIN		*wp;

		gui_mch_set_winsize();

		/* Force the scrollbar heights to get updated when a font is changed */
		if (gui.which_scrollbars[SB_LEFT])
		{
			for (wp = firstwin; wp; wp = wp->w_next)
				wp->w_scrollbar.update[SB_LEFT] = SB_UPDATE_HEIGHT;
			gui_mch_update_scrollbars(SB_UPDATE_HEIGHT, SB_LEFT);
		}
		if (gui.which_scrollbars[SB_RIGHT])
		{
			for (wp = firstwin; wp; wp = wp->w_next)
				wp->w_scrollbar.update[SB_RIGHT] = SB_UPDATE_HEIGHT;
			gui_mch_update_scrollbars(SB_UPDATE_HEIGHT, SB_RIGHT);
		}
	}

	return OK;
}

/*
 * Return OK if the key with the termcap name "name" is supported.
 */
	int
gui_mch_haskey(name)
	char_u	*name;
{
	int i;

	for (i = 0; special_keys[i].key_sym != (KeySym)0; i++)
		if (name[0] == special_keys[i].vim_code0 &&
										 name[1] == special_keys[i].vim_code1)
			return OK;
	return FAIL;
}

/*
 * Return the text window-id and display.  Only required for X-based GUI's
 */
	int
gui_get_x11_windis(win, dis)
	Window	*win;
	Display	**dis;
{
	*win = XtWindow(vimShell);
	*dis = gui.dpy;
	return OK;
}

	void
gui_mch_beep()
{
	XBell(gui.dpy, 0);
}

	void
gui_mch_flash()
{
	/* Do a visual beep by reversing the foreground and background colors */
	XFillRectangle(gui.dpy, gui.wid, gui.invert_gc, 0, 0,
			FILL_X(Columns) + gui.border_offset,
			FILL_Y(Rows) + gui.border_offset);
	XSync(gui.dpy, False);
	mch_delay(20L, TRUE);		/* wait 1/50 of a second */
	XFillRectangle(gui.dpy, gui.wid, gui.invert_gc, 0, 0,
			FILL_X(Columns) + gui.border_offset,
			FILL_Y(Rows) + gui.border_offset);
}

/*
 * Iconify the GUI window.
 */
	void
gui_mch_iconify()
{
	XIconifyWindow(gui.dpy, XtWindow(vimShell), DefaultScreen(gui.dpy));
}

/*
 * Return the Pixel value (color) for the given color name.  This routine was
 * pretty much taken from example code in the Silicon Graphics OSF/Motif
 * Programmer's Guide.
 */
	static Pixel
gui_x11_get_color(name)
	char_u *name;
{
	XrmValue	from, to;

	from.size = STRLEN(name) + 1;
	if (from.size < sizeof(String))
		from.size = sizeof(String);
	from.addr = (char *)name;
	to.addr = NULL;
	XtConvert(vimShell, XtRString, &from, XtRPixel, &to);
	if (to.addr != NULL)
		return (Pixel) *((Pixel *)to.addr);
	else
		return (Pixel)NULL;
}

/*
 * Set the current text font (gc should be either gui.text_gc or gui.back_gc)
 */
	static void
gui_x11_set_font(gc, fid)
	GC		gc;
	Font	fid;
{
	static Font prev_text_fid = (Font) -1;
	static Font prev_back_fid = (Font) -1;

	if (gc == gui.text_gc && fid != prev_text_fid)
	{
		XSetFont(gui.dpy, gc, fid);
		prev_text_fid = fid;
	}
	else if (gc == gui.back_gc && fid != prev_back_fid)
	{
		XSetFont(gui.dpy, gc, fid);
		prev_back_fid = fid;
	}
}

/*
 * Set the current text color (gc should be either gui.text_gc or gui.back_gc)
 */
	static void
gui_x11_set_color(gc, pixel)
	GC		gc;
	Pixel	pixel;
{
	static Pixel prev_text_pixel = (Pixel) -1;
	static Pixel prev_back_pixel = (Pixel) -1;

	if (gc == gui.text_gc && pixel != prev_text_pixel)
	{
		XSetForeground(gui.dpy, gc, pixel);
		prev_text_pixel = pixel;
	}
	else if (gc == gui.back_gc && pixel != prev_back_pixel)
	{
		XSetBackground(gui.dpy, gc, pixel);
		prev_back_pixel = pixel;
	}
}

/*
 * Draw the cursor.
 */
	void
gui_mch_draw_cursor()
{
	/* Only write to the screen after LinePointers[] has been initialized */
	if (screen_cleared && NextScreen != NULL)
	{
		/* Clear the selection if we are about to write over it */
		if (gui.selection.state == SELECT_DONE
				&& gui.row >= gui.selection.start.lnum
				&& gui.row <= gui.selection.end.lnum)
			gui_mch_clear_selection();

		if (gui.row < screen_Rows && gui.col < screen_Columns)
			gui_mch_outstr_nowrap(LinePointers[gui.row] + gui.col,
															  1, FALSE, TRUE);
		if (!gui.in_focus)
		{
			gui_x11_set_color(gui.text_gc, gui.cursor_pixel);
			XDrawRectangle(gui.dpy, gui.wid, gui.text_gc, FILL_X(gui.col),
					FILL_Y(gui.row), gui.char_width - 1, gui.char_height - 1);
		}
	}
}

/*
 * Catch up with any queued X events.  This may put keyboard input into the
 * input buffer, call resize call-backs, trigger timers etc.  If there is
 * nothing in the X event queue (& no timers pending), then we return
 * immediately.
 */
	void
gui_mch_update()
{
	while(XtAppPending(app_context) && !is_input_buf_full())
		XtAppProcessEvent(app_context, XtIMAll);
}

/*
 * The main GUI input routine.	Waits for a character from the keyboard.
 * wtime == -1		Wait forever.
 * wtime == 0		Don't wait.
 * wtime > 0		Wait wtime milliseconds for a character.
 * Returns OK if a character was found to be available within the given time,
 * or FAIL otherwise.
 */
	int
gui_mch_wait_for_chars(wtime)
	int		wtime;
{
	XtIntervalId	timer = (XtIntervalId)0;
	XtIntervalId	updatescript_timer = (XtIntervalId)0;
	/*
	 * Make these static, in case gui_x11_timer_cb is called after leaving
	 * this function (otherwise a random value on the stack may be changed).
	 */
	static int		timed_out;
	static int		do_updatescript;

	timed_out = FALSE;
	do_updatescript = FALSE;

	/*
	 * If we're going to wait a bit, update the menus for the current State.
	 */
	if (wtime != 0)
		gui_x11_update_menus(0);
	gui_mch_update();
	if (!is_input_buf_empty())	/* Got char, return immediately */
		return OK;
	else if (wtime == 0)		/* Don't wait for char */
		return FAIL;
	else if (wtime > 0)
	{
		timer = XtAppAddTimeOut(app_context, (long_u)wtime, gui_x11_timer_cb,
			&timed_out);
	}
	else
	{
		/* We are waiting for ever, so update swap files after p_ut msec's */
		updatescript_timer = XtAppAddTimeOut(app_context, (long_u)p_ut,
										  gui_x11_timer_cb, &do_updatescript);
	}

	while (!timed_out)
	{
		/*
		 * Don't use gui_mch_update() because then we will spin-lock until a
		 * char arrives, instead we use XtAppProcessEvent() to hang until an
		 * event arrives.  No need to check for input_buf_full because we are
		 * returning as soon as it contains a single char.	Note that
		 * XtAppNextEvent() may not be used because it will not return after a
		 * timer event has arrived -- webb
		 */
		XtAppProcessEvent(app_context, XtIMAll);

		/*
		 * If no characters arrive within 'updatetime' milli-seconds, flush all
		 * the swap files to disk.
		 */
		if (do_updatescript)
		{
			updatescript(0);
			do_updatescript = FALSE;
			updatescript_timer = (XtIntervalId)0;
		}
		if (!is_input_buf_empty())
		{
			if (timer != (XtIntervalId)0 && !timed_out)
				XtRemoveTimeOut(timer);
			if (updatescript_timer != (XtIntervalId)0 && !do_updatescript)
				XtRemoveTimeOut(updatescript_timer);
			return OK;
		}
	}
	return FAIL;
}

/*
 * Output routines.
 */

/* Flush any output to the screen */
	void
gui_mch_flush()
{
	XFlush(gui.dpy);
}

/*
 * Clear a rectangular region of the screen from text pos (row1, col1) to
 * (row2, col2) inclusive.
 */
	void
gui_mch_clear_block(row1, col1, row2, col2)
	int		row1;
	int		col1;
	int		row2;
	int		col2;
{
	/* Clear the selection if we are about to write over it */
	if (gui.selection.state == SELECT_DONE
			&& row2 >= gui.selection.start.lnum
			&& row1 <= gui.selection.end.lnum)
		gui_mch_clear_selection();

	XFillRectangle(gui.dpy, gui.wid, gui.back_gc, FILL_X(col1),
		FILL_Y(row1), (col2 - col1 + 1) * gui.char_width,
		(row2 - row1 + 1) * gui.char_height);

	/* Invalidate cursor if it was in this block */
	if (gui.cursor_row >= row1 && gui.cursor_row <= row2
	 && gui.cursor_col >= col1 && gui.cursor_col <= col2)
		INVALIDATE_CURSOR();
}

/*
 * Output the given string at the current cursor position.	If the string is
 * too long to fit on the line, then it is truncated.  wrap_cursor may be
 * TRUE if the cursor position should be wrapped when the end of the line is
 * reached, however the string will still be truncated and not continue on the
 * next line.  is_cursor should only be TRUE when this function is being called
 * to actually draw the cursor.
 */
	void
gui_mch_outstr_nowrap(s, len, wrap_cursor, is_cursor)
	char_u	*s;
	int		len;
	int		wrap_cursor;
	int		is_cursor;
{
	GC		text_gc;
	long_u	highlight_mask;

	if (len == 0)
		return;

	if (len < 0)
		len = STRLEN(s);

	if (is_cursor && gui.in_focus)
		highlight_mask = gui.highlight_mask | HL_INVERSE;
	else
		highlight_mask = gui.highlight_mask;

	if (highlight_mask & (HL_INVERSE | HL_STANDOUT))
		text_gc = gui.back_gc;
	else
		text_gc = gui.text_gc;

	/* Set the font */
	if ((highlight_mask & (HL_BOLD | HL_STANDOUT)) && gui.bold_font != NULL)
		if ((highlight_mask & HL_ITAL) && gui.boldital_font != NULL)
			gui_x11_set_font(text_gc, gui.boldital_font->fid);
		else
			gui_x11_set_font(text_gc, gui.bold_font->fid);
	else if ((highlight_mask & HL_ITAL) && gui.ital_font != NULL)
		gui_x11_set_font(text_gc, gui.ital_font->fid);
	else
		gui_x11_set_font(text_gc, gui.norm_font->fid);

	/* Set the color */
	if (is_cursor && gui.in_focus)
		gui_x11_set_color(text_gc, gui.cursor_pixel);
	else if (highlight_mask & (HL_BOLD | HL_STANDOUT))
		gui_x11_set_color(text_gc, gui.bold_pixel);
	else if (highlight_mask & HL_ITAL)
		gui_x11_set_color(text_gc, gui.ital_pixel);
	else if (highlight_mask & HL_UNDERLINE)
		gui_x11_set_color(text_gc, gui.underline_pixel);
	else
		gui_x11_set_color(text_gc, gui.norm_pixel);

	/* Clear the selection if we are about to write over it */
	if (gui.selection.state == SELECT_DONE
			&& gui.row >= gui.selection.start.lnum
			&& gui.row <= gui.selection.end.lnum)
		gui_mch_clear_selection();

	/* Draw the text */
	XDrawImageString(gui.dpy, gui.wid, text_gc,
		TEXT_X(gui.col), TEXT_Y(gui.row), (char *)s, len);

	/* No bold font, so fake it */
	if ((highlight_mask & (HL_BOLD | HL_STANDOUT)) && gui.bold_font == NULL)
		XDrawString(gui.dpy, gui.wid, text_gc,
			TEXT_X(gui.col) + 1, TEXT_Y(gui.row), (char *)s, len);

	/* Underline the text */
	if ((highlight_mask & HL_UNDERLINE)
	 || ((highlight_mask & HL_ITAL) && gui.ital_font == NULL))
		XDrawLine(gui.dpy, gui.wid, text_gc, FILL_X(gui.col),
			FILL_Y(gui.row + 1) - 1, FILL_X(gui.col + len) - 1,
			FILL_Y(gui.row + 1) - 1);

	if (!is_cursor)
	{
		/* Invalidate the old physical cursor position if we wrote over it */
		if (gui.cursor_row == gui.row && gui.cursor_col >= gui.col
		 && gui.cursor_col < gui.col + len)
			INVALIDATE_CURSOR();

		/* Update the cursor position */
		gui.col += len;
		if (wrap_cursor && gui.col >= Columns)
		{
			gui.col = 0;
			gui.row++;
		}
	}
}

/*
 * Delete the given number of lines from the given row, scrolling up any
 * text further down within the scroll region.
 */
	void
gui_mch_delete_lines(row, num_lines)
	int		row;
	int		num_lines;
{
	if (gui.visibility == VisibilityFullyObscured)
		return;		/* Can't see the window */
	
	if (num_lines <= 0)
		return;

	if (row + num_lines > gui.scroll_region_bot)
	{
		/* Scrolled out of region, just blank the lines out */
		gui_mch_clear_block(row, 0, gui.scroll_region_bot, Columns - 1);
	}
	else
	{
		XCopyArea(gui.dpy, gui.wid, gui.wid, gui.text_gc,
			FILL_X(0), FILL_Y(row + num_lines),
			gui.char_width * Columns,
			gui.char_height * (gui.scroll_region_bot - row - num_lines + 1),
			FILL_X(0), FILL_Y(row));

		/* Update gui.cursor_row if the cursor scrolled or copied over */
		if (gui.cursor_row >= row)
		{
			if (gui.cursor_row < row + num_lines)
				INVALIDATE_CURSOR();
			else if (gui.cursor_row <= gui.scroll_region_bot)
				gui.cursor_row -= num_lines;
		}

		gui_mch_clear_block(gui.scroll_region_bot - num_lines + 1, 0,
			gui.scroll_region_bot, Columns - 1);
		gui_x11_check_copy_area();
	}
}

/*
 * Insert the given number of lines before the given row, scrolling down any
 * following text within the scroll region.
 */
	void
gui_mch_insert_lines(row, num_lines)
	int		row;
	int		num_lines;
{
	if (gui.visibility == VisibilityFullyObscured)
		return;		/* Can't see the window */
	
	if (num_lines <= 0)
		return;

	if (row + num_lines > gui.scroll_region_bot)
	{
		/* Scrolled out of region, just blank the lines out */
		gui_mch_clear_block(row, 0, gui.scroll_region_bot, Columns - 1);
	}
	else
	{
		XCopyArea(gui.dpy, gui.wid, gui.wid, gui.text_gc,
			FILL_X(0), FILL_Y(row),
			gui.char_width * Columns,
			gui.char_height * (gui.scroll_region_bot - row - num_lines + 1),
			FILL_X(0), FILL_Y(row + num_lines));

		/* Update gui.cursor_row if the cursor scrolled or copied over */
		if (gui.cursor_row >= gui.row)
		{
			if (gui.cursor_row <= gui.scroll_region_bot - num_lines)
				gui.cursor_row += num_lines;
			else if (gui.cursor_row <= gui.scroll_region_bot)
				INVALIDATE_CURSOR();
		}

		gui_mch_clear_block(row, 0, row + num_lines - 1, Columns - 1);
		gui_x11_check_copy_area();
	}
}

/*
 * Scroll the text between gui.scroll_region_top & gui.scroll_region_bot by the
 * number of lines given.  Positive scrolls down (text goes up) and negative
 * scrolls up (text goes down).
 */
	static void
gui_x11_check_copy_area()
{
	XEvent					event;
	XGraphicsExposeEvent	*gevent;

	if (gui.visibility != VisibilityPartiallyObscured)
		return;

	XFlush(gui.dpy);

	/* Wait to check whether the scroll worked or not */
	for (;;)
	{
		if (XCheckTypedEvent(gui.dpy, NoExpose, &event))
			return;		/* The scroll worked. */
		
		if (XCheckTypedEvent(gui.dpy, GraphicsExpose, &event))
		{
			gevent = (XGraphicsExposeEvent *)&event;
			gui_redraw(gevent->x, gevent->y, gevent->width, gevent->height);
			if (gevent->count == 0)
				return;			/* This was the last expose event */
		}
		XSync(gui.dpy, False);
	}
}

/*
 * X Selection stuff, for cutting and pasting text to other windows.
 */

	static void
gui_x11_request_selection_cb(w, success, selection, type, value, length, format)
	Widget		w;
	XtPointer	success;
	Atom		*selection;
	Atom		*type;
	XtPointer	value;
	long_u		*length;
	int			*format;
{
	int		motion_type;
	long_u	len;
	char_u	*p;

	if (value == NULL || *length == 0)
	{
		gui_free_selection();	/* ??? */
		*(int *)success = FALSE;
		return;
	}
	motion_type = MCHAR;
	p = (char_u *)value;
	len = *length;
	if (*type == gui.selection.atom)
	{
		motion_type = *p++;
		len--;
	}
	gui_yank_selection(motion_type, p, len);

	XtFree((char *)value);
	*(int *)success = TRUE;
}

	void
gui_request_selection()
{
	XEvent	event;
	Atom	type = gui.selection.atom;
	int		success;
	int		i;

	for (i = 0; i < 2; i++)
	{
		XtGetSelectionValue(vimShell, XA_PRIMARY, type,
			gui_x11_request_selection_cb, (XtPointer)&success, CurrentTime);

		/* Do we need this?: */
		XFlush(gui.dpy);

		/*
		 * Wait for result of selection request, otherwise if we type more
		 * characters, then they will appear before the one that requested the
		 * paste!  Don't worry, we will catch up with any other events later.
		 */
		for (;;)
		{
			if (XCheckTypedEvent(gui.dpy, SelectionNotify, &event))
				break;

			/* Do we need this?: */
			XSync(gui.dpy, False);
		}
		XtDispatchEvent(&event);

		if (success)
			return;
		type = XA_STRING;
	}
}

	static Boolean
gui_x11_convert_selection_cb(w, selection, target, type, value, length, format)
	Widget		w;
	Atom		*selection;
	Atom		*target;
	Atom		*type;
	XtPointer	*value;
	long_u		*length;
	int			*format;
{
	char_u	*string;
	char_u	*result;
	int		motion_type;

	if (!gui.selection.owned)
		return False;		/* Shouldn't ever happen */

	if (*target == gui.selection.atom)
	{
		gui_get_selection();
		motion_type = gui_convert_selection(&string, length);
		if (motion_type < 0)
			return False;

		(*length)++;
		*value = XtMalloc(*length);
		result = (char_u *)*value;
		if (result == NULL)
			return False;
		result[0] = motion_type;
		vim_memmove(result + 1, string, (size_t)(*length - 1));
		*type = *target;
		*format = 8;		/* 8 bits per char */
		return True;
	}
	else if (*target == XA_STRING)
	{
		gui_get_selection();
		motion_type = gui_convert_selection(&string, length);
		if (motion_type < 0)
			return False;

		*value = XtMalloc(*length);
		result = (char_u *)*value;
		if (result == NULL)
			return False;
		vim_memmove(result, string, (size_t)(*length));
		*type = *target;
		*format = 8;		/* 8 bits per char */
		return True;
	}
	else
		return False;
}

	static void
gui_x11_lose_ownership_cb(w, selection)
	Widget	w;
	Atom	*selection;
{
	gui_lose_selection();
}

	void
gui_mch_lose_selection()
{
	XtDisownSelection(vimShell, XA_PRIMARY, CurrentTime);
	gui_mch_clear_selection();
}

	int
gui_mch_own_selection()
{
	if (XtOwnSelection(vimShell, XA_PRIMARY, CurrentTime,
			gui_x11_convert_selection_cb, gui_x11_lose_ownership_cb,
			NULL) == False)
		return FAIL;
	
	return OK;
}

/*
 * Menu stuff.
 */

	void
gui_x11_menu_cb(w, client_data, call_data)
	Widget		w;
	XtPointer	client_data, call_data;
{
	gui_menu_cb((GuiMenu *)client_data);
}

/*
 * Used recursively by gui_x11_update_menus (see below)
 */
	static void
gui_x11_update_menus_recurse(menu, mode)
	GuiMenu	*menu;
	int		mode;
{
	while (menu)
	{
		if (menu->modes & mode)
		{
			if (vim_strchr(p_guioptions, GO_GREY) != NULL)
				XtSetSensitive(menu->id, True);
			else
				XtManageChild(menu->id);
			gui_x11_update_menus_recurse(menu->children, mode);
		}
		else
		{
			if (vim_strchr(p_guioptions, GO_GREY) != NULL)
				XtSetSensitive(menu->id, False);
			else
				XtUnmanageChild(menu->id);
		}
		menu = menu->next;
	}
}

/*
 * Make sure only the valid menu items appear for this mode.  If
 * force_menu_update is not TRUE, then we only do this if the mode has changed
 * since last time.  If "modes" is not 0, then we use these modes instead.
 */
	void
gui_x11_update_menus(modes)
	int		modes;
{
	static int prev_mode = -1;

	int mode = 0;

	if (modes != 0x0)
		mode = modes;
	else if (VIsual_active)
		mode = MENU_VISUAL_MODE;
	else if (State & NORMAL)
		mode = MENU_NORMAL_MODE;
	else if (State & INSERT)
		mode = MENU_INSERT_MODE;
	else if (State & CMDLINE)
		mode = MENU_CMDLINE_MODE;

	if (force_menu_update || mode != prev_mode)
	{
		gui_x11_update_menus_recurse(gui.root_menu, mode);
		prev_mode = mode;
		force_menu_update = FALSE;
	}
}

/*
 * Function called when window closed.  Preserve files and exit.
 * Should put up a requester!
 */
/*ARGSUSED*/
	static void
gui_x11_wm_protocol_handler(w, client_data, event, bool)
	Widget		w;
	XtPointer	client_data;
	XEvent		*event;
	Boolean		*bool;
{
	/*
	 * On some HPUX system with Motif 1.2 this function is somehow called when
	 * starting up.  This if () avoids an unexpected exit.
	 */
	if (event->type != ClientMessage ||
		   ((XClientMessageEvent *)event)->data.l[0] != Atom_WM_DELETE_WINDOW)
		return;

	STRCPY(IObuff, "Vim: Window closed\n");
	preserve_exit();				/* preserve files and exit */
}

/*
 * Compare two screen positions ala strcmp()
 */
	static int
gui_x11_compare_pos(row1, col1, row2, col2)
	short_u		row1;
	short_u		col1;
	short_u		row2;
	short_u		col2;
{
	if (row1 > row2) return( 1);
	if (row1 < row2) return(-1);
	if (col1 > col2) return( 1);
	if (col1 < col2) return(-1);
	                 return( 0);
}

/*
 * Start out the selection
 */
	void
gui_x11_start_selection(button, x, y, repeated_click, modifiers)
    int     button;
	int     x;
	int     y;
	int     repeated_click;
	int_u   modifiers;
{
	GuiSelection	*gs = &gui.selection;

	if (gs->state == SELECT_DONE)
		gui_mch_clear_selection();
	
	gs->start.lnum	= Y_2_ROW(y);
	gs->start.col	= X_2_COL(x);
	gs->end			= gs->start;
	gs->origin_row	= gs->start.lnum;
	gs->state		= SELECT_IN_PROGRESS;

	if (repeated_click)
	{
		if (++(gs->mode) > SELECT_MODE_LINE)
			gs->mode = SELECT_MODE_CHAR;
	}
	else
		gs->mode = SELECT_MODE_CHAR;

	/* clear the cursor until the selection is made */
	gui_undraw_cursor();

	switch (gs->mode)
	{
		case SELECT_MODE_CHAR:
			gs->origin_start_col = gs->start.col;
			gs->word_end_col = gui_x11_get_line_end(gs->start.lnum);
			break;

		case SELECT_MODE_WORD:
			gui_x11_get_word_boundaries(gs, gs->start.lnum, gs->start.col);
			gs->origin_start_col = gs->word_start_col;
			gs->origin_end_col   = gs->word_end_col;

			gui_x11_invert_area(gs->start.lnum, gs->word_start_col,
								gs->end.lnum, gs->word_end_col);
			gs->start.col = gs->word_start_col;
			gs->end.col   = gs->word_end_col;
			break;

		case SELECT_MODE_LINE:
			gui_x11_invert_area(gs->start.lnum, 0, gs->start.lnum,
					gui.num_cols);
			gs->start.col = 0;
			gs->end.col   = gui.num_cols;
			break;
	}

	gs->prev = gs->start;
			
#ifdef DEBUG_SELECTION
	printf("Selection started at (%u,%u)\n", gs->start.lnum, gs->start.col);
#endif
}

/*
 * Continue processing the selection
 */
	void
gui_x11_process_selection(button, x, y, repeated_click, modifiers)
    int     button;
	int     x;
	int     y;
	int     repeated_click;
	int_u   modifiers;
{
	GuiSelection	*gs = &gui.selection;
	int		   		row, col;
	int				diff;
	
	if (button == MOUSE_RELEASE)
	{
		/* Check to make sure we have something selected */
		if (gs->start.lnum == gs->end.lnum && gs->start.col == gs->end.col)
		{
			gui_update_cursor();
			gs->state = SELECT_CLEARED;
			return;
		}

#ifdef DEBUG_SELECTION
		printf("Selection ended: (%u,%u) to (%u,%u)\n", gs->start.lnum,
				gs->start.col, gs->end.lnum, gs->end.col);
#endif
		gui_free_selection();
		gui_own_selection();
		gui_x11_yank_selection(gs->start.lnum, gs->start.col, gs->end.lnum,
				gs->end.col);
		gui_update_cursor();

		gs->state = SELECT_DONE;
		return;
	}

	row = Y_2_ROW(y);
	col = X_2_COL(x);

	row = check_row(row);
	col = check_col(col);

	if (col == gs->prev.col && row == gs->prev.lnum)
		return;

	/*
	 * When extending the selection with the right mouse button, swap the
	 * start and end if the position is before half the selection
	 */
	if (gs->state == SELECT_DONE && button == MOUSE_RIGHT)
	{
		/*
		 * If the click is before the start, or the click is inside the
		 * selection and the start is the closest side, set the origin to the
		 * end of the selection.
		 */
		if (gui_x11_compare_pos(row, col, gs->start.lnum, gs->start.col) < 0 ||
				(gui_x11_compare_pos(row, col, gs->end.lnum, gs->end.col) < 0 &&
				 (((gs->start.lnum == gs->end.lnum &&
					gs->end.col - col > col - gs->start.col)) ||
				  ((diff = (gs->end.lnum - row) - (row - gs->start.lnum)) > 0 ||
				   (diff == 0 && col < (gs->start.col + gs->end.col) / 2)))))
		{
			gs->origin_row = gs->end.lnum;
			gs->origin_start_col = gs->end.col - 1;
			gs->origin_end_col = gs->end.col;
		}
		else
		{
			gs->origin_row = gs->start.lnum;
			gs->origin_start_col = gs->start.col;
			gs->origin_end_col = gs->start.col;
		}
		if (gs->mode == SELECT_MODE_WORD)
		{
			gui_x11_get_word_boundaries(gs, gs->origin_row,
														gs->origin_start_col);
			gs->origin_start_col = gs->word_start_col;
			gs->origin_end_col   = gs->word_end_col;
		}
	}

	/* set state, for when using the right mouse button */
	gs->state = SELECT_IN_PROGRESS;

#ifdef DEBUG_SELECTION
	printf("Selection extending to (%d,%d)\n", row, col);
#endif

	switch (gs->mode)
	{
		case SELECT_MODE_CHAR:
			/* If we're on a different line, find where the line ends */
			if (row != gs->prev.lnum)
				gs->word_end_col = gui_x11_get_line_end(row);

			/* See if we are before or after the origin of the selection */
			if (gui_x11_compare_pos(row, col, gs->origin_row,
												   gs->origin_start_col) >= 0)
			{
				if (col >= (int)gs->word_end_col)
					gui_x11_update_selection(gs, gs->origin_row,
							gs->origin_start_col, row, gui.num_cols);
				else
					gui_x11_update_selection(gs, gs->origin_row,
							gs->origin_start_col, row, col + 1);
			}
			else
			{
				if (col >= (int)gs->word_end_col)
					gui_x11_update_selection(gs, row, gs->word_end_col,
							gs->origin_row, gs->origin_start_col + 1);
				else
					gui_x11_update_selection(gs, row, col, gs->origin_row,
							gs->origin_start_col + 1);
			}
			break;

		case SELECT_MODE_WORD:
			/* If we are still within the same word, do nothing */
			if (row == gs->prev.lnum && col >= (int)gs->word_start_col
					&& col < (int)gs->word_end_col)
				return;

			/* Get new word boundaries */
			gui_x11_get_word_boundaries(gs, row, col);

			/* Handle being after the origin point of selection */
			if (gui_x11_compare_pos(row, col, gs->origin_row,
					gs->origin_start_col) >= 0)
				gui_x11_update_selection(gs, gs->origin_row,
						gs->origin_start_col, row, gs->word_end_col);
			else
				gui_x11_update_selection(gs, row, gs->word_start_col,
						gs->origin_row, gs->origin_end_col);
			break;

		case SELECT_MODE_LINE:
			if (row == gs->prev.lnum)
				return;

			if (gui_x11_compare_pos(row, col, gs->origin_row,
					gs->origin_start_col) >= 0)
				gui_x11_update_selection(gs, gs->origin_row, 0, row,
						gui.num_cols);
			else
				gui_x11_update_selection(gs, row, 0, gs->origin_row,
						gui.num_cols);
			break;
	}

	gs->prev.lnum = row;
	gs->prev.col  = col;

#ifdef DEBUG_SELECTION
		printf("Selection is: (%u,%u) to (%u,%u)\n", gs->start.lnum,
				gs->start.col, gs->end.lnum, gs->end.col);
#endif
}

/*
 * Called after an Expose event to redraw the selection
 */
	void
gui_x11_redraw_selection(x, y, w, h)
	int     x;
	int     y;
	int     w;
	int     h;
{
	GuiSelection	*gs = &gui.selection;
	int				row1, col1, row2, col2;
	int 			row;
	int 			start;
	int 			end;
	
	if (gs->state == SELECT_CLEARED)
		return;

	row1 = Y_2_ROW(y);
	col1 = X_2_COL(x);
	row2 = Y_2_ROW(y + h - 1);
	col2 = X_2_COL(x + w - 1);

	/* Limit the rows that need to be re-drawn */
	if (gs->start.lnum > row1)
		row1 = gs->start.lnum;
	if (gs->end.lnum < row2)
		row2 = gs->end.lnum;
	
	/* Look at each row that might need to be re-drawn */
	for (row = row1; row <= row2; row++)
	{
		/* For the first selection row, use the starting selection column */
		if (row == gs->start.lnum)
			start = gs->start.col;
		else
			start = 0;

		/* For the last selection row, use the ending selection column */
		if (row == gs->end.lnum)
			end = gs->end.col;
		else
			end = gui.num_cols;
		
		if (col1 > start)
			start = col1;

		if (col2 < end)
			end = col2 + 1;

		if (end > start)
			invert_rectangle(row, start, 1, end - start);
	}
}

/*
 * Called from outside to clear selected region from the display
 */
	void
gui_mch_clear_selection()
{
	GuiSelection	*gs = &gui.selection;

	if (gs->state == SELECT_CLEARED)
		return;

	gui_x11_invert_area(gs->start.lnum, gs->start.col, gs->end.lnum,
			gs->end.col);
	gs->state = SELECT_CLEARED;
}

/*
 * Invert a region of the display between a starting and ending row and column
 */
	static void
gui_x11_invert_area(row1, col1, row2, col2)
	int		row1;
	int		col1;
	int		row2;
	int		col2;
{
	/* Swap the from and to positions so the from is always before */
	if (gui_x11_compare_pos(row1, col1, row2, col2) > 0)
	{
		int tmp_row, tmp_col;
		tmp_row = row1;
		tmp_col = col1;
		row1    = row2;
		col1    = col2;
		row2    = tmp_row;
		col2    = tmp_col;
	}

	/* If all on the same line, do it the easy way */
	if (row1 == row2)
	{
		invert_rectangle(row1, col1, 1, col2 - col1);
		return;
	}

	/* Handle a piece of the first line */
	if (col1 > 0)
	{
		invert_rectangle(row1, col1, 1, gui.num_cols - col1);
		row1++;
	}

	/* Handle a piece of the last line */
	if (col2 < gui.num_cols - 1)
	{
		invert_rectangle(row2, 0, 1, col2);
		row2--;
	}

	/* Handle the rectangle thats left */
	if (row2 >= row1)
		invert_rectangle(row1, 0, row2 - row1 + 1, gui.num_cols);
}

/*
 * Yank the currently selected area into the special selection buffer so it
 * will be available for pasting.
 */
	static void
gui_x11_yank_selection(row1, col1, row2, col2)
	int		row1;
	int		col1;
	int		row2;
	int		col2;
{
	char_u	*buffer;
	char_u	*bufp;
	int	  	row;
	int   	start_col;
	int   	end_col;
	int	  	line_end_col;
	int	  	add_newline_flag = FALSE;

	/* Create a temporary buffer for storing the text */
	buffer = lalloc((row2 - row1 + 1) * gui.num_cols + 1, TRUE);
	if (buffer == NULL)
		return;					/* Should there be an error message here? */

	/* Process each row in the selection */
	for (bufp = buffer, row = row1; row <= row2; row++)
	{
		if (row == row1)
			start_col = col1;
		else
			start_col = 0;

		if (row == row2)
			end_col = col2;
		else
			end_col = gui.num_cols;

		line_end_col = gui_x11_get_line_end(row);

		/* See if we need to nuke some trailing whitespace */
		if (end_col >= gui.num_cols && (row < row2 || end_col > line_end_col))
		{
			/* Get rid of trailing whitespace */
			end_col = line_end_col;
			if (end_col < start_col)
				end_col = start_col;

			/* If the last line extended to the end, add an extra newline */
			if (row == row2)
				add_newline_flag = TRUE;
		}

		/* If after the first row, we need to always add a newline */
		if (row > row1)
			*bufp++ = NL;

		if (gui.row < screen_Rows && end_col <= screen_Columns)
		{
			STRNCPY(bufp, &LinePointers[row][start_col], end_col - start_col);
			bufp += end_col - start_col;
		}
	}

	/* Add a newline at the end if the selection ended there */
	if (add_newline_flag)
		*bufp++ = NL;

	gui_yank_selection(MCHAR, buffer, bufp - buffer);
	vim_free(buffer);
}

/*
 * Find the starting and ending positions of the word at the given row and
 * column.
 */
	static void
gui_x11_get_word_boundaries(gs, row, col)
	GuiSelection	*gs;
	int				row;
	int				col;
{
	char	start_class;
	int		temp_col;

	if (row >= screen_Rows || col >= screen_Columns)
		return;

	start_class = char_class(LinePointers[row][col]);

	temp_col = col;
	for ( ; temp_col > 0; temp_col--)
		if (char_class(LinePointers[row][temp_col - 1]) != start_class)
			break;

	gs->word_start_col = temp_col;

	temp_col = col;
	for ( ; temp_col < screen_Columns; temp_col++)
		if (char_class(LinePointers[row][temp_col]) != start_class)
			break;
	gs->word_end_col = temp_col;

#ifdef DEBUG_SELECTION
	printf("Current word: col %u to %u\n", gs->word_start_col,
			gs->word_end_col);
#endif
}

/*
 * Find the column position for the last non-whitespace character on the given
 * line.
 */
	static int
gui_x11_get_line_end(row)
	int			row;
{
	int		i;

	if (row >= screen_Rows)
		return 0;
	for (i = screen_Columns; i > 0; i--)
		if (LinePointers[row][i - 1] != ' ')
			break;
	return i;
}

/*
 * Update the currently selected region by adding and/or subtracting from the
 * beginning or end and inverting the changed area(s).
 */
	static void
gui_x11_update_selection(gs, row1, col1, row2, col2)
	GuiSelection	*gs;
	int				row1;
	int				col1;
	int				row2;
	int				col2;
{
	/* See if we changed at the beginning of the selection */
	if (row1 != gs->start.lnum || col1 != gs->start.col)
	{
		gui_x11_invert_area(row1, col1, gs->start.lnum, gs->start.col);
		gs->start.lnum = row1;
		gs->start.col  = col1;
	}

	/* See if we changed at the end of the selection */
	if (row2 != gs->end.lnum || col2 != gs->end.col)
	{
		gui_x11_invert_area(row2, col2, gs->end.lnum, gs->end.col);
		gs->end.lnum = row2;
		gs->end.col  = col2;
	}
}
