/*	$OpenBSD: gui_athena.c,v 1.3 1996/10/14 03:55:13 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved			by Bram Moolenaar
 *								GUI/Motif support by Robert Webb
 *								Athena port by Bill Foster
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Box.h>

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "ops.h"
#include "gui_at_sb.h"

#define puller_width	19
#define puller_height	19

static char_u puller_bits[] =
{
	0x00,0x00,0xf8,0x00,0x00,0xf8,0xf8,0x7f,0xf8,0x04,0x80,0xf8,0x04,0x80,0xf9,
	0x84,0x81,0xf9,0x84,0x83,0xf9,0x84,0x87,0xf9,0x84,0x8f,0xf9,0x84,0x8f,0xf9,
	0x84,0x87,0xf9,0x84,0x83,0xf9,0x84,0x81,0xf9,0x04,0x80,0xf9,0x04,0x80,0xf9,
	0xf8,0xff,0xf9,0xf0,0x7f,0xf8,0x00,0x00,0xf8,0x00,0x00,0xf8
};

extern Widget vimShell;

static Widget vimPanes;
static Widget vimForm = (Widget)NULL;
static Widget textArea;
static Widget scrollbarBox[3];		/* Left, right & bottom scrollbar boxes */
static Widget bottomScrollbar;			/* Bottom scrollbar */
static Widget leftBottomScrollFiller;	/* Left filler for bottom scrollbar */
static Widget rightBottomScrollFiller;	/* Right filler for bottom scrollbar */
static Widget leftScrollbarFiller;		/* Filler for left scrollbar */
static Widget rightScrollbarFiller;		/* Filler for right scrollbar */
static Widget menuBar;

static void gui_athena_scroll_cb_jump   __ARGS((Widget, XtPointer, XtPointer));
static void gui_athena_scroll_cb_scroll __ARGS((Widget, XtPointer, XtPointer));
static void gui_athena_reorder_menus    __ARGS((void));
static void	gui_athena_pullright_action __ARGS((Widget, XEvent *, String *,
												Cardinal *));

static XtActionsRec		pullAction = { "menu-pullright",
								(XtActionProc)gui_athena_pullright_action };
static XtTranslations	parentTrans, menuTrans;
static Pixmap			pullerBitmap;

/*
 * Scrollbar callback (XtNjumpProc) for when the scrollbar is dragged with the
 * left or middle mouse button.
 */
	static void
gui_athena_scroll_cb_jump(w, client_data, call_data)
	Widget		w;
	XtPointer	client_data, call_data;
{
	char_u		bytes[4 + sizeof(long_u)];
	WIN			*wp;
	GuiScrollbar *sb;
	int			sb_num;
	int         i;
	int         byte_count;
	long_u		value;

	gui.dragged_sb = SB_NONE;
	for (i = 0; i <= SB_BOTTOM; i++)
		if (XtParent(w) == scrollbarBox[i])
		{
			gui.dragged_sb = i;
			break;
		}

	switch (gui.dragged_sb)
	{
		case SB_LEFT:
		case SB_RIGHT:
			gui.dragged_wp = (WIN *)client_data;
			sb_num = 0;
			wp = firstwin;
			for ( ; wp != gui.dragged_wp && wp != NULL; wp = wp->w_next)
				sb_num++;

			if (gui.dragged_wp == NULL)
				return;

			sb = &wp->w_scrollbar;

			value = *((float *)call_data) * (float)(sb->max - 1) + 0.5;
			++value;						/* range is 1 to line_count */
			sb->value = value;
			
			bytes[0] = CSI;
			bytes[1] = KS_SCROLLBAR;
			bytes[2] = K_FILLER;
			bytes[3] = (char_u)sb_num;
			byte_count = 4;
			break;

		case SB_BOTTOM:
											/* why not use sb->max? */
			value = *((float *)call_data) *
									(float)(gui_get_max_horiz_scroll()) + 0.5;
			bytes[0] = CSI;
			bytes[1] = KS_HORIZ_SCROLLBAR;
			bytes[2] = K_FILLER;
			byte_count = 3;
			break;

		case SB_NONE:
		default:
			return;
	}

	add_long_to_buf(value, bytes + byte_count);
	add_to_input_buf(bytes, byte_count + sizeof(long_u));
}

/*
 * Scrollbar callback (XtNscrollProc) for paging up or down with the left or
 * right mouse buttons.
 */
	static void
