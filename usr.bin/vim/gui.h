/*	$OpenBSD: gui.h,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved			by Bram Moolenaar
 *								Motif support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/* For debugging */
/* #define D(x)	printf x; */
#define D(x)

#ifdef USE_GUI_MOTIF
# define USE_GUI_X11
# include <Xm/Xm.h>
#endif

/*
 * No, Athena doesn't work.  Probably never will, there is probably a public
 * domain X widget set that is more like Motif which should be used instead.
 */
#ifdef USE_GUI_ATHENA
# define USE_GUI_X11
# include <X11/Intrinsic.h>
# include <X11/StringDefs.h>
#endif

/*
 * These macros convert between character row/column and pixel coordinates.
 * TEXT_X	- Convert character column into X pixel coord for drawing strings.
 * TEXT_Y	- Convert character row into Y pixel coord for drawing strings.
 * FILL_X	- Convert character column into X pixel coord for filling the area
 *				under the character.
 * FILL_Y	- Convert character row into Y pixel coord for filling the area
 *				under the character.
 * X_2_COL	- Convert X pixel coord into character column.
 * Y_2_ROW	- Convert Y pixel coord into character row.
 */
#define TEXT_X(col)		((col) * gui.char_width  + gui.border_offset)
#define TEXT_Y(row)		((row) * gui.char_height + gui.char_ascent \
												 + gui.border_offset)
#define FILL_X(col)		((col) * gui.char_width  + gui.border_offset)
#define FILL_Y(row)		((row) * gui.char_height + gui.border_offset)
#define X_2_COL(x)		(((x) - gui.border_offset) / gui.char_width)
#define Y_2_ROW(y)		(((y) - gui.border_offset) / gui.char_height)

/* Menu modes */
#define MENU_NORMAL_MODE	0x01
#define MENU_VISUAL_MODE	0x02
#define MENU_INSERT_MODE	0x04
#define MENU_CMDLINE_MODE	0x08
#define MENU_ALL_MODES		0x0f

/* Indices into GuiMenu->strings[] and GuiMenu->noremap[] for each mode */
#define MENU_INDEX_INVALID	-1
#define MENU_INDEX_NORMAL	0
#define MENU_INDEX_VISUAL	1
#define MENU_INDEX_INSERT	2
#define MENU_INDEX_CMDLINE	3

/* Update types for scrollbars, getting more severe on the way down */
#define SB_UPDATE_NOTHING	0
#define SB_UPDATE_VALUE		1
#define SB_UPDATE_HEIGHT	2
#define SB_UPDATE_CREATE	3

/* Indices for arrays of scrollbars */
#define SB_NONE				-1
#define SB_LEFT				0
#define SB_RIGHT			1
#define SB_BOTTOM			2

/* Default size of scrollbar */
#define SB_DEFAULT_WIDTH	20

/* Default height of the menu bar */
#define MENU_DEFAULT_HEIGHT 32

/* Highlighting attribute bits. */
#define HL_NORMAL				0x00
#define HL_INVERSE				0x01
#define HL_BOLD					0x02
#define HL_ITAL					0x04
#define HL_UNDERLINE			0x08
#define HL_STANDOUT				0x10
#define HL_SELECTED				0x20
#define HL_ALL					0x3f

#ifdef USE_GUI_X11

/* Selection states for X11 selection. */
#define SELECT_CLEARED			0
#define SELECT_IN_PROGRESS		1
#define SELECT_DONE				2

#define SELECT_MODE_CHAR		0
#define SELECT_MODE_WORD		1
#define SELECT_MODE_LINE		2

#endif

/*
 * When we know the cursor is no longer being displayed (eg it has been written
 * over).
 */
#define INVALIDATE_CURSOR()		(gui.cursor_row = -1)

/* #define INVALIDATE_CURSOR()	do{printf("Invalidate cursor %d\n", __LINE__); gui.cursor_row = -1;}while(0) */

/*
 * For checking whether cursor needs redrawing, or whether it doesn't need
 * undrawing.
 */
#define IS_CURSOR_VALID()		(gui.cursor_row >= 0)

typedef struct GuiSelection
{
	int			owned;				/* Flag: do we own the selection? */
	FPOS		start;				/* Start of selected area */
	FPOS		end;				/* End of selected area */
#ifdef USE_GUI_X11
	Atom		atom;				/* Vim's own special selection format */
	short_u		origin_row;
	short_u		origin_start_col;
	short_u		origin_end_col;
	short_u		word_start_col;
	short_u		word_end_col;
	FPOS		prev;				/* Previous position */
	short_u		state;				/* Current selection state */
	short_u		mode;				/* Select by char, word, or line. */
#endif
} GuiSelection;

