/*	$OpenBSD: gui_motif.c,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved			by Bram Moolenaar
 *								GUI/Motif support by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/PanedW.h>
#include <Xm/CascadeB.h>
#include <Xm/ScrollBar.h>
#include <Xm/RowColumn.h>
#include <Xm/MenuShell.h>
#if (XmVersion >= 1002)
# include <Xm/RepType.h>
#endif

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/StringDefs.h>

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "ops.h"

extern Widget vimShell;

static Widget vimForm;
static Widget textArea;
static Widget scrollbarBox[3];		/* Left, right & bottom scrollbar boxes */
static Widget menuBar;

/*
 * Call-back routines.
 */

	static void
scroll_cb(w, client_data, call_data)
	Widget		w;
	XtPointer	client_data, call_data;
{
	char_u		bytes[4 + sizeof(long_u)];
	WIN			*wp;
	GuiScrollbar *sb;
	int			sb_num;

	if (((XmScrollBarCallbackStruct *)call_data)->reason == XmCR_DRAG)
		gui.dragged_sb = (XtParent(w) == scrollbarBox[SB_LEFT]) ? SB_LEFT
																: SB_RIGHT;
	else
		gui.dragged_sb = SB_NONE;
	gui.dragged_wp = (WIN *)client_data;
	sb_num = 0;
	for (wp = firstwin; wp != gui.dragged_wp && wp != NULL; wp = wp->w_next)
		sb_num++;
	
	bytes[0] = CSI;
	bytes[1] = KS_SCROLLBAR;
	bytes[2] = K_FILLER;
	bytes[3] = (char_u)sb_num;
	if (gui.dragged_wp == NULL)
		sb = &gui.cmdline_sb;
	else
		sb = &wp->w_scrollbar;
	sb->value = ((XmScrollBarCallbackStruct *)call_data)->value;
	add_long_to_buf((long_u)sb->value, bytes + 4);
	add_to_input_buf(bytes, 4 + sizeof(long_u));
}

	static void
horiz_scroll_cb(w, client_data, call_data)
	Widget		w;
	XtPointer	client_data, call_data;
{
	char_u		bytes[3 + sizeof(long_u)];

	if (((XmScrollBarCallbackStruct *)call_data)->reason == XmCR_DRAG)
		gui.dragged_sb = SB_BOTTOM;
	else
		gui.dragged_sb = SB_NONE;
	
	bytes[0] = CSI;
	bytes[1] = KS_HORIZ_SCROLLBAR;
	bytes[2] = K_FILLER;
	add_long_to_buf((long_u)((XmScrollBarCallbackStruct *)call_data)->value,
					bytes + 3);
	add_to_input_buf(bytes, 3 + sizeof(long_u));
}

/*
 * End of call-back routines
 */

/*
 * Create all the motif widgets necessary.
 */
	void