gui_athena_scroll_cb_scroll(w, client_data, call_data)
	Widget		w;
	XtPointer	client_data, call_data;
{
	char_u		bytes[4 + sizeof(long_u)];
	WIN			*wp;
	GuiScrollbar *sb;
	int			sb_num;
	int         i;
	int         byte_count;
	long		value;
	int			data = (int)call_data;

	for (i = 0; i <= SB_BOTTOM; i++)
		if (XtParent(w) == scrollbarBox[i])
		{
			gui.dragged_sb = i;
			break;
		}

	switch (gui.dragged_sb)
	{
		case SB_LEFT:
		case SB_RIGHT:
			gui.dragged_wp = (WIN *)client_data;
			sb_num = 0;
			wp = firstwin;
			for ( ; wp != gui.dragged_wp && wp != NULL; wp = wp->w_next)
				sb_num++;

			if (gui.dragged_wp == NULL)
				return;

			sb = &wp->w_scrollbar;
			
			if (sb->size > 5)
				i = sb->size - 2;		/* use two lines of context */
			else
				i = sb->size;
			switch (data)
			{
				case  ONE_LINE_DATA: data = 1; break;
				case -ONE_LINE_DATA: data = -1; break;
				case  ONE_PAGE_DATA: data = i; break;
				case -ONE_PAGE_DATA: data = -i; break;
				case  END_PAGE_DATA: data = sb->max; break;
				case -END_PAGE_DATA: data = -sb->max; break;
							default: data = 0; break;
			}
			value = sb->value + data;
			if (value > sb->max)
				value = sb->max;
			else if (value < 1)			/* range is 1 to line_count */
				value = 1;

			bytes[0] = CSI;
			bytes[1] = KS_SCROLLBAR;
			bytes[2] = K_FILLER;
			bytes[3] = (char_u)sb_num;
			byte_count = 4;
			break;

		case SB_BOTTOM:
			if (data < -1)
				data = -(Columns - 5);
			else if (data > 1)
				data = (Columns - 5);
			value = curwin->w_leftcol + data;
			if (value < 0)				/* range is 0 to max_col */
				value = 0;
			else
			{
				int max;
										/* why not use sb->max here? */
				max = gui_get_max_horiz_scroll();
				if (value >= max)
					value = max;
			}

			bytes[0] = CSI;
			bytes[1] = KS_HORIZ_SCROLLBAR;
			bytes[2] = K_FILLER;
			byte_count = 3;
			break;

		case SB_NONE:
		default:
			return;
	}

	/*
	 * This type of scrolling doesn't move the thumb automatically so we need
	 * make sure the scrollbar still gets updated.
	 */
	gui.dragged_sb = SB_NONE;

	add_long_to_buf((long_u)value, bytes + byte_count);
	add_to_input_buf(bytes, byte_count + sizeof(long_u));
}

/*
 * Create all the Athena widgets necessary.
 */
	void
