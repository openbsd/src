/* 
 * tkScale.c --
 *
 *	This module implements a scale widgets for the Tk toolkit.
 *	A scale displays a slider that can be adjusted to change a
 *	value;  it also displays numeric labels and a textual label,
 *	if desired.
 *	
 *	The modifications to use floating-point values are based on
 *	an implementation by Paul Mackerras.  The -variable option
 *	is due to Henning Schulzrinne.  All of these are used with
 *	permission.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkScale.c 1.80 96/03/21 13:11:55
 */

#include "tkPort.h"
#include "default.h"
#include "tkInt.h"
#include <math.h>

/*
 * A data structure of the following type is kept for each scale
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the scale.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with scale. */
    Tcl_Command widgetCmd;	/* Token for scale's widget command. */
    Tk_Uid orientUid;		/* Orientation for window ("vertical" or
				 * "horizontal"). */
    int vertical;		/* Non-zero means vertical orientation,
				 * zero means horizontal. */
    int width;			/* Desired narrow dimension of scale,
				 * in pixels. */
    int length;			/* Desired long dimension of scale,
				 * in pixels. */
    double value;		/* Current value of scale. */
    char *varName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, scale's value tracks
				 * the contents of this variable and
				 * vice versa. */
    double fromValue;		/* Value corresponding to left or top of
				 * scale. */
    double toValue;		/* Value corresponding to right or bottom
				 * of scale. */
    double tickInterval;	/* Distance between tick marks;  0 means
				 * don't display any tick marks. */
    double resolution;		/* If > 0, all values are rounded to an
				 * even multiple of this value. */
    int digits;			/* Number of significant digits to print
				 * in values.  0 means we get to choose the
				 * number based on resolution and/or the
				 * range of the scale. */
    char format[10];		/* Sprintf conversion specifier computed from
				 * digits and other information. */
    double bigIncrement;	/* Amount to use for large increments to
				 * scale value.  (0 means we pick a value). */
    char *command;		/* Command prefix to use when invoking Tcl
				 * commands because the scale value changed.
				 * NULL means don't invoke commands.
				 * Malloc'ed. */
    int repeatDelay;		/* How long to wait before auto-repeating
				 * on scrolling actions (in ms). */
    int repeatInterval;		/* Interval between autorepeats (in ms). */
    char *label;		/* Label to display above or to right of
				 * scale;  NULL means don't display a
				 * label.  Malloc'ed. */
    int labelLength;		/* Number of non-NULL chars. in label. */
    Tk_Uid state;		/* Normal or disabled.  Value cannot be
				 * changed when scale is disabled. */

    /*
     * Information used when displaying widget:
     */

    int borderWidth;		/* Width of 3-D border around window. */
    Tk_3DBorder bgBorder;	/* Used for drawing slider and other
				 * background areas. */
    Tk_3DBorder activeBorder;	/* For drawing the slider when active. */
    int sliderRelief;		/* Is slider to be drawn raised, sunken, etc. */
    XColor *troughColorPtr;	/* Color for drawing trough. */
    GC troughGC;		/* For drawing trough. */
    GC copyGC;			/* Used for copying from pixmap onto screen. */
    XFontStruct *fontPtr;	/* Information about text font, or NULL. */
    XColor *textColorPtr;	/* Color for drawing text. */
    GC textGC;			/* GC for drawing text in normal mode. */
    int relief;			/* Indicates whether window as a whole is
				 * raised, sunken, or flat. */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColorPtr;
				/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */
    int inset;			/* Total width of all borders, including
				 * traversal highlight and 3-D border.
				 * Indicates how much interior stuff must
				 * be offset from outside edges to leave
				 * room for borders. */
    int sliderLength;		/* Length of slider, measured in pixels along
				 * long dimension of scale. */
    int showValue;		/* Non-zero means to display the scale value
				 * below or to the left of the slider;  zero
				 * means don't display the value. */

    /*
     * Layout information for horizontal scales, assuming that window
     * gets the size it requested:
     */

    int horizLabelY;		/* Y-coord at which to draw label. */
    int horizValueY;		/* Y-coord at which to draw value text. */
    int horizTroughY;		/* Y-coord of top of slider trough. */
    int horizTickY;		/* Y-coord at which to draw tick text. */
    /*
     * Layout information for vertical scales, assuming that window
     * gets the size it requested:
     */

    int vertTickRightX;		/* X-location of right side of tick-marks. */
    int vertValueRightX;	/* X-location of right side of value string. */
    int vertTroughX;		/* X-location of scale's slider trough. */
    int vertLabelX;		/* X-location of origin of label. */

    /*
     * Miscellaneous information:
     */

    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Scale;

/*
 * Flag bits for scales:
 *
 * REDRAW_SLIDER -		1 means slider (and numerical readout) need
 *				to be redrawn.
 * REDRAW_OTHER -		1 means other stuff besides slider and value
 *				need to be redrawn.
 * REDRAW_ALL -			1 means the entire widget needs to be redrawn.
 * ACTIVE -			1 means the widget is active (the mouse is
 *				in its window).
 * INVOKE_COMMAND -		1 means the scale's command needs to be
 *				invoked during the next redisplay (the
 *				value of the scale has changed since the
 *				last time the command was invoked).
 * SETTING_VAR -		1 means that the associated variable is
 *				being set by us, so there's no need for
 *				ScaleVarProc to do anything.
 * NEVER_SET -			1 means that the scale's value has never
 *				been set before (so must invoke -command and
 *				set associated variable even if the value
 *				doesn't appear to have changed).
 * GOT_FOCUS -			1 means that the focus is currently in
 *				this widget.
 */

#define REDRAW_SLIDER		1
#define REDRAW_OTHER		2
#define REDRAW_ALL		3
#define ACTIVE			4
#define INVOKE_COMMAND		0x10
#define SETTING_VAR		0x20
#define NEVER_SET		0x40
#define GOT_FOCUS		0x80

/*
 * Symbolic values for the active parts of a slider.  These are
 * the values that may be returned by the ScaleElement procedure.
 */

#define OTHER		0
#define TROUGH1		1
#define SLIDER		2
#define TROUGH2		3

/*
 * Space to leave between scale area and text, and between text and
 * edge of window.
 */

#define SPACING 2

/*
 * How many characters of space to provide when formatting the
 * scale's value:
 */

#define PRINT_CHARS 150