gui_mch_create_widgets()
{
	int			i;
	Dimension	n;

	/*
	 * Start out by adding the configured border width into the border offset
	 */
	gui.border_offset = gui.border_width;

	/*
	 * Install the tearOffModel resource converter.
	 */
#if (XmVersion >= 1002)
	XmRepTypeInstallTearOffModelConverter();
#endif

	XtInitializeWidgetClass(xmFormWidgetClass);
	XtInitializeWidgetClass(xmRowColumnWidgetClass);
	XtInitializeWidgetClass(xmRowColumnWidgetClass);
	XtInitializeWidgetClass(xmPrimitiveWidgetClass);

	vimForm = XtVaCreateManagedWidget("vimForm",
		xmFormWidgetClass, vimShell,
		XmNresizePolicy, XmRESIZE_GROW,
        XmNforeground, gui.menu_fg_pixel,
        XmNbackground, gui.menu_bg_pixel,
		NULL);

	menuBar = XtVaCreateManagedWidget("menuBar",
		xmRowColumnWidgetClass, vimForm,
		XmNresizeHeight, False,
#if (XmVersion >= 1002)
		XmNtearOffModel, XmTEAR_OFF_ENABLED,
#endif
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNrowColumnType, XmMENU_BAR,
		XmNheight, gui.menu_height,
        XmNforeground, gui.menu_fg_pixel,
        XmNbackground, gui.menu_bg_pixel,
		NULL);

	scrollbarBox[SB_LEFT] = XtVaCreateWidget("leftScrollBarBox",
		xmRowColumnWidgetClass, vimForm,
		XmNresizeWidth, False,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, menuBar,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNmarginWidth, 0,
		XmNmarginHeight, 0,
		XmNspacing, 0,
		XmNwidth, gui.scrollbar_width,
		XmNforeground, gui.scroll_fg_pixel,
		XmNbackground, gui.scroll_fg_pixel,
		NULL);

	scrollbarBox[SB_RIGHT] = XtVaCreateWidget("rightScrollBarBox",
		xmRowColumnWidgetClass, vimForm,
		XmNresizeWidth, False,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, menuBar,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNmarginWidth, 0,
		XmNmarginHeight, 0,
		XmNspacing, 0,
		XmNwidth, gui.scrollbar_width,
		XmNforeground, gui.scroll_fg_pixel,
		XmNbackground, gui.scroll_fg_pixel,
		NULL);

	scrollbarBox[SB_BOTTOM] = XtVaCreateWidget("bottomScrollBarBox",
		xmScrollBarWidgetClass, vimForm,
		XmNorientation, XmHORIZONTAL,
		XmNminimum, 0,
		XmNvalue, 0,
		XmNsliderSize, Columns,
		XmNmaximum, Columns,		/* Motif want one more than actual max */
		XmNresizeHeight, False,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNheight, gui.scrollbar_width,
		XmNshadowThickness, 1,
		XmNbackground, gui.scroll_fg_pixel,
		XmNtroughColor, gui.scroll_bg_pixel,
		NULL);
	XtAddCallback(scrollbarBox[SB_BOTTOM], XmNvalueChangedCallback,
		horiz_scroll_cb, (XtPointer)NULL);
	XtAddCallback(scrollbarBox[SB_BOTTOM], XmNdragCallback,
		horiz_scroll_cb, (XtPointer)NULL);

	textArea = XtVaCreateManagedWidget("textArea",
		xmPrimitiveWidgetClass, vimForm,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, menuBar,
		XmNbackground, gui.back_pixel,

		/* These take some control away from the user, but avoids making them
		 * add resources to get a decent looking setup. */
		XmNborderWidth, 0,
		XmNhighlightThickness, 0,
		XmNshadowThickness, 0,
		NULL);

	/*
	 * If there are highlight or shadow borders, add their widths to our
	 * border offset so we don't draw over them.
	 */
	XtVaGetValues(textArea, XmNhighlightThickness, &n, NULL);
	gui.border_offset += n;
	XtVaGetValues(textArea, XmNshadowThickness, &n, NULL);
	gui.border_offset += n;

	/* Create the command line scroll bars */
	for (i = 0; i < 2; i++)
	{
		gui.cmdline_sb.id[i] = XtVaCreateManagedWidget("cmdlineScrollBar",
			xmScrollBarWidgetClass, scrollbarBox[i],
			XmNshadowThickness, 1,
			XmNshowArrows, False,
			XmNbackground, gui.scroll_fg_pixel,
			XmNtroughColor, gui.scroll_fg_pixel,
			NULL);
		XtAddCallback(gui.cmdline_sb.id[i], XmNvalueChangedCallback,
			scroll_cb, (XtPointer)NULL);
		XtAddCallback(gui.cmdline_sb.id[i], XmNdragCallback,
			scroll_cb, (XtPointer)NULL);
	}
	gui.num_scrollbars = 1;

	/*
	 * Text area callbacks
	 */
	XtAddEventHandler(textArea, VisibilityChangeMask, FALSE,
		gui_x11_visibility_cb, (XtPointer)0);

	XtAddEventHandler(textArea, ExposureMask, FALSE, gui_x11_expose_cb,
		(XtPointer)0);

	XtAddEventHandler(textArea, StructureNotifyMask, FALSE,
		gui_x11_resize_window_cb, (XtPointer)0);

	XtAddEventHandler(textArea, FocusChangeMask, FALSE, gui_x11_focus_change_cb,
		(XtPointer)0);

	XtAddEventHandler(textArea, KeyPressMask, FALSE, gui_x11_key_hit_cb,
		(XtPointer)0);

	XtAddEventHandler(textArea, ButtonPressMask | ButtonReleaseMask |
		ButtonMotionMask, FALSE, gui_x11_mouse_cb, (XtPointer)0);
}

	int