gui_mch_create_widgets()
{
	Dimension	base_width, base_height;

	/*
	 * We don't have any borders handled internally by the textArea to worry
	 * about so only skip over the configured border width.
	 */
	gui.border_offset = gui.border_width;

	base_width  = 2 * gui.border_offset;
	base_height = 2 * gui.border_offset;

	XtInitializeWidgetClass(panedWidgetClass);
	XtInitializeWidgetClass(simpleMenuWidgetClass);
	XtInitializeWidgetClass(vim_scrollbarWidgetClass);
	XtInitializeWidgetClass(labelWidgetClass);

	/* Panes for menu bar, middle stuff, and bottom scrollbar box */
	vimPanes = XtVaCreateManagedWidget("vimPanes",
		panedWidgetClass,	vimShell,
		XtNorientation,		XtorientVertical,
		NULL);

	/* The top menu bar */
	menuBar = XtVaCreateManagedWidget("menuBar",
		boxWidgetClass,			vimPanes,
		XtNmin,					gui.menu_height,
		XtNborderWidth,			1,
		XtNallowResize,			True,
		XtNresizeToPreferred,	True,
		XtNskipAdjust,			True,
		XtNshowGrip,			False,
		XtNforeground,			gui.menu_fg_pixel,
		XtNbackground,			gui.menu_bg_pixel,
		XtNborderColor,			gui.menu_fg_pixel,
		NULL);

	/*
	 * Panes for the middle stuff (left scrollbar box, text area, and right
	 * scrollbar box.
	 */
	vimForm = XtVaCreateManagedWidget("vimForm",
		panedWidgetClass,		vimPanes,
		XtNallowResize,			True,
		XtNorientation,			XtorientHorizontal,
		XtNborderWidth,			0,
		XtNdefaultDistance,		0,
		XtNshowGrip,			False,
		NULL);

	/* Panes for the left window scrollbars. */
	scrollbarBox[SB_LEFT] = XtVaCreateWidget("scrollBarBox",
		panedWidgetClass,		vimForm,
		XtNpreferredPaneSize,	gui.scrollbar_width,
		XtNallowResize,			True,
		XtNskipAdjust,			True,
		XtNborderWidth,			1,
		XtNshowGrip,			False,
		XtNforeground,			gui.scroll_fg_pixel,
		XtNbackground,			gui.scroll_fg_pixel,
		XtNborderColor,			gui.scroll_fg_pixel,
		NULL);

	/* The text area. */
	textArea = XtVaCreateManagedWidget("textArea",
		coreWidgetClass,		vimForm,
		XtNallowResize,			True,
		XtNshowGrip,			False,
		XtNbackground,			gui.back_pixel,
		XtNborderWidth,			0,
		XtNheight,				Rows * gui.char_height + base_height,
		XtNwidth,				Columns * gui.char_width + base_width,
		NULL);

	/* Panes for the right window scrollbars. */
	scrollbarBox[SB_RIGHT] = XtVaCreateWidget("scrollBarBox",
		panedWidgetClass,		vimForm,
		XtNpreferredPaneSize,	gui.scrollbar_width,
		XtNallowResize,			True,
		XtNskipAdjust,			True,
		XtNborderWidth,			1,
		XtNresizeToPreferred,	True,
		XtNshowGrip,			False,
		XtNforeground,			gui.scroll_fg_pixel,
		XtNbackground,			gui.scroll_fg_pixel,
		XtNborderColor,			gui.scroll_fg_pixel,
		NULL);

	/* Panes for the bottom scrollbar and fillers on each side. */
	scrollbarBox[SB_BOTTOM] = XtVaCreateWidget("scrollBarBox",
		panedWidgetClass,		vimPanes,
		XtNpreferredPaneSize,	gui.scrollbar_width,
		XtNallowResize,			True,
		XtNskipAdjust,			True,
		XtNborderWidth,			1,
		XtNresizeToPreferred,	True,
		XtNshowGrip,			False,
		XtNforeground, 			gui.scroll_fg_pixel,
		XtNbackground, 			gui.scroll_fg_pixel,
		XtNborderColor,			gui.scroll_fg_pixel,
		XtNorientation, 		XtorientHorizontal,
		NULL);

	/* A filler for the gap on the left side of the bottom scrollbar. */
	leftBottomScrollFiller = XtVaCreateManagedWidget("",
		labelWidgetClass,	scrollbarBox[SB_BOTTOM],
		XtNshowGrip,		False,
		XtNresize,			False,
		XtNborderWidth,		4,
		XtNmin,				gui.scrollbar_width + 1,
		XtNmax,				gui.scrollbar_width + 1,
		XtNforeground,		gui.scroll_fg_pixel,
		XtNbackground,		gui.scroll_fg_pixel,
		XtNborderColor,		gui.scroll_fg_pixel,
		NULL);

	/* The bottom scrollbar. */
	bottomScrollbar = XtVaCreateManagedWidget("bottomScrollBar",
		vim_scrollbarWidgetClass,	scrollbarBox[SB_BOTTOM],
		XtNresizeToPreferred,	True,
		XtNallowResize,			True,
		XtNskipAdjust,			True,
		XtNshowGrip,			False,
		XtNorientation,			XtorientHorizontal,
		XtNforeground,			gui.scroll_fg_pixel,
		XtNbackground,			gui.scroll_bg_pixel,
		NULL);

	XtAddCallback(bottomScrollbar, XtNjumpProc,
			gui_athena_scroll_cb_jump, (XtPointer)NULL);
	XtAddCallback(bottomScrollbar, XtNscrollProc,
			gui_athena_scroll_cb_scroll, (XtPointer)NULL);

	vim_XawScrollbarSetThumb(bottomScrollbar, 0., 1., 0.);

	/* A filler for the gap on the right side of the bottom scrollbar. */
	rightBottomScrollFiller = XtVaCreateManagedWidget("",
		labelWidgetClass,	scrollbarBox[SB_BOTTOM],
		XtNshowGrip,		False,
		XtNresize,			False,
		XtNborderWidth,		4,
		XtNmin,				gui.scrollbar_width + 1,
		XtNmax,				gui.scrollbar_width + 1,
		XtNforeground,		gui.scroll_fg_pixel,
		XtNbackground,		gui.scroll_fg_pixel,
		NULL);

	/* A filler for the gap on the bottom of the left scrollbar. */
	leftScrollbarFiller = XtVaCreateManagedWidget("",
		labelWidgetClass,	scrollbarBox[SB_LEFT],
		XtNshowGrip,		False,
		XtNresize,			False,
		XtNborderWidth,		4,
		XtNmin,				gui.scrollbar_width + 1,
		XtNmax,				gui.scrollbar_width + 1,
		XtNforeground,		gui.scroll_fg_pixel,
		XtNbackground,		gui.scroll_fg_pixel,
		NULL);

	/* A filler for the gap on the bottom of the right scrollbar. */
	rightScrollbarFiller = XtVaCreateManagedWidget("",
		labelWidgetClass,	scrollbarBox[SB_RIGHT],
		XtNshowGrip,		False,
		XtNresize,			False,
		XtNborderWidth,		4,
		XtNmin,				gui.scrollbar_width + 1,
		XtNmax,				gui.scrollbar_width + 1,
		XtNforeground,		gui.scroll_fg_pixel,
		XtNbackground,		gui.scroll_fg_pixel,
		NULL);

	gui.num_scrollbars = 0;

	/*
	 * Text area callbacks
	 */
	XtAddEventHandler(textArea, VisibilityChangeMask, FALSE,
		gui_x11_visibility_cb, (XtPointer)0);

	XtAddEventHandler(textArea, ExposureMask, FALSE, gui_x11_expose_cb,
		(XtPointer)0);

	XtAddEventHandler(textArea, StructureNotifyMask, FALSE,
		gui_x11_resize_window_cb, (XtPointer)0);

	XtAddEventHandler(vimShell, FocusChangeMask, FALSE, gui_x11_focus_change_cb,
		(XtPointer)0);

	XtAddEventHandler(vimPanes, KeyPressMask, FALSE, gui_x11_key_hit_cb,
		(XtPointer)0);

	XtAddEventHandler(textArea, ButtonPressMask | ButtonReleaseMask |
		ButtonMotionMask, FALSE, gui_x11_mouse_cb, (XtPointer)0);

	parentTrans = XtParseTranslationTable("<BtnMotion>: highlight() menu-pullright()");
	menuTrans = XtParseTranslationTable("<LeaveWindow>: unhighlight() MenuPopdown()\n<BtnUp>: notify() unhighlight() MenuPopdown()\n<BtnMotion>: highlight()");

	XtAppAddActions(XtWidgetToApplicationContext(vimForm), &pullAction, 1);

	pullerBitmap = XCreateBitmapFromData(gui.dpy, DefaultRootWindow(gui.dpy),
							(char *)puller_bits, puller_width, puller_height);
}

	int