typedef struct GuiMenu
{
	int			modes;				/* Which modes is this menu visible for? */
	char_u		*name;				/* Name shown in menu */
	void		(*cb)();			/* Call-back routine */
	char_u		*strings[4];		/* Mapped string for each mode */
	int			noremap[4];			/* A noremap flag for each mode */
	struct GuiMenu *children;		/* Children of sub-menu */
	struct GuiMenu *next;			/* Next item in menu */
#ifdef USE_GUI_X11
	Widget		id;					/* Manage this to enable item */
	Widget		submenu_id;			/* If this is submenu, add children here */
#endif
} GuiMenu;

typedef struct GuiScrollbar
{
	int			update[2];			/* What kind of update is required? */
									/* (For left & right scrollbars) */
	int			value;				/* Represents top line number visible */
	int			size;				/* Size of scrollbar thumb */
	int			max;				/* Number of lines in buffer */
	int			top;				/* Top of scroll bar (chars from row 0) */
	int			height;				/* Height of scroll bar (num rows) */
	int			status_height;		/* Height of status line */
#ifdef USE_GUI_X11
	Widget		id[2];				/* Id of real scroll bar (left & right) */
#endif
} GuiScrollbar;

typedef struct Gui
{
	int			in_focus;			/* Vim has input focus */
	int			in_use;				/* Is the GUI being used? */
	int			starting;			/* GUI will start in a little while */
	int			dying;				/* Is vim dying? Then output to terminal */
	int			dofork;				/* Use fork() when GUI is starting */
	int			dragged_sb;			/* Which scrollbar being dragged, if any? */
	struct window	*dragged_wp;	/* Which WIN's sb being dragged, if any? */
	int			col;				/* Current cursor column in GUI display */
	int			row;				/* Current cursor row in GUI display */
	int			cursor_col;			/* Physical cursor column in GUI display */
	int			cursor_row;			/* Physical cursor row in GUI display */
	int			num_cols;			/* Number of columns */
	int			num_rows;			/* Number of rows */
	int			scroll_region_top;	/* Top (first) line of scroll region */
	int			scroll_region_bot;	/* Bottom (last) line of scroll region */
	long_u		highlight_mask;		/* Highlight attribute mask */
	GuiSelection selection;			/* Info about selected text */
	GuiMenu		*root_menu;			/* Root of menu hierarchy */
	int			num_scrollbars;		/* Number of scrollbars (= #windows + 1) */
	int			scrollbar_width;	/* Width of scrollbars */
	int			menu_height;		/* Height of the menu bar */
	int			menu_is_active;		/* TRUE if menu is present */
	GuiScrollbar cmdline_sb;		/* Scroll bar for command line */
									/* (Other scrollbars in 'struct window') */
	int			which_scrollbars[3];/* Which scrollbar boxes are active? */
	int			new_sb[3];			/* Which scrollbar boxes are new? */
	int			prev_wrap;			/* For updating the horizontal scrollbar */
	int			char_width;			/* Width of char in pixels */
	int			char_height;		/* Height of char in pixels */
	int			char_ascent;		/* Ascent of char in pixels */
	int			border_width;		/* Width of our border around text area */
	int			border_offset;		/* Total pixel offset for all borders */
#ifdef USE_GUI_X11
	Display		*dpy;				/* X display */
	Window		wid;				/* Window id of text area */
	int			visibility;			/* Is window partially/fully obscured? */
	GC			text_gc;
	GC			back_gc;
	GC			invert_gc;
	XFontStruct	*norm_font;
	XFontStruct	*bold_font;
	XFontStruct	*ital_font;
	XFontStruct	*boldital_font;
	Pixel		back_pixel;			/* Pixel value of background */
	Pixel		norm_pixel;			/* Pixel value of normal text */
	Pixel		bold_pixel;			/* Pixel value of bold text */
	Pixel		ital_pixel;			/* Pixel value of ital text */
	Pixel		underline_pixel;	/* Pixel value of underlined text */
	Pixel		cursor_pixel;		/* Pixel value of cursor */
	Pixel		menu_fg_pixel;		/* Pixel value of menu foregound */
	Pixel		menu_bg_pixel;		/* Pixel value of menu backgound */
	Pixel		scroll_fg_pixel;	/* Pixel value of scrollbar foreground */
	Pixel		scroll_bg_pixel;	/* Pixel value of scrollbar background */

	/* X Resources */
	char_u		*dflt_font;			/* Resource font, used if 'font' not set */
	char_u		*dflt_bold_fn;		/* Resource bold font */
	char_u		*dflt_ital_fn;		/* Resource italic font */
	char_u		*dflt_boldital_fn;	/* Resource bold-italic font */
	char_u		*geom;				/* Geometry, eg "80x24" */
	Bool		rev_video;			/* Use reverse video? */
#endif
} Gui;

extern Gui gui;						/* this is in gui.c */
extern int force_menu_update;		/* this is in gui.c */