gui_mch_get_winsize()
{
	Dimension	n;
	Dimension	base_width = 0, base_height = 0;

	if (gui.which_scrollbars[SB_LEFT])
	{
		XtVaGetValues(scrollbarBox[SB_LEFT], XmNwidth, &n, NULL);
		base_width += n;
	}
	if (gui.which_scrollbars[SB_RIGHT])
	{
		XtVaGetValues(scrollbarBox[SB_RIGHT], XmNwidth, &n, NULL);
		base_width += n;
	}
	if (gui.which_scrollbars[SB_BOTTOM])
	{
		XtVaGetValues(scrollbarBox[SB_BOTTOM], XmNheight, &n, NULL);
		base_height += n;
	}

	base_height += 2 * gui.border_offset;
	base_width  += 2 * gui.border_offset;

	if (gui.menu_is_active)
	{
		XtVaGetValues(menuBar, XmNheight, &n, NULL);
		base_height += n;
	}

	XtVaGetValues(vimShell, XmNheight, &n, NULL);
	gui.num_rows = (int)(n - base_height) / (int)gui.char_height;

	XtVaGetValues(vimShell, XmNwidth, &n, NULL);
	gui.num_cols = (int)(n - base_width) / (int)gui.char_width;

	Rows = gui.num_rows;
	Columns = gui.num_cols;
	gui_reset_scroll_region();

	return OK;
}

	void