gui_mch_get_winsize()
{
	Dimension	base_width, base_height;
	Dimension	total_width, total_height;
	Dimension	left_width = 0, right_width = 0;
	Dimension	bottom_height = 0, menu_height = 0;

	base_height = 2 * gui.border_offset;
	base_width  = 2 * gui.border_offset;

	if (gui.which_scrollbars[SB_LEFT])
		XtVaGetValues(scrollbarBox[SB_LEFT], XtNwidth, &left_width, NULL);

	if (gui.which_scrollbars[SB_RIGHT])
		XtVaGetValues(scrollbarBox[SB_RIGHT], XtNwidth, &right_width, NULL);

	if (gui.which_scrollbars[SB_BOTTOM])
		XtVaGetValues(scrollbarBox[SB_BOTTOM], XtNheight, &bottom_height, NULL);

	if (XtIsManaged(menuBar))
		XtVaGetValues(menuBar, XtNheight, &menu_height, NULL);

	base_width  += left_width + right_width;
	base_height += menu_height + bottom_height;

	XtVaGetValues(vimShell,
		XtNheight, &total_height,
		XtNwidth,  &total_width,
		NULL);

	gui.num_rows = (int)(total_height - base_height) / gui.char_height;
	gui.num_cols = (int)(total_width  - base_width)  / gui.char_width;

	Rows    = gui.num_rows;
	Columns = gui.num_cols;
	gui_reset_scroll_region();

	return OK;
}

	void