/*
 * Information used for argv parsing.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_SCALE_ACTIVE_BG_COLOR, Tk_Offset(Scale, activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_SCALE_ACTIVE_BG_MONO, Tk_Offset(Scale, activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_SCALE_BG_COLOR, Tk_Offset(Scale, bgBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_SCALE_BG_MONO, Tk_Offset(Scale, bgBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_DOUBLE, "-bigincrement", "bigIncrement", "BigIncrement",
	DEF_SCALE_BIG_INCREMENT, Tk_Offset(Scale, bigIncrement), 0},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_SCALE_BORDER_WIDTH, Tk_Offset(Scale, borderWidth), 0},
    {TK_CONFIG_STRING, "-command", "command", "Command",
	DEF_SCALE_COMMAND, Tk_Offset(Scale, command), TK_CONFIG_NULL_OK},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_SCALE_CURSOR, Tk_Offset(Scale, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-digits", "digits", "Digits",
	DEF_SCALE_DIGITS, Tk_Offset(Scale, digits), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_SCALE_FONT, Tk_Offset(Scale, fontPtr),
	0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SCALE_FG_COLOR, Tk_Offset(Scale, textColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SCALE_FG_MONO, Tk_Offset(Scale, textColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_DOUBLE, "-from", "from", "From",
	DEF_SCALE_FROM, Tk_Offset(Scale, fromValue), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_SCALE_HIGHLIGHT_BG,
	Tk_Offset(Scale, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_SCALE_HIGHLIGHT, Tk_Offset(Scale, highlightColorPtr), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_SCALE_HIGHLIGHT_WIDTH, Tk_Offset(Scale, highlightWidth), 0},
    {TK_CONFIG_STRING, "-label", "label", "Label",
	DEF_SCALE_LABEL, Tk_Offset(Scale, label), TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-length", "length", "Length",
	DEF_SCALE_LENGTH, Tk_Offset(Scale, length), 0},
    {TK_CONFIG_UID, "-orient", "orient", "Orient",
	DEF_SCALE_ORIENT, Tk_Offset(Scale, orientUid), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_SCALE_RELIEF, Tk_Offset(Scale, relief), 0},
    {TK_CONFIG_INT, "-repeatdelay", "repeatDelay", "RepeatDelay",
	DEF_SCALE_REPEAT_DELAY, Tk_Offset(Scale, repeatDelay), 0},
    {TK_CONFIG_INT, "-repeatinterval", "repeatInterval", "RepeatInterval",
	DEF_SCALE_REPEAT_INTERVAL, Tk_Offset(Scale, repeatInterval), 0},
    {TK_CONFIG_DOUBLE, "-resolution", "resolution", "Resolution",
	DEF_SCALE_RESOLUTION, Tk_Offset(Scale, resolution), 0},
    {TK_CONFIG_BOOLEAN, "-showvalue", "showValue", "ShowValue",
	DEF_SCALE_SHOW_VALUE, Tk_Offset(Scale, showValue), 0},
    {TK_CONFIG_PIXELS, "-sliderlength", "sliderLength", "SliderLength",
	DEF_SCALE_SLIDER_LENGTH, Tk_Offset(Scale, sliderLength), 0},
    {TK_CONFIG_RELIEF, "-sliderrelief", "sliderRelief", "SliderRelief",
	DEF_SCALE_SLIDER_RELIEF, Tk_Offset(Scale, sliderRelief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_UID, "-state", "state", "State",
	DEF_SCALE_STATE, Tk_Offset(Scale, state), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_SCALE_TAKE_FOCUS, Tk_Offset(Scale, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_DOUBLE, "-tickinterval", "tickInterval", "TickInterval",
	DEF_SCALE_TICK_INTERVAL, Tk_Offset(Scale, tickInterval), 0},
    {TK_CONFIG_DOUBLE, "-to", "to", "To",
	DEF_SCALE_TO, Tk_Offset(Scale, toValue), 0},
    {TK_CONFIG_COLOR, "-troughcolor", "troughColor", "Background",
	DEF_SCALE_TROUGH_COLOR, Tk_Offset(Scale, troughColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-troughcolor", "troughColor", "Background",
	DEF_SCALE_TROUGH_MONO, Tk_Offset(Scale, troughColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_SCALE_VARIABLE, Tk_Offset(Scale, varName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-width", "width", "Width",
	DEF_SCALE_WIDTH, Tk_Offset(Scale, width), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ComputeFormat _ANSI_ARGS_((Scale *scalePtr));
static void		ComputeScaleGeometry _ANSI_ARGS_((Scale *scalePtr));
static int		ConfigureScale _ANSI_ARGS_((Tcl_Interp *interp,
			    Scale *scalePtr, int argc, char **argv,
			    int flags));
static void		DestroyScale _ANSI_ARGS_((char *memPtr));
static void		DisplayScale _ANSI_ARGS_((ClientData clientData));
static void		DisplayHorizontalScale _ANSI_ARGS_((Scale *scalePtr,
			    Drawable drawable, XRectangle *drawnAreaPtr));
static void		DisplayHorizontalValue _ANSI_ARGS_((Scale *scalePtr,
			    Drawable drawable, double value, int top));
static void		DisplayVerticalScale _ANSI_ARGS_((Scale *scalePtr,
			    Drawable drawable, XRectangle *drawnAreaPtr));
static void		DisplayVerticalValue _ANSI_ARGS_((Scale *scalePtr,
			    Drawable drawable, double value, int rightEdge));
static void		EventuallyRedrawScale _ANSI_ARGS_((Scale *scalePtr,
			    int what));
static double		PixelToValue _ANSI_ARGS_((Scale *scalePtr, int x,
			    int y));
static double		RoundToResolution _ANSI_ARGS_((Scale *scalePtr,
			    double value));
static void		ScaleCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		ScaleElement _ANSI_ARGS_((Scale *scalePtr, int x,
			    int y));
static void		ScaleEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static char *		ScaleVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		ScaleWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		SetScaleValue _ANSI_ARGS_((Scale *scalePtr,
			    double value, int setVar, int invokeCommand));
static int		ValueToPixel _ANSI_ARGS_((Scale *scalePtr, double value));

/*
 *--------------------------------------------------------------
 *
 * Tk_ScaleCmd --
 *
 *	This procedure is invoked to process the "scale" Tcl
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Tk_ScaleCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    register Scale *scalePtr;
    Tk_Window new;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *) NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize fields that won't be initialized by ConfigureScale,
     * or which ConfigureScale expects to have reasonable values
     * (e.g. resource pointers).
     */

    scalePtr = (Scale *) ckalloc(sizeof(Scale));
    scalePtr->tkwin = new;
    scalePtr->display = Tk_Display(new);
    scalePtr->interp = interp;
    scalePtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(scalePtr->tkwin), ScaleWidgetCmd,
	    (ClientData) scalePtr, ScaleCmdDeletedProc);
    scalePtr->orientUid = NULL;
    scalePtr->vertical = 0;
    scalePtr->width = 0;
    scalePtr->length = 0;
    scalePtr->value = 0;
    scalePtr->varName = NULL;
    scalePtr->fromValue = 0;
    scalePtr->toValue = 0;
    scalePtr->tickInterval = 0;
    scalePtr->resolution = 1;
    scalePtr->bigIncrement = 0.0;
    scalePtr->command = NULL;
    scalePtr->repeatDelay = 0;
    scalePtr->repeatInterval = 0;
    scalePtr->label = NULL;
    scalePtr->labelLength = 0;
    scalePtr->state = tkNormalUid;
    scalePtr->borderWidth = 0;
    scalePtr->bgBorder = NULL;
    scalePtr->activeBorder = NULL;
    scalePtr->sliderRelief = TK_RELIEF_RAISED;
    scalePtr->troughColorPtr = NULL;
    scalePtr->troughGC = None;
    scalePtr->copyGC = None;
    scalePtr->fontPtr = NULL;
    scalePtr->textColorPtr = NULL;
    scalePtr->textGC = None;
    scalePtr->relief = TK_RELIEF_FLAT;
    scalePtr->highlightWidth = 0;
    scalePtr->highlightBgColorPtr = NULL;
    scalePtr->highlightColorPtr = NULL;
    scalePtr->inset = 0;
    scalePtr->sliderLength = 0;
    scalePtr->showValue = 0;
    scalePtr->horizLabelY = 0;
    scalePtr->horizValueY = 0;
    scalePtr->horizTroughY = 0;
    scalePtr->horizTickY = 0;
    scalePtr->vertTickRightX = 0;
    scalePtr->vertValueRightX = 0;
    scalePtr->vertTroughX = 0;
    scalePtr->vertLabelX = 0;
    scalePtr->cursor = None;
    scalePtr->takeFocus = NULL;
    scalePtr->flags = NEVER_SET;

    Tk_SetClass(scalePtr->tkwin, "Scale");
    Tk_CreateEventHandler(scalePtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    ScaleEventProc, (ClientData) scalePtr);
    if (ConfigureScale(interp, scalePtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = Tk_PathName(scalePtr->tkwin);
    return TCL_OK;

    error:
    Tk_DestroyWindow(scalePtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
ScaleWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about scale
					 * widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Scale *scalePtr = (Scale *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) scalePtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, scalePtr->tkwin, configSpecs,
		(char *) scalePtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 3)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, scalePtr->tkwin, configSpecs,
		    (char *) scalePtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, scalePtr->tkwin, configSpecs,
		    (char *) scalePtr, argv[2], 0);
	} else {
	    result = ConfigureScale(interp, scalePtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "coords", length) == 0)
	    && (length >= 3)) {
	int x, y ;
	double value;

	if ((argc != 2) && (argc != 3)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " coords ?value?\"", (char *) NULL);
	    goto error;
	}
	if (argc == 3) {
	    if (Tcl_GetDouble(interp, argv[2], &value) != TCL_OK) {
		goto error;
	    }
	} else {
	    value = scalePtr->value;
	}
	if (scalePtr->vertical) {
	    x = scalePtr->vertTroughX + scalePtr->width/2
		    + scalePtr->borderWidth;
	    y = ValueToPixel(scalePtr, value);
	} else {
	    x = ValueToPixel(scalePtr, value);
	    y = scalePtr->horizTroughY + scalePtr->width/2
		    + scalePtr->borderWidth;
	}
	sprintf(interp->result, "%d %d", x, y);
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	double value;
	int x, y;

	if ((argc != 2) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get ?x y?\"", (char *) NULL);
	    goto error;
	}
	if (argc == 2) {
	    value = scalePtr->value;
	} else {
	    if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
		goto error;
	    }
	    value = PixelToValue(scalePtr, x, y);
	}
	sprintf(interp->result, scalePtr->format, value);
    } else if ((c == 'i') && (strncmp(argv[1], "identify", length) == 0)) {
	int x, y, thing;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " identify x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    goto error;
	}
	thing = ScaleElement(scalePtr, x,y);
	switch (thing) {
	    case TROUGH1:	interp->result = "trough1";	break;
	    case SLIDER:	interp->result = "slider";	break;
	    case TROUGH2:	interp->result = "trough2";	break;
	}
    } else if ((c == 's') && (strncmp(argv[1], "set", length) == 0)) {
	double value;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " set value\"", (char *) NULL);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[2], &value) != TCL_OK) {
	    goto error;
	}
	if (scalePtr->state != tkDisabledUid) {
	    SetScaleValue(scalePtr, value, 1, 1);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cget, configure, coords, get, identify, or set",
		(char *) NULL);
	goto error;
    }
    Tcl_Release((ClientData) scalePtr);
    return result;

    error:
    Tcl_Release((ClientData) scalePtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyScale --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a button at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the scale is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyScale(memPtr)
    char *memPtr;	/* Info about scale widget. */
{
    register Scale *scalePtr = (Scale *) memPtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (scalePtr->varName != NULL) {
	Tcl_UntraceVar(scalePtr->interp, scalePtr->varName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ScaleVarProc, (ClientData) scalePtr);
    }
    if (scalePtr->troughGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->troughGC);
    }
    if (scalePtr->copyGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->copyGC);
    }
    if (scalePtr->textGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->textGC);
    }
    Tk_FreeOptions(configSpecs, (char *) scalePtr, scalePtr->display, 0);
    ckfree((char *) scalePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureScale --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a scale widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for scalePtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureScale(interp, scalePtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Scale *scalePtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    XGCValues gcValues;
    GC newGC;
    size_t length;

    /*
     * Eliminate any existing trace on a variable monitored by the scale.
     */

    if (scalePtr->varName != NULL) {
	Tcl_UntraceVar(interp, scalePtr->varName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ScaleVarProc, (ClientData) scalePtr);
    }

    if (Tk_ConfigureWidget(interp, scalePtr->tkwin, configSpecs,
	    argc, argv, (char *) scalePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * If the scale is tied to the value of a variable, then set up
     * a trace on the variable's value and set the scale's value from
     * the value of the variable, if it exists.
     */

    if (scalePtr->varName != NULL) {
	char *stringValue, *end;
	double value;

	stringValue = Tcl_GetVar(interp, scalePtr->varName, TCL_GLOBAL_ONLY);
	if (stringValue != NULL) {
	    value = strtod(stringValue, &end);
	    if ((end != stringValue) && (*end == 0)) {
		scalePtr->value = RoundToResolution(scalePtr, value);
	    }
	}
	Tcl_TraceVar(interp, scalePtr->varName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ScaleVarProc, (ClientData) scalePtr);
    }

    /*
     * Several options need special processing, such as parsing the
     * orientation and creating GCs.
     */

    length = strlen(scalePtr->orientUid);
    if (strncmp(scalePtr->orientUid, "vertical", length) == 0) {
	scalePtr->vertical = 1;
    } else if (strncmp(scalePtr->orientUid, "horizontal", length) == 0) {
	scalePtr->vertical = 0;
    } else {
	Tcl_AppendResult(interp, "bad orientation \"", scalePtr->orientUid,
		"\": must be vertical or horizontal", (char *) NULL);
	return TCL_ERROR;
    }

    scalePtr->fromValue = RoundToResolution(scalePtr, scalePtr->fromValue);
    scalePtr->toValue = RoundToResolution(scalePtr, scalePtr->toValue);
    scalePtr->tickInterval = RoundToResolution(scalePtr,
	    scalePtr->tickInterval);

    /*
     * Make sure that the tick interval has the right sign so that
     * addition moves from fromValue to toValue.
     */

    if ((scalePtr->tickInterval < 0)
	    ^ ((scalePtr->toValue - scalePtr->fromValue) <  0)) {
	scalePtr->tickInterval = -scalePtr->tickInterval;
    }

    /*
     * Set the scale value to itself;  all this does is to make sure
     * that the scale's value is within the new acceptable range for
     * the scale and reflect the value in the associated variable,
     * if any.
     */

    ComputeFormat(scalePtr);
    SetScaleValue(scalePtr, scalePtr->value, 1, 1);

    if (scalePtr->label != NULL) {
	scalePtr->labelLength = strlen(scalePtr->label);
    } else {
	scalePtr->labelLength = 0;
    }

    if ((scalePtr->state != tkNormalUid)
	    && (scalePtr->state != tkDisabledUid)
	    && (scalePtr->state != tkActiveUid)) {
	Tcl_AppendResult(interp, "bad state value \"", scalePtr->state,
		"\": must be normal, active, or disabled", (char *) NULL);
	scalePtr->state = tkNormalUid;
	return TCL_ERROR;
    }

    Tk_SetBackgroundFromBorder(scalePtr->tkwin, scalePtr->bgBorder);

    gcValues.foreground = scalePtr->troughColorPtr->pixel;
    newGC = Tk_GetGC(scalePtr->tkwin, GCForeground, &gcValues);
    if (scalePtr->troughGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->troughGC);
    }
    scalePtr->troughGC = newGC;
    if (scalePtr->copyGC == None) {
	gcValues.graphics_exposures = False;
	scalePtr->copyGC = Tk_GetGC(scalePtr->tkwin, GCGraphicsExposures,
	    &gcValues);
    }
    if (scalePtr->highlightWidth < 0) {
	scalePtr->highlightWidth = 0;
    }
    gcValues.font = scalePtr->fontPtr->fid;
    gcValues.foreground = scalePtr->textColorPtr->pixel;
    newGC = Tk_GetGC(scalePtr->tkwin, GCForeground|GCFont, &gcValues);
    if (scalePtr->textGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->textGC);
    }
    scalePtr->textGC = newGC;

    scalePtr->inset = scalePtr->highlightWidth + scalePtr->borderWidth;

    /*
     * Recompute display-related information, and let the geometry
     * manager know how much space is needed now.
     */

    ComputeScaleGeometry(scalePtr);

    EventuallyRedrawScale(scalePtr, REDRAW_ALL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeFormat --
 *
 *	This procedure is invoked to recompute the "format" field
 *	of a scale's widget record, which determines how the value
 *	of the scale is converted to a string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The format field of scalePtr is modified.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeFormat(scalePtr)
    Scale *scalePtr;			/* Information about scale widget. */
{
    double maxValue, x;
    int mostSigDigit, numDigits, leastSigDigit, afterDecimal;
    int eDigits, fDigits;

    /*
     * Compute the displacement from the decimal of the most significant
     * digit required for any number in the scale's range.
     */

    maxValue = fabs(scalePtr->fromValue);
    x = fabs(scalePtr->toValue);
    if (x > maxValue) {
	maxValue = x;
    }
    if (maxValue == 0) {
	maxValue = 1;
    }
    mostSigDigit = floor(log10(maxValue));

    /*
     * If the number of significant digits wasn't specified explicitly,
     * compute it. It's the difference between the most significant
     * digit needed to represent any number on the scale and the
     * most significant digit of the smallest difference between
     * numbers on the scale.  In other words, display enough digits so
     * that at least one digit will be different between any two adjacent
     * positions of the scale.
     */

    numDigits = scalePtr->digits;
    if (numDigits <= 0) {
	if  (scalePtr->resolution > 0) {
	    /*
	     * A resolution was specified for the scale, so just use it.
	     */

	    leastSigDigit = floor(log10(scalePtr->resolution));
	} else {
	    /*
	     * No resolution was specified, so compute the difference
	     * in value between adjacent pixels and use it for the least
	     * significant digit.
	     */

	    x = fabs(scalePtr->fromValue - scalePtr->toValue);
	    if (scalePtr->length > 0) {
		x /= scalePtr->length;
	    }
	    if (x > 0){
		leastSigDigit = floor(log10(x));
	    } else {
		leastSigDigit = 0;
	    }
	}
	numDigits = mostSigDigit - leastSigDigit + 1;
	if (numDigits < 1) {
	    numDigits = 1;
	}
    }

    /*
     * Compute the number of characters required using "e" format and
     * "f" format, and then choose whichever one takes fewer characters.
     */

    eDigits = numDigits + 4;
    if (numDigits > 1) {
	eDigits++;			/* Decimal point. */
    }
    afterDecimal = numDigits - mostSigDigit - 1;
    if (afterDecimal < 0) {
	afterDecimal = 0;
    }
    fDigits = (mostSigDigit >= 0) ? mostSigDigit + afterDecimal : afterDecimal;
    if (afterDecimal > 0) {
	fDigits++;			/* Decimal point. */
    }
    if (mostSigDigit < 0) {
	fDigits++;			/* Zero to left of decimal point. */
    }
    if (fDigits <= eDigits) {
	sprintf(scalePtr->format, "%%.%df", afterDecimal);
    } else {
	sprintf(scalePtr->format, "%%.%de", numDigits-1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeScaleGeometry --
 *
 *	This procedure is called to compute various geometrical
 *	information for a scale, such as where various things get
 *	displayed.  It's called when the window is reconfigured.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display-related numbers get changed in *scalePtr.  The
 *	geometry manager gets told about the window's preferred size.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeScaleGeometry(scalePtr)
    register Scale *scalePtr;		/* Information about widget. */
{
    XCharStruct bbox;
    char valueString[PRINT_CHARS];
    int dummy, lineHeight, valuePixels, x, y, extraSpace;

    /*
     * Horizontal scales are simpler than vertical ones because
     * all sizes are the same (the height of a line of text);
     * handle them first and then quit.
     */

    if (!scalePtr->vertical) {
	lineHeight = scalePtr->fontPtr->ascent + scalePtr->fontPtr->descent;
	y = scalePtr->inset;
	extraSpace = 0;
	if (scalePtr->labelLength != 0) {
	    scalePtr->horizLabelY = y + SPACING;
	    y += lineHeight + SPACING;
	    extraSpace = SPACING;
	}
	if (scalePtr->showValue) {
	    scalePtr->horizValueY = y + SPACING;
	    y += lineHeight + SPACING;
	    extraSpace = SPACING;
	} else {
	    scalePtr->horizValueY = y;
	}
	y += extraSpace;
	scalePtr->horizTroughY = y;
	y += scalePtr->width + 2*scalePtr->borderWidth;
	if (scalePtr->tickInterval != 0) {
	    scalePtr->horizTickY = y + SPACING;
	    y += lineHeight + 2*SPACING;
	}
	Tk_GeometryRequest(scalePtr->tkwin,
		scalePtr->length + 2*scalePtr->inset, y + scalePtr->inset);
	Tk_SetInternalBorder(scalePtr->tkwin, scalePtr->inset);
	return;
    }

    /*
     * Vertical scale:  compute the amount of space needed to display
     * the scales value by formatting strings for the two end points;
     * use whichever length is longer.
     */

    sprintf(valueString, scalePtr->format, scalePtr->fromValue);
    XTextExtents(scalePtr->fontPtr, valueString, (int) strlen(valueString),
	    &dummy, &dummy, &dummy, &bbox);
    valuePixels = bbox.rbearing - bbox.lbearing;
    sprintf(valueString, scalePtr->format, scalePtr->toValue);
    XTextExtents(scalePtr->fontPtr, valueString, (int) strlen(valueString),
	    &dummy, &dummy, &dummy, &bbox);
    if (valuePixels < bbox.rbearing - bbox.lbearing) {
	valuePixels = bbox.rbearing - bbox.lbearing;
    }

    /*
     * Assign x-locations to the elements of the scale, working from
     * left to right.
     */

    x = scalePtr->inset;
    if ((scalePtr->tickInterval != 0) && (scalePtr->showValue)) {
	scalePtr->vertTickRightX = x + SPACING + valuePixels;
	scalePtr->vertValueRightX = scalePtr->vertTickRightX + valuePixels
		+ scalePtr->fontPtr->ascent/2;
	x = scalePtr->vertValueRightX + SPACING;
    } else if (scalePtr->tickInterval != 0) {
	scalePtr->vertTickRightX = x + SPACING + valuePixels;
	scalePtr->vertValueRightX = scalePtr->vertTickRightX;
	x = scalePtr->vertTickRightX + SPACING;
    } else if (scalePtr->showValue) {
	scalePtr->vertTickRightX = x;
	scalePtr->vertValueRightX = x + SPACING + valuePixels;
	x = scalePtr->vertValueRightX + SPACING;
    } else {
	scalePtr->vertTickRightX = x;
	scalePtr->vertValueRightX = x;
    }
    scalePtr->vertTroughX = x;
    x += 2*scalePtr->borderWidth + scalePtr->width;
    if (scalePtr->labelLength == 0) {
	scalePtr->vertLabelX = 0;
    } else {
	XTextExtents(scalePtr->fontPtr, scalePtr->label,
		scalePtr->labelLength, &dummy, &dummy, &dummy, &bbox);
	scalePtr->vertLabelX = x + scalePtr->fontPtr->ascent/2 - bbox.lbearing;
	x = scalePtr->vertLabelX + bbox.rbearing
		+ scalePtr->fontPtr->ascent/2;
    }
    Tk_GeometryRequest(scalePtr->tkwin, x + scalePtr->inset,
	    scalePtr->length + 2*scalePtr->inset);
    Tk_SetInternalBorder(scalePtr->tkwin, scalePtr->inset);
}

/*
 *--------------------------------------------------------------
 *
 * DisplayVerticalScale --
 *
 *	This procedure redraws the contents of a vertical scale
 *	window.  It is invoked as a do-when-idle handler, so it only
 *	runs when there's nothing else for the application to do.
 *
 * Results:
 *	There is no return value.  If only a part of the scale needs
 *	to be redrawn, then drawnAreaPtr is modified to reflect the
 *	area that was actually modified.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
DisplayVerticalScale(scalePtr, drawable, drawnAreaPtr)
    Scale *scalePtr;			/* Widget record for scale. */
    Drawable drawable;			/* Where to display scale (window
					 * or pixmap). */
    XRectangle *drawnAreaPtr;		/* Initally contains area of window;
					 * if only a part of the scale is
					 * redrawn, gets modified to reflect
					 * the part of the window that was
					 * redrawn. */
{
    Tk_Window tkwin = scalePtr->tkwin;
    int x, y, width, height, shadowWidth;
    double tickValue;
    Tk_3DBorder sliderBorder;

    /*
     * Display the information from left to right across the window.
     */

    if (!(scalePtr->flags & REDRAW_OTHER)) {
	drawnAreaPtr->x = scalePtr->vertTickRightX;
	drawnAreaPtr->y = scalePtr->inset;
	drawnAreaPtr->width = scalePtr->vertTroughX + scalePtr->width
		+ 2*scalePtr->borderWidth - scalePtr->vertTickRightX;
	drawnAreaPtr->height -= 2*scalePtr->inset;
    }
    Tk_Fill3DRectangle(tkwin, drawable, scalePtr->bgBorder,
	    drawnAreaPtr->x, drawnAreaPtr->y, drawnAreaPtr->width,
	    drawnAreaPtr->height, 0, TK_RELIEF_FLAT);
    if (scalePtr->flags & REDRAW_OTHER) {
	/*
	 * Display the tick marks.
	 */

	if (scalePtr->tickInterval != 0) {
	    for (tickValue = scalePtr->fromValue; ;
		    tickValue += scalePtr->tickInterval) {
		/*
		 * The RoundToResolution call gets rid of accumulated
		 * round-off errors, if any.
		 */

		tickValue = RoundToResolution(scalePtr, tickValue);
		if (scalePtr->toValue >= scalePtr->fromValue) {
		    if (tickValue > scalePtr->toValue) {
			break;
		    }
		} else {
		    if (tickValue < scalePtr->toValue) {
			break;
		    }
		}
		DisplayVerticalValue(scalePtr, drawable, tickValue,
			scalePtr->vertTickRightX);
	    }
	}
    }

    /*
     * Display the value, if it is desired.
     */

    if (scalePtr->showValue) {
	DisplayVerticalValue(scalePtr, drawable, scalePtr->value,
		scalePtr->vertValueRightX);
    }

    /*
     * Display the trough and the slider.
     */

    Tk_Draw3DRectangle(tkwin, drawable,
	    scalePtr->bgBorder, scalePtr->vertTroughX, scalePtr->inset,
	    scalePtr->width + 2*scalePtr->borderWidth,
	    Tk_Height(tkwin) - 2*scalePtr->inset, scalePtr->borderWidth,
	    TK_RELIEF_SUNKEN);
    XFillRectangle(scalePtr->display, drawable, scalePtr->troughGC,
	    scalePtr->vertTroughX + scalePtr->borderWidth,
	    scalePtr->inset + scalePtr->borderWidth,
	    (unsigned) scalePtr->width,
	    (unsigned) (Tk_Height(tkwin) - 2*scalePtr->inset
		- 2*scalePtr->borderWidth));
    if (scalePtr->state == tkActiveUid) {
	sliderBorder = scalePtr->activeBorder;
    } else {
	sliderBorder = scalePtr->bgBorder;
    }
    width = scalePtr->width;
    height = scalePtr->sliderLength/2;
    x = scalePtr->vertTroughX + scalePtr->borderWidth;
    y = ValueToPixel(scalePtr, scalePtr->value) - height;
    shadowWidth = scalePtr->borderWidth/2;
    if (shadowWidth == 0) {
	shadowWidth = 1;
    }
    Tk_Draw3DRectangle(tkwin, drawable, sliderBorder, x, y, width,
	    2*height, shadowWidth, scalePtr->sliderRelief);
    x += shadowWidth;
    y += shadowWidth;
    width -= 2*shadowWidth;
    height -= shadowWidth;
    Tk_Fill3DRectangle(tkwin, drawable, sliderBorder, x, y, width,
	    height, shadowWidth, scalePtr->sliderRelief);
    Tk_Fill3DRectangle(tkwin, drawable, sliderBorder, x, y+height,
	    width, height, shadowWidth, scalePtr->sliderRelief);

    /*
     * Draw the label to the right of the scale.
     */

    if ((scalePtr->flags & REDRAW_OTHER) && (scalePtr->labelLength != 0)) {
	XDrawString(scalePtr->display, drawable,
	    scalePtr->textGC, scalePtr->vertLabelX,
	    scalePtr->inset + (3*scalePtr->fontPtr->ascent)/2,
	    scalePtr->label, scalePtr->labelLength);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayVerticalValue --
 *
 *	This procedure is called to display values (scale readings)
 *	for vertically-oriented scales.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The numerical value corresponding to value is displayed with
 *	its right edge at "rightEdge", and at a vertical position in
 *	the scale that corresponds to "value".
 *
 *----------------------------------------------------------------------
 */

static void
DisplayVerticalValue(scalePtr, drawable, value, rightEdge)
    register Scale *scalePtr;	/* Information about widget in which to
				 * display value. */
    Drawable drawable;		/* Pixmap or window in which to draw
				 * the value. */
    double value;		/* Y-coordinate of number to display,
				 * specified in application coords, not
				 * in pixels (we'll compute pixels). */
    int rightEdge;		/* X-coordinate of right edge of text,
				 * specified in pixels. */
{
    register Tk_Window tkwin = scalePtr->tkwin;
    int y, dummy, length;
    char valueString[PRINT_CHARS];
    XCharStruct bbox;

    y = ValueToPixel(scalePtr, value) + scalePtr->fontPtr->ascent/2;
    sprintf(valueString, scalePtr->format, value);
    length = strlen(valueString);
    XTextExtents(scalePtr->fontPtr, valueString, length,
	    &dummy, &dummy, &dummy, &bbox);

    /*
     * Adjust the y-coordinate if necessary to keep the text entirely
     * inside the window.
     */

    if ((y - bbox.ascent) < (scalePtr->inset + SPACING)) {
	y = scalePtr->inset + SPACING + bbox.ascent;
    }
    if ((y + bbox.descent) > (Tk_Height(tkwin) - scalePtr->inset - SPACING)) {
	y = Tk_Height(tkwin) - scalePtr->inset - SPACING - bbox.descent;
    }
    XDrawString(scalePtr->display, drawable, scalePtr->textGC,
	    rightEdge - bbox.rbearing, y, valueString, length);
}

/*
 *--------------------------------------------------------------
 *
 * DisplayHorizontalScale --
 *
 *	This procedure redraws the contents of a horizontal scale
 *	window.  It is invoked as a do-when-idle handler, so it only
 *	runs when there's nothing else for the application to do.
 *
 * Results:
 *	There is no return value.  If only a part of the scale needs
 *	to be redrawn, then drawnAreaPtr is modified to reflect the
 *	area that was actually modified.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
DisplayHorizontalScale(scalePtr, drawable, drawnAreaPtr)
    Scale *scalePtr;			/* Widget record for scale. */
    Drawable drawable;			/* Where to display scale (window
					 * or pixmap). */
    XRectangle *drawnAreaPtr;		/* Initally contains area of window;
					 * if only a part of the scale is
					 * redrawn, gets modified to reflect
					 * the part of the window that was
					 * redrawn. */
{
    register Tk_Window tkwin = scalePtr->tkwin;
    int x, y, width, height, shadowWidth;
    double tickValue;
    Tk_3DBorder sliderBorder;

    /*
     * Display the information from bottom to top across the window.
     */

    if (!(scalePtr->flags & REDRAW_OTHER)) {
	drawnAreaPtr->x = scalePtr->inset;
	drawnAreaPtr->y = scalePtr->horizValueY;
	drawnAreaPtr->width -= 2*scalePtr->inset;
	drawnAreaPtr->height = scalePtr->horizTroughY + scalePtr->width
		+ 2*scalePtr->borderWidth - scalePtr->horizValueY;
    }
    Tk_Fill3DRectangle(tkwin, drawable, scalePtr->bgBorder,
	    drawnAreaPtr->x, drawnAreaPtr->y, drawnAreaPtr->width,
	    drawnAreaPtr->height, 0, TK_RELIEF_FLAT);
    if (scalePtr->flags & REDRAW_OTHER) {
	/*
	 * Display the tick marks.
	 */

	if (scalePtr->tickInterval != 0) {
	    for (tickValue = scalePtr->fromValue; ;
		    tickValue += scalePtr->tickInterval) {
		/*
		 * The RoundToResolution call gets rid of accumulated
		 * round-off errors, if any.
		 */

		tickValue = RoundToResolution(scalePtr, tickValue);
		if (scalePtr->toValue >= scalePtr->fromValue) {
		    if (tickValue > scalePtr->toValue) {
			break;
		    }
		} else {
		    if (tickValue < scalePtr->toValue) {
			break;
		    }
		}
		DisplayHorizontalValue(scalePtr, drawable, tickValue,
			scalePtr->horizTickY);
	    }
	}
    }

    /*
     * Display the value, if it is desired.
     */

    if (scalePtr->showValue) {
	DisplayHorizontalValue(scalePtr, drawable, scalePtr->value,
		scalePtr->horizValueY);
    }

    /*
     * Display the trough and the slider.
     */

    y = scalePtr->horizTroughY;
    Tk_Draw3DRectangle(tkwin, drawable,
	    scalePtr->bgBorder, scalePtr->inset, y,
	    Tk_Width(tkwin) - 2*scalePtr->inset,
	    scalePtr->width + 2*scalePtr->borderWidth,
	    scalePtr->borderWidth, TK_RELIEF_SUNKEN);
    XFillRectangle(scalePtr->display, drawable, scalePtr->troughGC,
	    scalePtr->inset + scalePtr->borderWidth,
	    y + scalePtr->borderWidth,
	    (unsigned) (Tk_Width(tkwin) - 2*scalePtr->inset
		- 2*scalePtr->borderWidth),
	    (unsigned) scalePtr->width);
    if (scalePtr->state == tkActiveUid) {
	sliderBorder = scalePtr->activeBorder;
    } else {
	sliderBorder = scalePtr->bgBorder;
    }
    width = scalePtr->sliderLength/2;
    height = scalePtr->width;
    x = ValueToPixel(scalePtr, scalePtr->value) - width;
    y += scalePtr->borderWidth;
    shadowWidth = scalePtr->borderWidth/2;
    if (shadowWidth == 0) {
	shadowWidth = 1;
    }
    Tk_Draw3DRectangle(tkwin, drawable, sliderBorder,
	    x, y, 2*width, height, shadowWidth, scalePtr->sliderRelief);
    x += shadowWidth;
    y += shadowWidth;
    width -= shadowWidth;
    height -= 2*shadowWidth;
    Tk_Fill3DRectangle(tkwin, drawable, sliderBorder, x, y, width, height,
	    shadowWidth, scalePtr->sliderRelief);
    Tk_Fill3DRectangle(tkwin, drawable, sliderBorder, x+width, y,
	    width, height, shadowWidth, scalePtr->sliderRelief);

    /*
     * Draw the label at the top of the scale.
     */

    if ((scalePtr->flags & REDRAW_OTHER) && (scalePtr->labelLength != 0)) {
	XDrawString(scalePtr->display, drawable,
	    scalePtr->textGC, scalePtr->inset + scalePtr->fontPtr->ascent/2,
	    scalePtr->horizLabelY + scalePtr->fontPtr->ascent,
	    scalePtr->label, scalePtr->labelLength);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayHorizontalValue --
 *
 *	This procedure is called to display values (scale readings)
 *	for horizontally-oriented scales.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The numerical value corresponding to value is displayed with
 *	its bottom edge at "bottom", and at a horizontal position in
 *	the scale that corresponds to "value".
 *
 *----------------------------------------------------------------------
 */

static void
DisplayHorizontalValue(scalePtr, drawable, value, top)
    register Scale *scalePtr;	/* Information about widget in which to
				 * display value. */
    Drawable drawable;		/* Pixmap or window in which to draw
				 * the value. */
    double value;		/* X-coordinate of number to display,
				 * specified in application coords, not
				 * in pixels (we'll compute pixels). */
    int top;			/* Y-coordinate of top edge of text,
				 * specified in pixels. */
{
    register Tk_Window tkwin = scalePtr->tkwin;
    int x, y, dummy, length;
    char valueString[PRINT_CHARS];
    XCharStruct bbox;

    x = ValueToPixel(scalePtr, value);
    y = top + scalePtr->fontPtr->ascent;
    sprintf(valueString, scalePtr->format, value);
    length = strlen(valueString);
    XTextExtents(scalePtr->fontPtr, valueString, length,
	    &dummy, &dummy, &dummy, &bbox);

    /*
     * Adjust the x-coordinate if necessary to keep the text entirely
     * inside the window.
     */

    x -= (bbox.rbearing - bbox.lbearing)/2;
    if ((x + bbox.lbearing) < (scalePtr->inset + SPACING)) {
	x = scalePtr->inset + SPACING - bbox.lbearing;
    }
    if ((x + bbox.rbearing) > (Tk_Width(tkwin) - scalePtr->inset)) {
	x = Tk_Width(tkwin) - scalePtr->inset - SPACING - bbox.rbearing;
    }
    XDrawString(scalePtr->display, drawable, scalePtr->textGC, x, y,
	    valueString, length);
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayScale --
 *
 *	This procedure is invoked as an idle handler to redisplay
 *	the contents of a scale widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The scale gets redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayScale(clientData)
    ClientData clientData;	/* Widget record for scale. */
{
    Scale *scalePtr = (Scale *) clientData;
    Tk_Window tkwin = scalePtr->tkwin;
    Tcl_Interp *interp = scalePtr->interp;
    Pixmap pixmap;
    int result;
    char string[PRINT_CHARS];
    XRectangle drawnArea;

    if ((scalePtr->tkwin == NULL) || !Tk_IsMapped(scalePtr->tkwin)) {
	goto done;
    }

    /*
     * Invoke the scale's command if needed.
     */

    Tcl_Preserve((ClientData) scalePtr);
    Tcl_Preserve((ClientData) interp);
    if ((scalePtr->flags & INVOKE_COMMAND) && (scalePtr->command != NULL)) {
	sprintf(string, scalePtr->format, scalePtr->value);
	result = Tcl_VarEval(interp, scalePtr->command,	" ", string,
                             (char *) NULL);
	if (result != TCL_OK) {
	    Tcl_AddErrorInfo(interp, "\n    (command executed by scale)");
	    Tcl_BackgroundError(interp);
	}
    }
    Tcl_Release((ClientData) interp);
    scalePtr->flags &= ~INVOKE_COMMAND;
    if (scalePtr->tkwin == NULL) {
	Tcl_Release((ClientData) scalePtr);
	return;
    }
    Tcl_Release((ClientData) scalePtr);

    /*
     * In order to avoid screen flashes, this procedure redraws
     * the scale in a pixmap, then copies the pixmap to the
     * screen in a single operation.  This means that there's no
     * point in time where the on-sreen image has been cleared.
     */

    pixmap = Tk_GetPixmap(scalePtr->display, Tk_WindowId(tkwin),
	    Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));
    drawnArea.x = 0;
    drawnArea.y = 0;
    drawnArea.width = Tk_Width(tkwin);
    drawnArea.height = Tk_Height(tkwin);

    /*
     * Much of the redisplay is done totally differently for
     * horizontal and vertical scales.  Handle the part that's
     * different.
     */

    if (scalePtr->vertical) {
	DisplayVerticalScale(scalePtr, pixmap, &drawnArea);
    } else {
	DisplayHorizontalScale(scalePtr, pixmap, &drawnArea);
    }

    /*
     * Now handle the part of redisplay that is the same for
     * horizontal and vertical scales:  border and traversal
     * highlight.
     */

    if (scalePtr->flags & REDRAW_OTHER) {
	if (scalePtr->relief != TK_RELIEF_FLAT) {
	    Tk_Draw3DRectangle(tkwin, pixmap, scalePtr->bgBorder,
		    scalePtr->highlightWidth, scalePtr->highlightWidth,
		    Tk_Width(tkwin) - 2*scalePtr->highlightWidth,
		    Tk_Height(tkwin) - 2*scalePtr->highlightWidth,
		    scalePtr->borderWidth, scalePtr->relief);
	}
	if (scalePtr->highlightWidth != 0) {
	    GC gc;
    
	    if (scalePtr->flags & GOT_FOCUS) {
		gc = Tk_GCForColor(scalePtr->highlightColorPtr, pixmap);
	    } else {
		gc = Tk_GCForColor(scalePtr->highlightBgColorPtr, pixmap);
	    }
	    Tk_DrawFocusHighlight(tkwin, gc, scalePtr->highlightWidth, pixmap);
	}
    }

    /*
     * Copy the information from the off-screen pixmap onto the screen,
     * then delete the pixmap.
     */

    XCopyArea(scalePtr->display, pixmap, Tk_WindowId(tkwin),
	    scalePtr->copyGC, drawnArea.x, drawnArea.y, drawnArea.width,
	    drawnArea.height, drawnArea.x, drawnArea.y);
    Tk_FreePixmap(scalePtr->display, pixmap);

    done:
    scalePtr->flags &= ~REDRAW_ALL;
}

/*
 *----------------------------------------------------------------------
 *
 * ScaleElement --
 *
 *	Determine which part of a scale widget lies under a given
 *	point.
 *
 * Results:
 *	The return value is either TROUGH1, SLIDER, TROUGH2, or
 *	OTHER, depending on which of the scale's active elements
 *	(if any) is under the point at (x,y).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ScaleElement(scalePtr, x, y)
    Scale *scalePtr;		/* Widget record for scale. */
    int x, y;			/* Coordinates within scalePtr's window. */
{
    int sliderFirst;

    if (scalePtr->vertical) {
	if ((x < scalePtr->vertTroughX)
		|| (x >= (scalePtr->vertTroughX + 2*scalePtr->borderWidth +
		scalePtr->width))) {
	    return OTHER;
	}
	if ((y < scalePtr->inset)
		|| (y >= (Tk_Height(scalePtr->tkwin) - scalePtr->inset))) {
	    return OTHER;
	}
	sliderFirst = ValueToPixel(scalePtr, scalePtr->value)
		- scalePtr->sliderLength/2;
	if (y < sliderFirst) {
	    return TROUGH1;
	}
	if (y < (sliderFirst+scalePtr->sliderLength)) {
	    return SLIDER;
	}
	return TROUGH2;
    }

    if ((y < scalePtr->horizTroughY)
	    || (y >= (scalePtr->horizTroughY + 2*scalePtr->borderWidth +
	    scalePtr->width))) {
	return OTHER;
    }
    if ((x < scalePtr->inset)
	    || (x >= (Tk_Width(scalePtr->tkwin) - scalePtr->inset))) {
	return OTHER;
    }
    sliderFirst = ValueToPixel(scalePtr, scalePtr->value)
	    - scalePtr->sliderLength/2;
    if (x < sliderFirst) {
	return TROUGH1;
    }
    if (x < (sliderFirst+scalePtr->sliderLength)) {
	return SLIDER;
    }
    return TROUGH2;
}

/*
 *----------------------------------------------------------------------
 *
 * PixelToValue --
 *
 *	Given a pixel within a scale window, return the scale
 *	reading corresponding to that pixel.
 *
 * Results:
 *	A double-precision scale reading.  If the value is outside
 *	the legal range for the scale then it's rounded to the nearest
 *	end of the scale.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static double
PixelToValue(scalePtr, x, y)
    register Scale *scalePtr;		/* Information about widget. */
    int x, y;				/* Coordinates of point within
					 * window. */
{
    double value, pixelRange;

    if (scalePtr->vertical) {
	pixelRange = Tk_Height(scalePtr->tkwin) - scalePtr->sliderLength
		- 2*scalePtr->inset - 2*scalePtr->borderWidth;
	value = y;
    } else {
	pixelRange = Tk_Width(scalePtr->tkwin) - scalePtr->sliderLength
		- 2*scalePtr->inset - 2*scalePtr->borderWidth;
	value = x;
    }

    if (pixelRange <= 0) {
	/*
	 * Not enough room for the slider to actually slide:  just return
	 * the scale's current value.
	 */

	return scalePtr->value;
    }
    value -= scalePtr->sliderLength/2 + scalePtr->inset
		+ scalePtr->borderWidth;
    value /= pixelRange;
    if (value < 0) {
	value = 0;
    }
    if (value > 1) {
	value = 1;
    }
    value = scalePtr->fromValue +
		value * (scalePtr->toValue - scalePtr->fromValue);
    return RoundToResolution(scalePtr, value);
}

/*
 *----------------------------------------------------------------------
 *
 * ValueToPixel --
 *
 *	Given a reading of the scale, return the x-coordinate or
 *	y-coordinate corresponding to that reading, depending on
 *	whether the scale is vertical or horizontal, respectively.
 *
 * Results:
 *	An integer value giving the pixel location corresponding
 *	to reading.  The value is restricted to lie within the
 *	defined range for the scale.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ValueToPixel(scalePtr, value)
    register Scale *scalePtr;		/* Information about widget. */
    double value;			/* Reading of the widget. */
{
    int y, pixelRange;
    double valueRange;

    valueRange = scalePtr->toValue - scalePtr->fromValue;
    pixelRange = (scalePtr->vertical ? Tk_Height(scalePtr->tkwin)
	    : Tk_Width(scalePtr->tkwin)) - scalePtr->sliderLength
	    - 2*scalePtr->inset - 2*scalePtr->borderWidth;
    if (valueRange == 0) {
	y = 0;
    } else {
	y = (int) ((value - scalePtr->fromValue) * pixelRange
		  / valueRange + 0.5);
	if (y < 0) {
	    y = 0;
	} else if (y > pixelRange) {
	    y = pixelRange;
	}
    }
    y += scalePtr->sliderLength/2 + scalePtr->inset + scalePtr->borderWidth;
    return y;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on scales.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
ScaleEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Scale *scalePtr = (Scale *) clientData;

    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	EventuallyRedrawScale(scalePtr, REDRAW_ALL);
    } else if (eventPtr->type == DestroyNotify) {
	if (scalePtr->tkwin != NULL) {
	    scalePtr->tkwin = NULL;
	    Tcl_DeleteCommand(scalePtr->interp,
		    Tcl_GetCommandName(scalePtr->interp, scalePtr->widgetCmd));
	}
	if (scalePtr->flags & REDRAW_ALL) {
	    Tcl_CancelIdleCall(DisplayScale, (ClientData) scalePtr);
	}
	Tcl_EventuallyFree((ClientData) scalePtr, DestroyScale);
    } else if (eventPtr->type == ConfigureNotify) {
	ComputeScaleGeometry(scalePtr);
	EventuallyRedrawScale(scalePtr, REDRAW_ALL);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    scalePtr->flags |= GOT_FOCUS;
	    if (scalePtr->highlightWidth > 0) {
		EventuallyRedrawScale(scalePtr, REDRAW_ALL);
	    }
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    scalePtr->flags &= ~GOT_FOCUS;
	    if (scalePtr->highlightWidth > 0) {
		EventuallyRedrawScale(scalePtr, REDRAW_ALL);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ScaleCmdDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
ScaleCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Scale *scalePtr = (Scale *) clientData;
    Tk_Window tkwin = scalePtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	scalePtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * SetScaleValue --
 *
 *	This procedure changes the value of a scale and invokes
 *	a Tcl command to reflect the current position of a scale
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A Tcl command is invoked, and an additional error-processing
 *	command may also be invoked.  The scale's slider is redrawn.
 *
 *--------------------------------------------------------------
 */

static void
SetScaleValue(scalePtr, value, setVar, invokeCommand)
    register Scale *scalePtr;	/* Info about widget. */
    double value;		/* New value for scale.  Gets adjusted
				 * if it's off the scale. */
    int setVar;			/* Non-zero means reflect new value through
				 * to associated variable, if any. */
    int invokeCommand;		/* Non-zero means invoked -command option
				 * to notify of new value, 0 means don't. */
{
    char string[PRINT_CHARS];

    value = RoundToResolution(scalePtr, value);
    if ((value < scalePtr->fromValue)
	    ^ (scalePtr->toValue < scalePtr->fromValue)) {
	value = scalePtr->fromValue;
    }
    if ((value > scalePtr->toValue)
	    ^ (scalePtr->toValue < scalePtr->fromValue)) {
	value = scalePtr->toValue;
    }
    if (scalePtr->flags & NEVER_SET) {
	scalePtr->flags &= ~NEVER_SET;
    } else if (scalePtr->value == value) {
	return;
    }
    scalePtr->value = value;
    if (invokeCommand) {
	scalePtr->flags |= INVOKE_COMMAND;
    }
    EventuallyRedrawScale(scalePtr, REDRAW_SLIDER);

    if (setVar && (scalePtr->varName != NULL)) {
	sprintf(string, scalePtr->format, scalePtr->value);
	scalePtr->flags |= SETTING_VAR;
	Tcl_SetVar(scalePtr->interp, scalePtr->varName, string,
	       TCL_GLOBAL_ONLY);
	scalePtr->flags &= ~SETTING_VAR;
    }
}

/*
 *--------------------------------------------------------------
 *
 * EventuallyRedrawScale --
 *
 *	Arrange for part or all of a scale widget to redrawn at
 *	the next convenient time in the future.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If "what" is REDRAW_SLIDER then just the slider and the
 *	value readout will be redrawn;  if "what" is REDRAW_ALL
 *	then the entire widget will be redrawn.
 *
 *--------------------------------------------------------------
 */

static void
EventuallyRedrawScale(scalePtr, what)
    register Scale *scalePtr;	/* Information about widget. */
    int what;			/* What to redraw:  REDRAW_SLIDER
				 * or REDRAW_ALL. */
{
    if ((what == 0) || (scalePtr->tkwin == NULL)
	    || !Tk_IsMapped(scalePtr->tkwin)) {
	return;
    }
    if ((scalePtr->flags & REDRAW_ALL) == 0) {
	Tcl_DoWhenIdle(DisplayScale, (ClientData) scalePtr);
    }
    scalePtr->flags |= what;
}

/*
 *--------------------------------------------------------------
 *
 * RoundToResolution --
 *
 *	Round a given floating-point value to the nearest multiple
 *	of the scale's resolution.
 *
 * Results:
 *	The return value is the rounded result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static double
RoundToResolution(scalePtr, value)
    Scale *scalePtr;		/* Information about scale widget. */
    double value;		/* Value to round. */
{
    double rem, new;

    if (scalePtr->resolution <= 0) {
	return value;
    }
    new = scalePtr->resolution * floor(value/scalePtr->resolution);
    rem = value - new;
    if (rem < 0) {
	if (rem <= -scalePtr->resolution/2) {
	    new -= scalePtr->resolution;
	}
    } else {
	if (rem >= scalePtr->resolution/2) {
	    new += scalePtr->resolution;
	}
    }
    return new;
}

/*
 *----------------------------------------------------------------------
 *
 * ScaleVarProc --
 *
 *	This procedure is invoked by Tcl whenever someone modifies a
 *	variable associated with a scale widget.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The value displayed in the scale will change to match the
 *	variable's new value.  If the variable has a bogus value then
 *	it is reset to the value of the scale.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static char *
ScaleVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register Scale *scalePtr = (Scale *) clientData;
    char *stringValue, *end, *result;
    double value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar(interp, scalePtr->varName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ScaleVarProc, clientData);
	    scalePtr->flags |= NEVER_SET;
	    SetScaleValue(scalePtr, scalePtr->value, 1, 0);
	}
	return (char *) NULL;
    }

    /*
     * If we came here because we updated the variable (in SetScaleValue),
     * then ignore the trace.  Otherwise update the scale with the value
     * of the variable.
     */

    if (scalePtr->flags & SETTING_VAR) {
	return (char *) NULL;
    }
    result = NULL;
    stringValue = Tcl_GetVar(interp, scalePtr->varName, TCL_GLOBAL_ONLY);
    if (stringValue != NULL) {
	value = strtod(stringValue, &end);
	if ((end == stringValue) || (*end != 0)) {
	    result = "can't assign non-numeric value to scale variable";
	} else {
	    scalePtr->value = RoundToResolution(scalePtr, value);
	}

	/*
	 * This code is a bit tricky because it sets the scale's value before
	 * calling SetScaleValue.  This way, SetScaleValue won't bother to
	 * set the variable again or to invoke the -command.  However, it
	 * also won't redisplay the scale, so we have to ask for that
	 * explicitly.
	 */

	SetScaleValue(scalePtr, scalePtr->value, 1, 0);
	EventuallyRedrawScale(scalePtr, REDRAW_SLIDER);
    }

    return result;
}