gui_mch_set_winsize()
{
	Dimension	left_width, right_width, bottom_height, menu_height;
	Dimension	base_width = 0, base_height = 0;

	base_width  += 2 * gui.border_offset;
	base_height += 2 * gui.border_offset;

	if (gui.which_scrollbars[SB_LEFT])
	{
		XtVaGetValues(scrollbarBox[SB_LEFT], XmNwidth, &left_width, NULL);
		base_width += left_width;
	}
	if (gui.which_scrollbars[SB_RIGHT])
	{
		XtVaGetValues(scrollbarBox[SB_RIGHT], XmNwidth, &right_width, NULL);
		base_width += right_width;
	}
	if (gui.which_scrollbars[SB_BOTTOM])
	{
		XtVaGetValues(scrollbarBox[SB_BOTTOM], XmNheight, &bottom_height, NULL);
		base_height += bottom_height;
	}
	if (gui.menu_is_active)
	{
		XtVaGetValues(menuBar, XmNheight, &menu_height, NULL);
		base_height += menu_height;
	}

	XtVaSetValues(vimShell,
#ifdef XmNbaseWidth
		XmNbaseWidth, base_width,
		XmNbaseHeight, base_height,
#endif
		XmNwidthInc,  gui.char_width,
		XmNheightInc, gui.char_height,
		XmNminWidth,  base_width  + MIN_COLUMNS * gui.char_width,
		XmNminHeight, base_height + MIN_ROWS * gui.char_height,
		XmNwidth,	  base_width  + Columns * gui.char_width,
		XmNheight,	  base_height + Rows * gui.char_height,
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
#if (XmVersion >= 1002)
	Widget		widget;
	XmString label = XmStringCreate((char *)menu->name,
													  XmFONTLIST_DEFAULT_TAG);
#else
	XmString label = XmStringCreate((char *)menu->name,
													XmSTRING_DEFAULT_CHARSET);
#endif
	Widget shell;

	menu->id = XtVaCreateWidget("subMenu",
		xmCascadeButtonWidgetClass,
		(parent == NULL) ? menuBar : parent->submenu_id,
		XmNlabelString, label,
        XmNforeground, gui.menu_fg_pixel,
        XmNbackground, gui.menu_bg_pixel,
		NULL);
	/* XtFree((char *)label); makes Lesstif crash */

	/* if 'guic' contains 'g', make menu's contain grey items */
	if (vim_strchr(p_guioptions, GO_GREY) != NULL)
		XtManageChild(menu->id);

	shell = XtVaCreateWidget("subMenuShell",
		xmMenuShellWidgetClass, menu->id,
		XmNwidth, 1,
		XmNheight, 1,
        XmNforeground, gui.menu_fg_pixel,
        XmNbackground, gui.menu_bg_pixel,
		NULL);
	menu->submenu_id = XtVaCreateWidget("rowColumnMenu",
		xmRowColumnWidgetClass, shell,
		XmNrowColumnType, XmMENU_PULLDOWN,
#if (XmVersion >= 1002)
		XmNtearOffModel, XmTEAR_OFF_ENABLED,
#endif
		NULL);

#if (XmVersion >= 1002)
	/* Set the colors for the tear off widget */
	if ((widget = XmGetTearOffControl(menu->submenu_id)) != (Widget)NULL)
		XtVaSetValues(widget,
			XmNforeground, gui.menu_fg_pixel,
			XmNbackground, gui.menu_bg_pixel,
			NULL);
#endif
	
	XtVaSetValues(menu->id,
		XmNsubMenuId, menu->submenu_id,
		NULL);

	/*
	 * The "Help" menu is a special case, and should be placed at the far right
	 * hand side of the menu-bar.
	 */
	if (parent == NULL && STRCMP((char *)menu->name, "Help") == 0)
		XtVaSetValues(menuBar,
			XmNmenuHelpWidget, menu->id,
			NULL);

    if (parent == NULL)
	    XtVaSetValues(XtParent(menu->id),
            XmNforeground, gui.menu_fg_pixel,
            XmNbackground, gui.menu_bg_pixel,
            NULL);
}

	void
gui_mch_add_menu_item(menu, parent)
	GuiMenu	*menu;
	GuiMenu	*parent;
{
#if (XmVersion >= 1002)
	XmString label = XmStringCreate((char *)menu->name,
													  XmFONTLIST_DEFAULT_TAG);
#else
	XmString label = XmStringCreate((char *)menu->name,
													XmSTRING_DEFAULT_CHARSET);
#endif

	menu->submenu_id = (Widget)0;
	menu->id = XtVaCreateWidget("subMenu",
		xmPushButtonWidgetClass, parent->submenu_id,
		XmNlabelString, label,
        XmNforeground, gui.menu_fg_pixel,
        XmNbackground, gui.menu_bg_pixel,
		NULL);
	/* XtFree((char *)label); makes Lesstif crash */

	if (vim_strchr(p_guioptions, GO_GREY) != NULL)
		XtManageChild(menu->id);

	XtAddCallback(menu->id, XmNactivateCallback, gui_x11_menu_cb,
		(XtPointer)menu);
}

/*
 * Destroy the machine specific menu widget.
 */
	void
gui_mch_destroy_menu(menu)
	GuiMenu	*menu;
{
	if (menu->id != (Widget)0)
		XtDestroyWidget(menu->id);
	if (menu->submenu_id != (Widget)0)
		XtDestroyWidget(menu->submenu_id);
}

/*
 * Scrollbar stuff:
 */

	void
gui_mch_create_which_components()
{
	static int prev_which_scrollbars[3] = {FALSE, FALSE, FALSE};

	int		i;
	char	*attach = NULL, *widget = NULL;		/* NOT char_u */
	WIN		*wp;

	gui_x11_use_resize_callback(textArea, FALSE);

	for (i = 0; i < 3; i++)
	{
		switch (i)
		{
			case SB_LEFT:	attach = XmNleftAttachment;
							widget = XmNleftWidget;			break;
			case SB_RIGHT:	attach = XmNrightAttachment;
							widget = XmNrightWidget;		break;
			case SB_BOTTOM:	attach = XmNbottomAttachment;
							widget = XmNbottomWidget;		break;
		}
		if (gui.which_scrollbars[i])
		{
			XtManageChild(scrollbarBox[i]);
			XtVaSetValues(textArea,
				attach, XmATTACH_WIDGET,
				widget, scrollbarBox[i],
				NULL);
			if (i != SB_BOTTOM && gui.which_scrollbars[SB_BOTTOM])
				XtVaSetValues(scrollbarBox[SB_BOTTOM],
					attach, XmATTACH_WIDGET,
					widget, scrollbarBox[i],
					NULL);
		}
		else
		{
			XtUnmanageChild(scrollbarBox[i]);
			XtVaSetValues(textArea,
				attach, XmATTACH_FORM,
				NULL);
			if (i != SB_BOTTOM && gui.which_scrollbars[SB_BOTTOM])
				XtVaSetValues(scrollbarBox[SB_BOTTOM],
					attach, XmATTACH_FORM,
					NULL);
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
				else
				{
					/* Scrollbar box has just been deleted */
					for (wp = firstwin; wp != NULL; wp = wp->w_next)
						XtDestroyWidget(wp->w_scrollbar.id[i]);
				}
			}
			prev_which_scrollbars[i] = gui.which_scrollbars[i];
		}
	}
	if (gui.menu_is_active)
	{
		XtManageChild(menuBar);
		XtVaSetValues(textArea, XmNtopAttachment, XmATTACH_WIDGET,
						XmNtopWidget, menuBar, NULL);
		if (gui.which_scrollbars[SB_LEFT])
		{
			XtVaSetValues(scrollbarBox[SB_LEFT],
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, menuBar,
				NULL);
		}
		if (gui.which_scrollbars[SB_RIGHT])
		{
			XtVaSetValues(scrollbarBox[SB_RIGHT],
				XmNtopAttachment, XmATTACH_WIDGET,
				XmNtopWidget, menuBar,
				NULL);
		}
	}
    else
	{
		XtUnmanageChild(menuBar);
		XtVaSetValues(textArea, XmNtopAttachment, XmATTACH_FORM, NULL);
		if (gui.which_scrollbars[SB_LEFT])
		{
			XtVaSetValues(scrollbarBox[SB_LEFT],
				XmNtopAttachment, XmATTACH_FORM, NULL);
		}
		if (gui.which_scrollbars[SB_RIGHT])
		{
			XtVaSetValues(scrollbarBox[SB_RIGHT],
				XmNtopAttachment, XmATTACH_FORM, NULL);
  		}
  	}
	gui_x11_use_resize_callback(textArea, TRUE);
	if (vimForm != (Widget)NULL)
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
	int				val = 0, size = 0, max = 0;

	if (worst_update >= SB_UPDATE_HEIGHT)
	{
		gui_x11_use_resize_callback(textArea, FALSE);
		XtUnmanageChild(scrollbarBox[which_sb]);
	}

	for (wp = firstwin, idx = 0; wp; wp = wp->w_next, idx++)
	{
		sb = &wp->w_scrollbar;
		if (sb->update[which_sb] >= SB_UPDATE_VALUE)
		{
			val = sb->value;
			size = sb->size;
			max = sb->max + 1;		/* Motif has max one past the end */
		}
		if (sb->update[which_sb] == SB_UPDATE_CREATE)
		{
			sb->id[which_sb] = XtVaCreateManagedWidget("scrollBar",
				xmScrollBarWidgetClass, scrollbarBox[which_sb],
				XmNshadowThickness, 1,
#if (XmVersion >= 1002)		/* What do we do otherwise? */
				XmNpositionIndex, idx,
#endif
				XmNminimum, 1,
				XmNmaximum, max,
				XmNbackground, gui.scroll_fg_pixel,
				XmNtroughColor, gui.scroll_bg_pixel,
				NULL);
			XtAddCallback(sb->id[which_sb], XmNvalueChangedCallback,
				scroll_cb, (XtPointer)wp);
			XtAddCallback(sb->id[which_sb], XmNdragCallback,
				scroll_cb, (XtPointer)wp);
		}
		if (sb->update[which_sb] >= SB_UPDATE_HEIGHT)
		{
			h = sb->height * gui.char_height
					+ sb->status_height * gui.char_height / 2;
			y = wp->w_winpos * gui.char_height;

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
				y += gui.border_offset - tmp;
			}

			XtVaSetValues(sb->id[which_sb],
				XmNvalue, val,
				XmNsliderSize, size,
				XmNpageIncrement, (size > 2 ? size - 2 : 1),
				XmNmaximum, max,
				XmNheight, h,
				XmNy, y,
				NULL);
		}
		else if (sb->update[which_sb] == SB_UPDATE_VALUE)
		{
			XtVaSetValues(sb->id[which_sb],
				XmNvalue, val,
				XmNsliderSize, size,
				XmNpageIncrement, (size > 2 ? size - 2 : 1),
				XmNmaximum, max,
				NULL);
		}
		sb->update[which_sb] = SB_UPDATE_NOTHING;
	}

	/* Command line scrollbar */
	sb = &gui.cmdline_sb;
	max = sb->max + 1;			/* Motif has max one past the end */
	if (sb->update[which_sb] == SB_UPDATE_HEIGHT)
	{
		h = lastwin->w_status_height * (gui.char_height + 1) / 2;
		y = (Rows - sb->height) * gui.char_height - h;
		h += sb->height * gui.char_height;

		/* Height of cmdline scrollbar includes width of bottom border */
		h += gui.border_offset;

		XtVaSetValues(sb->id[which_sb],
			XmNvalue, sb->value,
			XmNsliderSize, sb->size,
			XmNmaximum, max,
			XmNheight, h,
			XmNy, y,
			NULL);
	}
	else if (sb->update[which_sb] == SB_UPDATE_VALUE)
	{
		XtVaSetValues(sb->id[which_sb],
			XmNvalue, sb->value,
			XmNsliderSize, sb->size,
			XmNmaximum, max,
			NULL);
	}
	sb->update[which_sb] = SB_UPDATE_NOTHING;

	if (worst_update >= SB_UPDATE_HEIGHT)
	{
		if (worst_update >= SB_UPDATE_CREATE)
			gui_mch_reorder_scrollbars(which_sb);
		XtManageChild(scrollbarBox[which_sb]);
		gui_x11_use_resize_callback(textArea, TRUE);
	}
}

	void