gui_mch_set_winsize()
{
	Dimension	left_width = 0, right_width = 0;
	Dimension	bottom_height = 0, menu_height = 0;
	Dimension	base_width, base_height;

	base_width  = 2 * gui.border_offset;
	base_height = 2 * gui.border_offset;

	if (gui.which_scrollbars[SB_LEFT])
		XtVaGetValues(scrollbarBox[SB_LEFT], XtNwidth, &left_width, NULL);

	if (gui.which_scrollbars[SB_RIGHT])
		XtVaGetValues(scrollbarBox[SB_RIGHT], XtNwidth, &right_width, NULL);

	if (gui.which_scrollbars[SB_BOTTOM])
		XtVaGetValues(scrollbarBox[SB_BOTTOM], XtNheight, &bottom_height, NULL);

	if (XtIsManaged(menuBar))
		XtVaGetValues(menuBar, XtNheight, &menu_height, NULL);

	base_width  += left_width + right_width;
	base_height += menu_height + bottom_height;

	XtVaSetValues(vimShell,
		XtNwidthInc,   gui.char_width,
		XtNheightInc,  gui.char_height,
		XtNbaseWidth,  base_width,
		XtNbaseHeight, base_height,
		XtNminWidth,   base_width  + MIN_COLUMNS * gui.char_width,
		XtNminHeight,  base_height + MIN_ROWS    * gui.char_height,
		XtNwidth,	   base_width  + Columns     * gui.char_width,
		XtNheight,	   base_height + Rows        * gui.char_height,
		NULL);
}

/*
 * Menu stuff.
 */

	void
gui_mch_add_menu(menu, parent)
	GuiMenu	*menu;
	GuiMenu	*parent;
{
	char_u	*pullright_name;

	if (parent == NULL)
	{
		menu->id = XtVaCreateManagedWidget(menu->name,
			menuButtonWidgetClass, menuBar,
			XtNmenuName, menu->name,
			XtNforeground, gui.menu_fg_pixel,
			XtNbackground, gui.menu_bg_pixel,
			NULL);

		menu->submenu_id = XtVaCreatePopupShell(menu->name,
			simpleMenuWidgetClass, menu->id,
			XtNforeground, gui.menu_fg_pixel,
			XtNbackground, gui.menu_bg_pixel,
			NULL);

		gui_athena_reorder_menus();
	}
	else
	{
		menu->id = XtVaCreateManagedWidget(menu->name,
			smeBSBObjectClass, parent->submenu_id,
			XtNforeground, gui.menu_fg_pixel,
			XtNbackground, gui.menu_bg_pixel,
			XtNrightMargin, puller_width,
			XtNrightBitmap, pullerBitmap,

			NULL);
		XtAddCallback(menu->id, XtNcallback, gui_x11_menu_cb,
			(XtPointer)menu);

		pullright_name = strnsave(menu->name,
								   STRLEN(menu->name) + strlen("-pullright"));
		strcat((char *)pullright_name, "-pullright");
		menu->submenu_id = XtVaCreatePopupShell(pullright_name,
			simpleMenuWidgetClass, parent->submenu_id,
			XtNforeground, gui.menu_fg_pixel,
			XtNbackground, gui.menu_bg_pixel,
			XtNtranslations, menuTrans,
			NULL);
		vim_free(pullright_name);

		XtOverrideTranslations(parent->submenu_id, parentTrans);
	}
}

	void
gui_mch_add_menu_item(menu, parent)
	GuiMenu	*menu;
	GuiMenu	*parent;
{
	menu->submenu_id = (Widget)0;
	menu->id = XtVaCreateManagedWidget(menu->name,
		smeBSBObjectClass, parent->submenu_id,
		XtNforeground, gui.menu_fg_pixel,
		XtNbackground, gui.menu_bg_pixel,
		NULL);
	XtAddCallback(menu->id, XtNcallback, gui_x11_menu_cb,
		(XtPointer)menu);
}

/*
 * Destroy the machine specific menu widget.
 */
	void
gui_mch_destroy_menu(menu)
	GuiMenu	*menu;
{
	if (menu->id != (Widget)NULL)
	{
		/*
		 * This is a hack for the Athena simpleMenuWidget to keep it from
		 * getting a BadValue error when it's last child is destroyed. We
		 * check to see if this is the last child and if so, go ahead and
		 * delete the parent ahead of time. The parent will delete it's
		 * children like all good widgets do.
		 */
		if (XtParent(menu->id) != menuBar)
		{
			int num_children;

			XtVaGetValues(XtParent(menu->id),
					XtNnumChildren, &num_children, NULL);
			if (num_children <= 1)
				XtDestroyWidget(XtParent(menu->id));
			else
				XtDestroyWidget(menu->id);
		}
		else
			XtDestroyWidget(menu->id);
		menu->id = (Widget)NULL;
	}
}

/*
 * Reorder the menus so "Help" is the rightmost item on the menu.
 */
	static void
gui_athena_reorder_menus()
{
	Widget	*children;
	Widget  help_widget = (Widget)NULL;
	int		num_children;
	int		i;

	XtVaGetValues(menuBar,
			XtNchildren,    &children,
			XtNnumChildren, &num_children,
			NULL);

	XtUnmanageChildren(children, num_children);

	for (i = 0; i < num_children - 1; i++)
		if (help_widget == (Widget)NULL)
		{
			if (strcmp((char *)XtName(children[i]), "Help") == 0)
			{
				help_widget = children[i];
				children[i] = children[i + 1];
			}
		}
		else
			children[i] = children[i + 1];

	if (help_widget != (Widget)NULL)
		children[num_children - 1] = help_widget;

	XtManageChildren(children, num_children);
}


/*
 * Scrollbar stuff:
 */

	void
gui_mch_create_which_components()
{
	static int prev_which_scrollbars[3] = {-1, -1, -1};
	static int prev_menu_is_active = -1;

	int		  i;
	WIN		  *wp;

	/*
	 * When removing the left/right scrollbar and creating the right/left
	 * scrollbar, we have to force a redraw (the size of the text area doesn't
	 * change).
	 */
	if (prev_which_scrollbars[SB_LEFT] != gui.which_scrollbars[SB_LEFT] &&
			prev_which_scrollbars[SB_RIGHT] != gui.which_scrollbars[SB_RIGHT])
		must_redraw = CLEAR;

	gui_x11_use_resize_callback(textArea, FALSE);

	for (i = 0; i < 3; i++)
	{
		if (gui.which_scrollbars[i] != prev_which_scrollbars[i])
		{
			if (gui.which_scrollbars[i])
			{
				switch (i)
				{
					/* When adding the left one, we need to reorder them all */
					case SB_LEFT:
						XtUnmanageChild(textArea);
						if (gui.which_scrollbars[SB_RIGHT])
							XtUnmanageChild(scrollbarBox[SB_RIGHT]);
						XtManageChild(scrollbarBox[SB_LEFT]);
						XtManageChild(textArea);
						if (gui.which_scrollbars[SB_RIGHT])
							XtManageChild(scrollbarBox[SB_RIGHT]);

						/*
						 * When adding at the left and we have a bottom
						 * scrollbar, we need to reorder these too.
						 */
						if (gui.which_scrollbars[SB_BOTTOM])
						{
							XtUnmanageChild(bottomScrollbar);
							if (gui.which_scrollbars[SB_RIGHT])
								XtUnmanageChild(rightBottomScrollFiller);

							XtManageChild(leftBottomScrollFiller);
							XtManageChild(bottomScrollbar);
							if (gui.which_scrollbars[SB_RIGHT])
								XtManageChild(rightBottomScrollFiller);
						}
						break;

					case SB_RIGHT:
						XtManageChild(rightBottomScrollFiller);
						XtManageChild(scrollbarBox[i]);
						break;

					case SB_BOTTOM:
						/* Unmanage the bottom scrollbar and fillers */
						XtUnmanageChild(leftBottomScrollFiller);
						XtUnmanageChild(bottomScrollbar);
						XtUnmanageChild(rightBottomScrollFiller);

						/*
						 * Now manage the bottom scrollbar and fillers that
						 * are supposed to be there.
						 */
						if (gui.which_scrollbars[SB_LEFT])
							XtManageChild(leftBottomScrollFiller);
						XtManageChild(bottomScrollbar);
						if (gui.which_scrollbars[SB_RIGHT])
							XtManageChild(rightBottomScrollFiller);

						XtManageChild(scrollbarBox[i]);
						break;
				}
			}
			else
			{
				switch (i)
				{
					case SB_LEFT:
						XtUnmanageChild(leftBottomScrollFiller);
						break;

					case SB_RIGHT:
						XtUnmanageChild(rightBottomScrollFiller);
						break;
				}
				XtUnmanageChild(scrollbarBox[i]);
			}
		}
		if (gui.which_scrollbars[i] != prev_which_scrollbars[i])
		{
			if (i == SB_LEFT || i == SB_RIGHT)
			{
				if (gui.which_scrollbars[i])
				{
					/* Scrollbar box has just appeared */
					gui.new_sb[i] = TRUE;
				}
				else if (prev_which_scrollbars[i] == TRUE)
				{
					/* Scrollbar box has just been deleted */
					for (wp = firstwin; wp != NULL; wp = wp->w_next)
						XtDestroyWidget(wp->w_scrollbar.id[i]);
				}
			}
		}
		prev_which_scrollbars[i] = gui.which_scrollbars[i];
	}

	if (gui.menu_is_active != prev_menu_is_active)
	{
		if (gui.menu_is_active)
		{
			XtUnmanageChild(menuBar);
			XtUnmanageChild(vimForm);
			if (gui.which_scrollbars[SB_BOTTOM])
				XtUnmanageChild(scrollbarBox[SB_BOTTOM]);

			XtManageChild(menuBar);
			XtManageChild(vimForm);
			if (gui.which_scrollbars[SB_BOTTOM])
				XtManageChild(scrollbarBox[SB_BOTTOM]);
		}
		else
			XtUnmanageChild(menuBar);
		prev_menu_is_active = gui.menu_is_active;
	}

	gui_x11_use_resize_callback(textArea, TRUE);
	if (vimForm != (Widget)NULL && XtIsRealized(vimForm))
		gui_mch_set_winsize();
}