gui_mch_reorder_scrollbars(which_sb)
	int		which_sb;
{
	Widget *children;
	int		num_children;
	Widget	tmp;
	WIN		*wp, *wp2;
	int		i, j;

	XtVaGetValues(scrollbarBox[which_sb],
		XmNchildren, &children,
		XmNnumChildren, &num_children,
		NULL);
	
	/* Should be in same order as in the window list */
	wp = firstwin;
	for (i = 0; i < num_children; i++, wp = wp->w_next)
	{
		if (wp == NULL)
			break;		/* Shouldn't happen */
		if (wp->w_scrollbar.id[which_sb] != children[i])
		{
			/* It's in the wrong place, find what should go here */
			wp2 = wp->w_next;
			for (j = i + 1; j < num_children; j++, wp2 = wp2->w_next)
			{
				if (wp2 == NULL)
					break;		/* Shouldn't happen */
				if (wp->w_scrollbar.id[which_sb] == children[j])
					break;		/* Found it */
			}
			if (j >= num_children || wp2 == NULL)
				break;			/* Shouldn't happen */
			tmp = children[i];
			children[i] = children[j];
			children[j] = tmp;
		}
	}

	XtVaSetValues(scrollbarBox[which_sb],
		XmNchildren, children,
		NULL);
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

	if (value == prev_value && size == prev_size && max == prev_max)
		return;

	prev_value = value;
	prev_size = size;
	prev_max = max;

	XtVaSetValues(scrollbarBox[SB_BOTTOM],
		XmNvalue, value,
		XmNsliderSize, size,
		XmNpageIncrement, (size > 2 ? size - 2 : 1),
		XmNmaximum, max,
		NULL);
}

	Window
gui_mch_get_wid()
{
	return( XtWindow(textArea) );
}