/*
 * Vertical scrollbar stuff:
 */
	void
gui_mch_update_scrollbars(worst_update, which_sb)
	int		worst_update;
	int		which_sb;		/* SB_LEFT or SB_RIGHT */
{
	WIN				*wp;
	GuiScrollbar	*sb;
	int				idx;
	Dimension		h;		/* Height of scrollbar (in pixels) */
	Dimension		y;		/* Coord of top of scrollbar (in pixels) */
	int				tmp;
	float			val = 0., size = 0.;

	if (worst_update >= SB_UPDATE_HEIGHT)
	{
		XawPanedSetRefigureMode(scrollbarBox[which_sb], False);
		gui_x11_use_resize_callback(textArea, FALSE);
	}

	/*
	 * This has to get cleared manually since Athena doesn't tell us when the
	 * draggin' stops.
	 */
	gui.dragged_sb = SB_NONE;

	for (wp = firstwin, idx = 0; wp; wp = wp->w_next, idx++)
	{
		sb = &wp->w_scrollbar;
		if (sb->update[which_sb] >= SB_UPDATE_VALUE)
		{
			val = (float)(sb->value - 1) / (float)sb->max;
			size = (float)sb->size / (float)sb->max;
		}
		if (sb->update[which_sb] == SB_UPDATE_CREATE)
		{
			sb->id[which_sb] = XtVaCreateManagedWidget("scrollBar",
					vim_scrollbarWidgetClass, scrollbarBox[which_sb],
					XtNborderWidth, 1,
					XtNdefaultDistance, 0,
					XtNallowResize, True,
					XtNpreferredPaneSize, True,
					XtNresizeToPreferred, True,
					XtNskipAdjust, True,
					XtNorientation, XtorientVertical,
					XtNshowGrip, False,
					XtNforeground, gui.scroll_fg_pixel,
					XtNbackground, gui.scroll_bg_pixel,
					NULL);
			XtAddCallback(sb->id[which_sb], XtNjumpProc,
				gui_athena_scroll_cb_jump, (XtPointer)wp);
			XtAddCallback(sb->id[which_sb], XtNscrollProc,
				gui_athena_scroll_cb_scroll, (XtPointer)wp);
		}
		if (sb->update[which_sb] >= SB_UPDATE_HEIGHT)
		{
			h = sb->height * gui.char_height
					+ sb->status_height * gui.char_height / 2;
			y = wp->w_winpos * gui.char_height + gui.border_offset;

			if (wp == firstwin)
			{
				/* Height of top scrollbar includes width of top border */
				h += gui.border_offset;
			}
			else
			{
				/*
				 * Height of other scrollbars includes half of status bar above
				 */
				tmp = wp->w_prev->w_status_height * (gui.char_height + 1) / 2;
				h += tmp;
				y -= tmp;
			}

			XtVaSetValues(sb->id[which_sb],
				XtNheight, h,
				XtNmin, h,
				XtNmax, h,
				XtNy, y,
				NULL);
			vim_XawScrollbarSetThumb(sb->id[which_sb], val, size, 1.0);
		}
		else if (sb->update[which_sb] == SB_UPDATE_VALUE)
		{
			vim_XawScrollbarSetThumb(sb->id[which_sb], val, size, 1.0);
		}
		sb->update[which_sb] = SB_UPDATE_NOTHING;
	}

	if (worst_update >= SB_UPDATE_HEIGHT)
	{
		if (worst_update >= SB_UPDATE_CREATE)
			gui_mch_reorder_scrollbars(which_sb);
		XawPanedSetRefigureMode(scrollbarBox[which_sb], True);
		gui_x11_use_resize_callback(textArea, TRUE);
	}
}

	void
gui_mch_reorder_scrollbars(which_sb)
	int		which_sb;
{
	WIN		*wp;

	if (which_sb == SB_LEFT)
		XtUnmanageChild(leftScrollbarFiller);
	else
		XtUnmanageChild(rightScrollbarFiller);

	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		if (wp->w_scrollbar.id[which_sb] != NULL)
			XtUnmanageChild(wp->w_scrollbar.id[which_sb]);

	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		if (wp->w_scrollbar.id[which_sb] != NULL)
			XtManageChild(wp->w_scrollbar.id[which_sb]);

	if (which_sb == SB_LEFT)
		XtManageChild(leftScrollbarFiller);
	else
		XtManageChild(rightScrollbarFiller);

}

	void
gui_mch_destroy_scrollbar(wp)
	WIN		*wp;
{
	if (gui.which_scrollbars[SB_LEFT])
		XtDestroyWidget(wp->w_scrollbar.id[SB_LEFT]);
	if (gui.which_scrollbars[SB_RIGHT])
		XtDestroyWidget(wp->w_scrollbar.id[SB_RIGHT]);
	gui.num_scrollbars--;
}


/*
 * Horizontal scrollbar stuff:
 */
	void
gui_mch_update_horiz_scrollbar(value, size, max)
	int		value;
	int		size;
	int		max;
{
	static int prev_value = -1, prev_size = -1, prev_max = -1;
	float val, shown, maxval;

	if (value == prev_value && size == prev_size && max == prev_max)
		return;

	prev_value = value;
	prev_size = size;
	prev_max = max;

	if (max == 1)			/* maximum is one more than maximal value */
	{
		val   = 0.0;
		shown = 1.0;
		maxval = 0.0;
	}
	else
	{
		val   = (float)value / (float)max;
		shown = (float)size  / (float)max;
		maxval = 1.0;
	}
	vim_XawScrollbarSetThumb(bottomScrollbar, val, shown, maxval);
}

	Window
gui_mch_get_wid()
{
	return( XtWindow(textArea) );
}

	static void
gui_athena_pullright_action(w, event, args, nargs)
	Widget		w;
	XEvent		*event;
	String		*args;
	Cardinal	*nargs;
{
	Widget		menuw;
	Dimension	width, height;
	char_u		*pullright_name;
	Widget		popup;

	if (event->type != MotionNotify)
		return;

	/* Get the active entry for the current menu */
	if ((menuw = XawSimpleMenuGetActiveEntry(w)) == (Widget)NULL)
		return;

	XtVaGetValues(w,
		XtNwidth,	&width,
		XtNheight,	&height,
		NULL);

	if (event->xmotion.x >= (int)width || event->xmotion.y >= (int)height)
		return;

	/* We do the pull-off when the pointer is in the rightmost 1/4th */
	if (event->xmotion.x < (int)(width * 3) / 4)
		return;

	pullright_name = strnsave((char_u *)XtName(menuw),
								strlen(XtName(menuw)) + strlen("-pullright"));
	strcat((char *)pullright_name, "-pullright");
	popup = XtNameToWidget(w, pullright_name);
	vim_free(pullright_name);

	if (popup == (Widget)NULL)
		return;

	XtVaSetValues(popup,
		XtNx, event->xmotion.x_root,
		XtNy, event->xmotion.y_root - 7,
		NULL);

	XtPopup(popup, XtGrabExclusive);
}
