/* 
 * tkButton.c --
 *
 *	This module implements a collection of button-like
 *	widgets for the Tk toolkit.  The widgets implemented
 *	include labels, buttons, check buttons, and radio
 *	buttons.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkButton.c 1.128 96/03/01 17:34:49
 */

#include "default.h"
#include "tkPort.h"
#include "tkInt.h"

/*
 * A data structure of the following type is kept for each
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the button.  NULL
				 * means that the window has been destroyed. */
    Display *display;		/* Display containing widget.  Needed to
				 * free up resources after tkwin is gone. */
    Tcl_Interp *interp;		/* Interpreter associated with button. */
    Tcl_Command widgetCmd;	/* Token for button's widget command. */
    int type;			/* Type of widget:  restricts operations
				 * that may be performed on widget.  See
				 * below for possible values. */

    /*
     * Information about what's in the button.
     */

    char *text;			/* Text to display in button (malloc'ed)
				 * or NULL. */
    int numChars;		/* # of characters in text. */
    int underline;		/* Index of character to underline.  < 0 means
				 * don't underline anything. */
    char *textVarName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, button displays the contents
				 * of this variable. */
    Pixmap bitmap;		/* Bitmap to display or None.  If not None
				 * then text and textVar are ignored. */
    char *imageString;		/* Name of image to display (malloc'ed), or
				 * NULL.  If non-NULL, bitmap, text, and
				 * textVarName are ignored. */
    Tk_Image image;		/* Image to display in window, or NULL if
				 * none. */
    char *selectImageString;	/* Name of image to display when selected
				 * (malloc'ed), or NULL. */
    Tk_Image selectImage;	/* Image to display in window when selected,
				 * or NULL if none.  Ignored if image is
				 * NULL. */

    /*
     * Information used when displaying widget:
     */

    Tk_Uid state;		/* State of button for display purposes:
				 * normal, active, or disabled. */
    Tk_3DBorder normalBorder;	/* Structure used to draw 3-D
				 * border and background when window
				 * isn't active.  NULL means no such
				 * border exists. */
    Tk_3DBorder activeBorder;	/* Structure used to draw 3-D
				 * border and background when window
				 * is active.  NULL means no such
				 * border exists. */
    int borderWidth;		/* Width of border. */
    int relief;			/* 3-d effect: TK_RELIEF_RAISED, etc. */
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
    XFontStruct *fontPtr;	/* Information about text font, or NULL. */
    XColor *normalFg;		/* Foreground color in normal mode. */
    XColor *activeFg;		/* Foreground color in active mode.  NULL
				 * means use normalFg instead. */
    XColor *disabledFg;		/* Foreground color when disabled.  NULL
				 * means use normalFg with a 50% stipple
				 * instead. */
    GC normalTextGC;		/* GC for drawing text in normal mode.  Also
				 * used to copy from off-screen pixmap onto
				 * screen. */
    GC activeTextGC;		/* GC for drawing text in active mode (NULL
				 * means use normalTextGC). */
    Pixmap gray;		/* Pixmap for displaying disabled text if
				 * disabledFg is NULL. */
    GC disabledGC;		/* Used to produce disabled effect.  If
				 * disabledFg isn't NULL, this GC is used to
				 * draw button text or icon.  Otherwise
				 * text or icon is drawn with normalGC and
				 * this GC is used to stipple background
				 * across it.  For labels this is None. */
    GC copyGC;			/* Used for copying information from an
				 * off-screen pixmap to the screen. */
    char *widthString;		/* Value of -width option.  Malloc'ed. */
    char *heightString;		/* Value of -height option.  Malloc'ed. */
    int width, height;		/* If > 0, these specify dimensions to request
				 * for window, in characters for text and in
				 * pixels for bitmaps.  In this case the actual
				 * size of the text string or bitmap is
				 * ignored in computing desired window size. */
    int wrapLength;		/* Line length (in pixels) at which to wrap
				 * onto next line.  <= 0 means don't wrap
				 * except at newlines. */
    int padX, padY;		/* Extra space around text (pixels to leave
				 * on each side).  Ignored for bitmaps and
				 * images. */
    Tk_Anchor anchor;		/* Where text/bitmap should be displayed
				 * inside button region. */
    Tk_Justify justify;		/* Justification to use for multi-line text. */
    int indicatorOn;		/* True means draw indicator, false means
				 * don't draw it. */
    Tk_3DBorder selectBorder;	/* For drawing indicator background, or perhaps
				 * widget background, when selected. */
    int textWidth;		/* Width needed to display text as requested,
				 * in pixels. */
    int textHeight;		/* Height needed to display text as requested,
				 * in pixels. */
    int indicatorSpace;		/* Horizontal space (in pixels) allocated for
				 * display of indicator. */
    int indicatorDiameter;	/* Diameter of indicator, in pixels. */

    /*
     * For check and radio buttons, the fields below are used
     * to manage the variable indicating the button's state.
     */

    char *selVarName;		/* Name of variable used to control selected
				 * state of button.  Malloc'ed (if
				 * not NULL). */
    char *onValue;		/* Value to store in variable when
				 * this button is selected.  Malloc'ed (if
				 * not NULL). */
    char *offValue;		/* Value to store in variable when this
				 * button isn't selected.  Malloc'ed
				 * (if not NULL).  Valid only for check
				 * buttons. */

    /*
     * Miscellaneous information:
     */

    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *command;		/* Command to execute when button is
				 * invoked; valid for buttons only.
				 * If not NULL, it's malloc-ed. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Button;

/*
 * Possible "type" values for buttons.  These are the kinds of
 * widgets supported by this file.  The ordering of the type
 * numbers is significant:  greater means more features and is
 * used in the code.
 */

#define TYPE_LABEL		0
#define TYPE_BUTTON		1
#define TYPE_CHECK_BUTTON	2
#define TYPE_RADIO_BUTTON	3

/*
 * Class names for buttons, indexed by one of the type values above.
 */

static char *classNames[] = {"Label", "Button", "Checkbutton", "Radiobutton"};

/*
 * Flag bits for buttons:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * SELECTED:			Non-zero means this button is selected,
 *				so special highlight should be drawn.
 * GOT_FOCUS:			Non-zero means this button currently
 *				has the input focus.
 */

#define REDRAW_PENDING		1
#define SELECTED		2
#define GOT_FOCUS		4

/*
 * Mask values used to selectively enable entries in the
 * configuration specs:
 */

#define LABEL_MASK		TK_CONFIG_USER_BIT
#define BUTTON_MASK		TK_CONFIG_USER_BIT << 1
#define CHECK_BUTTON_MASK	TK_CONFIG_USER_BIT << 2
#define RADIO_BUTTON_MASK	TK_CONFIG_USER_BIT << 3
#define ALL_MASK		(LABEL_MASK | BUTTON_MASK \
	| CHECK_BUTTON_MASK | RADIO_BUTTON_MASK)

static int configFlags[] = {LABEL_MASK, BUTTON_MASK,
	CHECK_BUTTON_MASK, RADIO_BUTTON_MASK};

/*
 * Information used for parsing configuration specs:
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_BUTTON_ACTIVE_BG_COLOR, Tk_Offset(Button, activeBorder),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK
	|TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_BUTTON_ACTIVE_BG_MONO, Tk_Offset(Button, activeBorder),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK
	|TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_BUTTON_ACTIVE_FG_COLOR, Tk_Offset(Button, activeFg), 
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK
	|TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_BUTTON_ACTIVE_FG_MONO, Tk_Offset(Button, activeFg), 
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK
	|TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_BUTTON_ANCHOR, Tk_Offset(Button, anchor), ALL_MASK},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_BUTTON_BG_COLOR, Tk_Offset(Button, normalBorder),
	ALL_MASK | TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_BUTTON_BG_MONO, Tk_Offset(Button, normalBorder),
	ALL_MASK | TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {TK_CONFIG_BITMAP, "-bitmap", "bitmap", "Bitmap",
	DEF_BUTTON_BITMAP, Tk_Offset(Button, bitmap),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BUTTON_BORDER_WIDTH, Tk_Offset(Button, borderWidth), ALL_MASK},
    {TK_CONFIG_STRING, "-command", "command", "Command",
	DEF_BUTTON_COMMAND, Tk_Offset(Button, command),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_BUTTON_CURSOR, Tk_Offset(Button, cursor),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_BUTTON_DISABLED_FG_COLOR,
	Tk_Offset(Button, disabledFg), BUTTON_MASK|CHECK_BUTTON_MASK
	|RADIO_BUTTON_MASK|TK_CONFIG_COLOR_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_BUTTON_DISABLED_FG_MONO,
	Tk_Offset(Button, disabledFg), BUTTON_MASK|CHECK_BUTTON_MASK
	|RADIO_BUTTON_MASK|TK_CONFIG_MONO_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_BUTTON_FONT, Tk_Offset(Button, fontPtr),
	ALL_MASK},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_FG, Tk_Offset(Button, normalFg), ALL_MASK},
    {TK_CONFIG_STRING, "-height", "height", "Height",
	DEF_BUTTON_HEIGHT, Tk_Offset(Button, heightString), ALL_MASK},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_BUTTON_HIGHLIGHT_BG,
	Tk_Offset(Button, highlightBgColorPtr), ALL_MASK},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_BUTTON_HIGHLIGHT, Tk_Offset(Button, highlightColorPtr),
	ALL_MASK},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_LABEL_HIGHLIGHT_WIDTH, Tk_Offset(Button, highlightWidth),
	LABEL_MASK},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_BUTTON_HIGHLIGHT_WIDTH, Tk_Offset(Button, highlightWidth),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-image", "image", "Image",
	DEF_BUTTON_IMAGE, Tk_Offset(Button, imageString),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-indicatoron", "indicatorOn", "IndicatorOn",
	DEF_BUTTON_INDICATOR, Tk_Offset(Button, indicatorOn),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_BUTTON_JUSTIFY, Tk_Offset(Button, justify), ALL_MASK},
    {TK_CONFIG_STRING, "-offvalue", "offValue", "Value",
	DEF_BUTTON_OFF_VALUE, Tk_Offset(Button, offValue),
	CHECK_BUTTON_MASK},
    {TK_CONFIG_STRING, "-onvalue", "onValue", "Value",
	DEF_BUTTON_ON_VALUE, Tk_Offset(Button, onValue),
	CHECK_BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-padx", "padX", "Pad",
	DEF_BUTTON_PADX, Tk_Offset(Button, padX), BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-padx", "padX", "Pad",
	DEF_LABCHKRAD_PADX, Tk_Offset(Button, padX),
	LABEL_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-pady", "padY", "Pad",
	DEF_BUTTON_PADY, Tk_Offset(Button, padY), BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-pady", "padY", "Pad",
	DEF_LABCHKRAD_PADY, Tk_Offset(Button, padY),
	LABEL_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_BUTTON_RELIEF, Tk_Offset(Button, relief), BUTTON_MASK},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_LABCHKRAD_RELIEF, Tk_Offset(Button, relief),
	LABEL_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_BORDER, "-selectcolor", "selectColor", "Background",
	DEF_BUTTON_SELECT_COLOR, Tk_Offset(Button, selectBorder),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_COLOR_ONLY
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BORDER, "-selectcolor", "selectColor", "Background",
	DEF_BUTTON_SELECT_MONO, Tk_Offset(Button, selectBorder),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_MONO_ONLY
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-selectimage", "selectImage", "SelectImage",
	DEF_BUTTON_SELECT_IMAGE, Tk_Offset(Button, selectImageString),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_UID, "-state", "state", "State",
	DEF_BUTTON_STATE, Tk_Offset(Button, state),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_LABEL_TAKE_FOCUS, Tk_Offset(Button, takeFocus),
	LABEL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_BUTTON_TAKE_FOCUS, Tk_Offset(Button, takeFocus),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-text", "text", "Text",
	DEF_BUTTON_TEXT, Tk_Offset(Button, text), ALL_MASK},
    {TK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_BUTTON_TEXT_VARIABLE, Tk_Offset(Button, textVarName),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-underline", "underline", "Underline",
	DEF_BUTTON_UNDERLINE, Tk_Offset(Button, underline), ALL_MASK},
    {TK_CONFIG_STRING, "-value", "value", "Value",
	DEF_BUTTON_VALUE, Tk_Offset(Button, onValue),
	RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_RADIOBUTTON_VARIABLE, Tk_Offset(Button, selVarName),
	RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_CHECKBUTTON_VARIABLE, Tk_Offset(Button, selVarName),
	CHECK_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-width", "width", "Width",
	DEF_BUTTON_WIDTH, Tk_Offset(Button, widthString), ALL_MASK},
    {TK_CONFIG_PIXELS, "-wraplength", "wrapLength", "WrapLength",
	DEF_BUTTON_WRAP_LENGTH, Tk_Offset(Button, wrapLength), ALL_MASK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * String to print out in error messages, identifying options for
 * widget commands for different types of labels or buttons:
 */

static char *optionStrings[] = {
    "cget or configure",
    "cget, configure, flash, or invoke",
    "cget, configure, deselect, flash, invoke, select, or toggle",
    "cget, configure, deselect, flash, invoke, or select"
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ButtonCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		ButtonCreate _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv,
			    int type));
static void		ButtonEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		ButtonImageProc _ANSI_ARGS_((ClientData clientData,
			    int x, int y, int width, int height,
			    int imgWidth, int imgHeight));
static void		ButtonSelectImageProc _ANSI_ARGS_((
			    ClientData clientData, int x, int y, int width,
			    int height, int imgWidth, int imgHeight));
static char *		ButtonTextVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static char *		ButtonVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		ButtonWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		ComputeButtonGeometry _ANSI_ARGS_((Button *butPtr));
static int		ConfigureButton _ANSI_ARGS_((Tcl_Interp *interp,
			    Button *butPtr, int argc, char **argv,
			    int flags));
static void		DestroyButton _ANSI_ARGS_((Button *butPtr));
static void		DisplayButton _ANSI_ARGS_((ClientData clientData));
static int		InvokeButton  _ANSI_ARGS_((Button *butPtr));

/*
 *--------------------------------------------------------------
 *
 * Tk_ButtonCmd, Tk_CheckbuttonCmd, Tk_LabelCmd, Tk_RadiobuttonCmd --
 *
 *	These procedures are invoked to process the "button", "label",
 *	"radiobutton", and "checkbutton" Tcl commands.  See the
 *	user documentation for details on what they do.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.  These procedures are just wrappers;
 *	they call ButtonCreate to do all of the real work.
 *
 *--------------------------------------------------------------
 */

int
Tk_ButtonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_BUTTON);
}

int
Tk_CheckbuttonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_CHECK_BUTTON);
}

int
Tk_LabelCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_LABEL);
}

int
Tk_RadiobuttonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_RADIO_BUTTON);
}

/*
 *--------------------------------------------------------------
 *
 * ButtonCreate --
 *
 *	This procedure does all the real work of implementing the
 *	"button", "label", "radiobutton", and "checkbutton" Tcl
 *	commands.  See the user documentation for details on what it does.
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
ButtonCreate(clientData, interp, argc, argv, type)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
    int type;			/* Type of button to create: TYPE_LABEL,
				 * TYPE_BUTTON, TYPE_CHECK_BUTTON, or
				 * TYPE_RADIO_BUTTON. */
{
    register Button *butPtr;
    Tk_Window tkwin = (Tk_Window) clientData;
    Tk_Window new;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Create the new window.
     */

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *) NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize the data structure for the button.
     */

    butPtr = (Button *) ckalloc(sizeof(Button));
    butPtr->tkwin = new;
    butPtr->display = Tk_Display(new);
    butPtr->widgetCmd = Tcl_CreateCommand(interp, Tk_PathName(butPtr->tkwin),
	    ButtonWidgetCmd, (ClientData) butPtr, ButtonCmdDeletedProc);
    butPtr->interp = interp;
    butPtr->type = type;
    butPtr->text = NULL;
    butPtr->numChars = 0;
    butPtr->underline = -1;
    butPtr->textVarName = NULL;
    butPtr->bitmap = None;
    butPtr->imageString = NULL;
    butPtr->image = NULL;
    butPtr->selectImageString = NULL;
    butPtr->selectImage = NULL;
    butPtr->state = tkNormalUid;
    butPtr->normalBorder = NULL;
    butPtr->activeBorder = NULL;
    butPtr->borderWidth = 0;
    butPtr->relief = TK_RELIEF_FLAT;
    butPtr->highlightWidth = 0;
    butPtr->highlightBgColorPtr = NULL;
    butPtr->highlightColorPtr = NULL;
    butPtr->inset = 0;
    butPtr->fontPtr = NULL;
    butPtr->normalFg = NULL;
    butPtr->activeFg = NULL;
    butPtr->disabledFg = NULL;
    butPtr->normalTextGC = None;
    butPtr->activeTextGC = None;
    butPtr->gray = None;
    butPtr->disabledGC = None;
    butPtr->copyGC = None;
    butPtr->widthString = NULL;
    butPtr->heightString = NULL;
    butPtr->width = 0;
    butPtr->height = 0;
    butPtr->wrapLength = 0;
    butPtr->padX = 0;
    butPtr->padY = 0;
    butPtr->anchor = TK_ANCHOR_CENTER;
    butPtr->justify = TK_JUSTIFY_CENTER;
    butPtr->indicatorOn = 0;
    butPtr->selectBorder = NULL;
    butPtr->indicatorSpace = 0;
    butPtr->indicatorDiameter = 0;
    butPtr->selVarName = NULL;
    butPtr->onValue = NULL;
    butPtr->offValue = NULL;
    butPtr->cursor = None;
    butPtr->command = NULL;
    butPtr->takeFocus = NULL;
    butPtr->flags = 0;

    Tk_SetClass(new, classNames[type]);
    Tk_CreateEventHandler(butPtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    ButtonEventProc, (ClientData) butPtr);
    if (ConfigureButton(interp, butPtr, argc-2, argv+2,
	    configFlags[type]) != TCL_OK) {
	Tk_DestroyWindow(butPtr->tkwin);
	return TCL_ERROR;
    }

    interp->result = Tk_PathName(butPtr->tkwin);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonWidgetCmd --
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
ButtonWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about button widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Button *butPtr = (Button *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	sprintf(interp->result,
		"wrong # args: should be \"%.50s option ?arg arg ...?\"",
		argv[0]);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) butPtr);
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
	result = Tk_ConfigureValue(interp, butPtr->tkwin, configSpecs,
		(char *) butPtr, argv[2], configFlags[butPtr->type]);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, butPtr->tkwin, configSpecs,
		    (char *) butPtr, (char *) NULL, configFlags[butPtr->type]);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, butPtr->tkwin, configSpecs,
		    (char *) butPtr, argv[2],
		    configFlags[butPtr->type]);
	} else {
	    result = ConfigureButton(interp, butPtr, argc-2, argv+2,
		    configFlags[butPtr->type] | TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "deselect", length) == 0)
	    && (butPtr->type >= TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s deselect\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->type == TYPE_CHECK_BUTTON) {
	    if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	} else if (butPtr->flags & SELECTED) {
	    if (Tcl_SetVar(interp, butPtr->selVarName, "",
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    };
	}
    } else if ((c == 'f') && (strncmp(argv[1], "flash", length) == 0)
	    && (butPtr->type != TYPE_LABEL)) {
	int i;

	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s flash\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->state != tkDisabledUid) {
	    for (i = 0; i < 4; i++) {
		butPtr->state = (butPtr->state == tkNormalUid)
			? tkActiveUid : tkNormalUid;
		Tk_SetBackgroundFromBorder(butPtr->tkwin,
			(butPtr->state == tkActiveUid) ? butPtr->activeBorder
			: butPtr->normalBorder);
		DisplayButton((ClientData) butPtr);

		/*
		 * Special note: must cancel any existing idle handler
		 * for DisplayButton;  it's no longer needed, and DisplayButton
		 * cleared the REDRAW_PENDING flag.
		 */

		Tcl_CancelIdleCall(DisplayButton, (ClientData) butPtr);
		XFlush(butPtr->display);
		Tcl_Sleep(50);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "invoke", length) == 0)
	    && (butPtr->type > TYPE_LABEL)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s invoke\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->state != tkDisabledUid) {
	    result = InvokeButton(butPtr);
	}
    } else if ((c == 's') && (strncmp(argv[1], "select", length) == 0)
	    && (butPtr->type >= TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s select\"",
		    argv[0]);
	    goto error;
	}
	if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->onValue,
		TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	    result = TCL_ERROR;
	}
    } else if ((c == 't') && (strncmp(argv[1], "toggle", length) == 0)
	    && (length >= 2) && (butPtr->type == TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s toggle\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->flags & SELECTED) {
	    if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY) == NULL) {
		result = TCL_ERROR;
	    }
	} else {
	    if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->onValue,
		    TCL_GLOBAL_ONLY) == NULL) {
		result = TCL_ERROR;
	    }
	}
    } else {
	sprintf(interp->result,
		"bad option \"%.50s\": must be %s", argv[1],
		optionStrings[butPtr->type]);
	goto error;
    }
    Tcl_Release((ClientData) butPtr);
    return result;

    error:
    Tcl_Release((ClientData) butPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyButton --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a button at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyButton(butPtr)
    Button *butPtr;		/* Info about button widget. */
{
    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (butPtr->textVarName != NULL) {
	Tcl_UntraceVar(butPtr->interp, butPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }
    if (butPtr->image != NULL) {
	Tk_FreeImage(butPtr->image);
    }
    if (butPtr->selectImage != NULL) {
	Tk_FreeImage(butPtr->selectImage);
    }
    if (butPtr->normalTextGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->normalTextGC);
    }
    if (butPtr->activeTextGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->activeTextGC);
    }
    if (butPtr->gray != None) {
	Tk_FreeBitmap(butPtr->display, butPtr->gray);
    }
    if (butPtr->disabledGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->disabledGC);
    }
    if (butPtr->copyGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->copyGC);
    }
    if (butPtr->selVarName != NULL) {
	Tcl_UntraceVar(butPtr->interp, butPtr->selVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }
    Tk_FreeOptions(configSpecs, (char *) butPtr, butPtr->display,
	    configFlags[butPtr->type]);
    Tcl_EventuallyFree((ClientData)butPtr, TCL_DYNAMIC);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureButton --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a button widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for butPtr;  old resources get freed, if there
 *	were any.  The button is redisplayed.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureButton(interp, butPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Button *butPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    XGCValues gcValues;
    GC newGC;
    unsigned long mask;
    Tk_Image image;

    /*
     * Eliminate any existing trace on variables monitored by the button.
     */

    if (butPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, butPtr->textVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }
    if (butPtr->selVarName != NULL) {
	Tcl_UntraceVar(interp, butPtr->selVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }

    if (Tk_ConfigureWidget(interp, butPtr->tkwin, configSpecs,
	    argc, argv, (char *) butPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as setting the
     * background from a 3-D border, or filling in complicated
     * defaults that couldn't be specified to Tk_ConfigureWidget.
     */

    if ((butPtr->state == tkActiveUid) && !Tk_StrictMotif(butPtr->tkwin)) {
	Tk_SetBackgroundFromBorder(butPtr->tkwin, butPtr->activeBorder);
    } else {
	Tk_SetBackgroundFromBorder(butPtr->tkwin, butPtr->normalBorder);
	if ((butPtr->state != tkNormalUid) && (butPtr->state != tkActiveUid)
		&& (butPtr->state != tkDisabledUid)) {
	    Tcl_AppendResult(interp, "bad state value \"", butPtr->state,
		    "\": must be normal, active, or disabled", (char *) NULL);
	    butPtr->state = tkNormalUid;
	    return TCL_ERROR;
	}
    }

    if (butPtr->highlightWidth < 0) {
	butPtr->highlightWidth = 0;
    }

    gcValues.font = butPtr->fontPtr->fid;
    gcValues.foreground = butPtr->normalFg->pixel;
    gcValues.background = Tk_3DBorderColor(butPtr->normalBorder)->pixel;
    
    /*
     * Note: GraphicsExpose events are disabled in normalTextGC because it's
     * used to copy stuff from an off-screen pixmap onto the screen (we know
     * that there's no problem with obscured areas).
     */

    gcValues.graphics_exposures = False;
    newGC = Tk_GetGC(butPtr->tkwin,
	    GCForeground|GCBackground|GCFont|GCGraphicsExposures,
	    &gcValues);
    if (butPtr->normalTextGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->normalTextGC);
    }
    butPtr->normalTextGC = newGC;

    if (butPtr->activeFg != NULL) {
	gcValues.font = butPtr->fontPtr->fid;
	gcValues.foreground = butPtr->activeFg->pixel;
	gcValues.background = Tk_3DBorderColor(butPtr->activeBorder)->pixel;
	newGC = Tk_GetGC(butPtr->tkwin,
		GCForeground|GCBackground|GCFont, &gcValues);
	if (butPtr->activeTextGC != None) {
	    Tk_FreeGC(butPtr->display, butPtr->activeTextGC);
	}
	butPtr->activeTextGC = newGC;
    }

    if (butPtr->type != TYPE_LABEL) {
	gcValues.font = butPtr->fontPtr-> fid;
	gcValues.background = Tk_3DBorderColor(butPtr->normalBorder)->pixel;
	if ((butPtr->disabledFg != NULL) && (butPtr->imageString == NULL)) {
	    gcValues.foreground = butPtr->disabledFg->pixel;
	    mask = GCForeground|GCBackground|GCFont;
	} else {
	    gcValues.foreground = gcValues.background;
	    if (butPtr->gray == None) {
		butPtr->gray = Tk_GetBitmap(interp, butPtr->tkwin,
			Tk_GetUid("gray50"));
		if (butPtr->gray == None) {
		    return TCL_ERROR;
		}
	    }
	    gcValues.fill_style = FillStippled;
	    gcValues.stipple = butPtr->gray;
	    mask = GCForeground|GCFillStyle|GCStipple;
	}
	newGC = Tk_GetGC(butPtr->tkwin, mask, &gcValues);
	if (butPtr->disabledGC != None) {
	    Tk_FreeGC(butPtr->display, butPtr->disabledGC);
	}
	butPtr->disabledGC = newGC;
    }

    if (butPtr->copyGC == None) {
	butPtr->copyGC = Tk_GetGC(butPtr->tkwin, 0, &gcValues);
    }

    if (butPtr->padX < 0) {
	butPtr->padX = 0;
    }
    if (butPtr->padY < 0) {
	butPtr->padY = 0;
    }

    if (butPtr->type >= TYPE_CHECK_BUTTON) {
	char *value;

	if (butPtr->selVarName == NULL) {
	    butPtr->selVarName = (char *) ckalloc((unsigned)
		    (strlen(Tk_Name(butPtr->tkwin)) + 1));
	    strcpy(butPtr->selVarName, Tk_Name(butPtr->tkwin));
	}

	/*
	 * Select the button if the associated variable has the
	 * appropriate value, initialize the variable if it doesn't
	 * exist, then set a trace on the variable to monitor future
	 * changes to its value.
	 */

	value = Tcl_GetVar(interp, butPtr->selVarName, TCL_GLOBAL_ONLY);
	butPtr->flags &= ~SELECTED;
	if (value != NULL) {
	    if (strcmp(value, butPtr->onValue) == 0) {
		butPtr->flags |= SELECTED;
	    }
	} else {
	    if (Tcl_SetVar(interp, butPtr->selVarName,
		    (butPtr->type == TYPE_CHECK_BUTTON) ? butPtr->offValue : "",
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	}
	Tcl_TraceVar(interp, butPtr->selVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }

    /*
     * Get the images for the widget, if there are any.  Allocate the
     * new images before freeing the old ones, so that the reference
     * counts don't go to zero and cause image data to be discarded.
     */

    if (butPtr->imageString != NULL) {
	image = Tk_GetImage(butPtr->interp, butPtr->tkwin,
		butPtr->imageString, ButtonImageProc, (ClientData) butPtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (butPtr->image != NULL) {
	Tk_FreeImage(butPtr->image);
    }
    butPtr->image = image;
    if (butPtr->selectImageString != NULL) {
	image = Tk_GetImage(butPtr->interp, butPtr->tkwin,
		butPtr->selectImageString, ButtonSelectImageProc,
		(ClientData) butPtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (butPtr->selectImage != NULL) {
	Tk_FreeImage(butPtr->selectImage);
    }
    butPtr->selectImage = image;

    if ((butPtr->image == NULL) && (butPtr->bitmap == None)
	    && (butPtr->textVarName != NULL)) {
	/*
	 * The button must display the value of a variable: set up a trace
	 * on the variable's value, create the variable if it doesn't
	 * exist, and fetch its current value.
	 */

	char *value;

	value = Tcl_GetVar(interp, butPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    if (Tcl_SetVar(interp, butPtr->textVarName, butPtr->text,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    if (butPtr->text != NULL) {
		ckfree(butPtr->text);
	    }
	    butPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
	    strcpy(butPtr->text, value);
	}
	Tcl_TraceVar(interp, butPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }

    if ((butPtr->bitmap != None) || (butPtr->image != NULL)) {
	if (Tk_GetPixels(interp, butPtr->tkwin, butPtr->widthString,
		&butPtr->width) != TCL_OK) {
	    widthError:
	    Tcl_AddErrorInfo(interp, "\n    (processing -width option)");
	    return TCL_ERROR;
	}
	if (Tk_GetPixels(interp, butPtr->tkwin, butPtr->heightString,
		&butPtr->height) != TCL_OK) {
	    heightError:
	    Tcl_AddErrorInfo(interp, "\n    (processing -height option)");
	    return TCL_ERROR;
	}
    } else {
	if (Tcl_GetInt(interp, butPtr->widthString, &butPtr->width)
		!= TCL_OK) {
	    goto widthError;
	}
	if (Tcl_GetInt(interp, butPtr->heightString, &butPtr->height)
		!= TCL_OK) {
	    goto heightError;
	}
    }
    ComputeButtonGeometry(butPtr);

    /*
     * Lastly, arrange for the button to be redisplayed.
     */

    if (Tk_IsMapped(butPtr->tkwin) && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayButton --
 *
 *	This procedure is invoked to display a button widget.  It is
 *	normally invoked as an idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the button in its
 *	current mode.  The REDRAW_PENDING flag is cleared.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayButton(clientData)
    ClientData clientData;	/* Information about widget. */
{
    register Button *butPtr = (Button *) clientData;
    GC gc;
    Tk_3DBorder border;
    Pixmap pixmap;
    int x = 0;			/* Initialization only needed to stop
				 * compiler warning. */
    int y, relief;
    register Tk_Window tkwin = butPtr->tkwin;
    int width, height;
    int offset;			/* 0 means this is a label widget.  1 means
				 * it is a flavor of button, so we offset
				 * the text to make the button appear to
				 * move up and down as the relief changes. */

    butPtr->flags &= ~REDRAW_PENDING;
    if ((butPtr->tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	return;
    }

    border = butPtr->normalBorder;
    if ((butPtr->state == tkDisabledUid) && (butPtr->disabledFg != NULL)) {
	gc = butPtr->disabledGC;
    } else if ((butPtr->state == tkActiveUid)
	    && !Tk_StrictMotif(butPtr->tkwin)) {
	gc = butPtr->activeTextGC;
	border = butPtr->activeBorder;
    } else {
	gc = butPtr->normalTextGC;
    }
    if ((butPtr->flags & SELECTED) && (butPtr->state != tkActiveUid)
	    && (butPtr->selectBorder != NULL) && !butPtr->indicatorOn) {
	border = butPtr->selectBorder;
    }

    /*
     * Override the relief specified for the button if this is a
     * checkbutton or radiobutton and there's no indicator.
     */

    relief = butPtr->relief;
    if ((butPtr->type >= TYPE_CHECK_BUTTON) && !butPtr->indicatorOn) {
	relief = (butPtr->flags & SELECTED) ? TK_RELIEF_SUNKEN
		: TK_RELIEF_RAISED;
    }

    offset = (butPtr->type == TYPE_BUTTON) && !Tk_StrictMotif(butPtr->tkwin);

    /*
     * In order to avoid screen flashes, this procedure redraws
     * the button in a pixmap, then copies the pixmap to the
     * screen in a single operation.  This means that there's no
     * point in time where the on-sreen image has been cleared.
     */

    pixmap = Tk_GetPixmap(butPtr->display, Tk_WindowId(tkwin),
	    Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));
    Tk_Fill3DRectangle(tkwin, pixmap, border, 0, 0, Tk_Width(tkwin),
	    Tk_Height(tkwin), 0, TK_RELIEF_FLAT);

    /*
     * Display image or bitmap or text for button.
     */

    if (butPtr->image != None) {
	Tk_SizeOfImage(butPtr->image, &width, &height);

	imageOrBitmap:
	switch (butPtr->anchor) {
	    case TK_ANCHOR_NW: case TK_ANCHOR_W: case TK_ANCHOR_SW:
		x = butPtr->inset + butPtr->indicatorSpace + offset;
		break;
	    case TK_ANCHOR_N: case TK_ANCHOR_CENTER: case TK_ANCHOR_S:
		x = ((int) (Tk_Width(tkwin) + butPtr->indicatorSpace
			- width))/2;
		break;
	    default:
		x = Tk_Width(tkwin) - butPtr->inset - width - offset;
		break;
	}
	switch (butPtr->anchor) {
	    case TK_ANCHOR_NW: case TK_ANCHOR_N: case TK_ANCHOR_NE:
		y = butPtr->inset + offset;
		break;
	    case TK_ANCHOR_W: case TK_ANCHOR_CENTER: case TK_ANCHOR_E:
		y = ((int) (Tk_Height(tkwin) - height))/2;
		break;
	    default:
		y = Tk_Height(tkwin) - butPtr->inset - height - offset;
		break;
	}
	if (relief == TK_RELIEF_RAISED) {
	    x -= offset;
	    y -= offset;
	} else if (relief == TK_RELIEF_SUNKEN) {
	    x += offset;
	    y += offset;
	}
	if (butPtr->image != NULL) {
	    if ((butPtr->selectImage != NULL) && (butPtr->flags & SELECTED)) {
		Tk_RedrawImage(butPtr->selectImage, 0, 0, width, height, pixmap,
			x, y);
	    } else {
		Tk_RedrawImage(butPtr->image, 0, 0, width, height, pixmap,
			x, y);
	    }
	} else {
	    XSetClipOrigin(butPtr->display, gc, x, y);
	    XCopyPlane(butPtr->display, butPtr->bitmap, pixmap, gc, 0, 0,
		    (unsigned int) width, (unsigned int) height, x, y, 1);
	    XSetClipOrigin(butPtr->display, gc, 0, 0);
	}
	y += height/2;
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	goto imageOrBitmap;
    } else {
	switch (butPtr->anchor) {
	    case TK_ANCHOR_NW: case TK_ANCHOR_W: case TK_ANCHOR_SW:
		x = butPtr->inset + butPtr->padX + butPtr->indicatorSpace
			+ offset;
		break;
	    case TK_ANCHOR_N: case TK_ANCHOR_CENTER: case TK_ANCHOR_S:
		x = ((int) (Tk_Width(tkwin) + butPtr->indicatorSpace
			- butPtr->textWidth))/2;
		break;
	    default:
		x = Tk_Width(tkwin) - butPtr->inset - butPtr->padX
			- butPtr->textWidth - offset;
		break;
	}
	switch (butPtr->anchor) {
	    case TK_ANCHOR_NW: case TK_ANCHOR_N: case TK_ANCHOR_NE:
		y = butPtr->inset + butPtr->padY + offset;
		break;
	    case TK_ANCHOR_W: case TK_ANCHOR_CENTER: case TK_ANCHOR_E:
		y = ((int) (Tk_Height(tkwin) - butPtr->textHeight))/2;
		break;
	    default:
		y = Tk_Height(tkwin) - butPtr->inset - butPtr->padY
			- butPtr->textHeight - offset;
		break;
	}
	if (relief == TK_RELIEF_RAISED) {
	    x -= offset;
	    y -= offset;
	} else if (relief == TK_RELIEF_SUNKEN) {
	    x += offset;
	    y += offset;
	}
	TkDisplayText(butPtr->display, pixmap, butPtr->fontPtr,
		butPtr->text, butPtr->numChars, x, y, butPtr->textWidth,
		butPtr->justify, butPtr->underline, gc);
	y += butPtr->textHeight/2;
    }

    /*
     * Draw the indicator for check buttons and radio buttons.  At this
     * point x and y refer to the top-left corner of the text or image
     * or bitmap.
     */

    if ((butPtr->type == TYPE_CHECK_BUTTON) && butPtr->indicatorOn) {
	int dim;

	dim = butPtr->indicatorDiameter;
	x -= butPtr->indicatorSpace;
	y -= dim/2;
	if (dim > 2*butPtr->borderWidth) {
	    Tk_Draw3DRectangle(tkwin, pixmap, border, x, y, dim, dim,
		    butPtr->borderWidth, 
		    (butPtr->flags & SELECTED) ? TK_RELIEF_SUNKEN :
		    TK_RELIEF_RAISED);
	    x += butPtr->borderWidth;
	    y += butPtr->borderWidth;
	    dim -= 2*butPtr->borderWidth;
	    if (butPtr->flags & SELECTED) {
		GC gc;

		gc = Tk_3DBorderGC(tkwin,(butPtr->selectBorder != NULL)
			? butPtr->selectBorder : butPtr->normalBorder,
			TK_3D_FLAT_GC);
		XFillRectangle(butPtr->display, pixmap, gc, x, y,
			(unsigned int) dim, (unsigned int) dim);
	    } else {
		Tk_Fill3DRectangle(tkwin, pixmap, butPtr->normalBorder, x, y,
			dim, dim, butPtr->borderWidth, TK_RELIEF_FLAT);
	    }
	}
    } else if ((butPtr->type == TYPE_RADIO_BUTTON) && butPtr->indicatorOn) {
	XPoint points[4];
	int radius;

	radius = butPtr->indicatorDiameter/2;
	points[0].x = x - butPtr->indicatorSpace;
	points[0].y = y;
	points[1].x = points[0].x + radius;
	points[1].y = points[0].y + radius;
	points[2].x = points[1].x + radius;
	points[2].y = points[0].y;
	points[3].x = points[1].x;
	points[3].y = points[0].y - radius;
	if (butPtr->flags & SELECTED) {
	    GC gc;

	    gc = Tk_3DBorderGC(tkwin, (butPtr->selectBorder != NULL)
		    ? butPtr->selectBorder : butPtr->normalBorder,
		    TK_3D_FLAT_GC);
	    XFillPolygon(butPtr->display, pixmap, gc, points, 4, Convex,
		    CoordModeOrigin);
	} else {
	    Tk_Fill3DPolygon(tkwin, pixmap, butPtr->normalBorder, points,
		    4, butPtr->borderWidth, TK_RELIEF_FLAT);
	}
	Tk_Draw3DPolygon(tkwin, pixmap, border, points, 4, butPtr->borderWidth,
		(butPtr->flags & SELECTED) ? TK_RELIEF_SUNKEN :
		TK_RELIEF_RAISED);
    }

    /*
     * If the button is disabled with a stipple rather than a special
     * foreground color, generate the stippled effect.  If the widget
     * is selected and we use a different background color when selected,
     * must temporarily modify the GC.
     */

    if ((butPtr->state == tkDisabledUid)
	    && ((butPtr->disabledFg == NULL) || (butPtr->image != NULL))) {
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->disabledGC,
		    Tk_3DBorderColor(butPtr->selectBorder)->pixel);
	}
	XFillRectangle(butPtr->display, pixmap, butPtr->disabledGC,
		butPtr->inset, butPtr->inset,
		(unsigned) (Tk_Width(tkwin) - 2*butPtr->inset),
		(unsigned) (Tk_Height(tkwin) - 2*butPtr->inset));
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->disabledGC,
		    Tk_3DBorderColor(butPtr->normalBorder)->pixel);
	}
    }

    /*
     * Draw the border and traversal highlight last.  This way, if the
     * button's contents overflow they'll be covered up by the border.
     */

    if (relief != TK_RELIEF_FLAT) {
	Tk_Draw3DRectangle(tkwin, pixmap, border,
		butPtr->highlightWidth, butPtr->highlightWidth,
		Tk_Width(tkwin) - 2*butPtr->highlightWidth,
		Tk_Height(tkwin) - 2*butPtr->highlightWidth,
		butPtr->borderWidth, relief);
    }
    if (butPtr->highlightWidth != 0) {
	GC gc;

	if (butPtr->flags & GOT_FOCUS) {
	    gc = Tk_GCForColor(butPtr->highlightColorPtr, pixmap);
	} else {
	    gc = Tk_GCForColor(butPtr->highlightBgColorPtr, pixmap);
	}
	Tk_DrawFocusHighlight(tkwin, gc, butPtr->highlightWidth, pixmap);
    }

    /*
     * Copy the information from the off-screen pixmap onto the screen,
     * then delete the pixmap.
     */

    XCopyArea(butPtr->display, pixmap, Tk_WindowId(tkwin),
	    butPtr->copyGC, 0, 0, (unsigned) Tk_Width(tkwin),
	    (unsigned) Tk_Height(tkwin), 0, 0);
    Tk_FreePixmap(butPtr->display, pixmap);
}

/*
 *--------------------------------------------------------------
 *
 * ButtonEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on buttons.
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
ButtonEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Button *butPtr = (Button *) clientData;
    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	goto redraw;
    } else if (eventPtr->type == ConfigureNotify) {
	/*
	 * Must redraw after size changes, since layout could have changed
	 * and borders will need to be redrawn.
	 */

	goto redraw;
    } else if (eventPtr->type == DestroyNotify) {
	if (butPtr->tkwin != NULL) {
	    butPtr->tkwin = NULL;
	    Tcl_DeleteCommand(butPtr->interp,
		    Tcl_GetCommandName(butPtr->interp, butPtr->widgetCmd));
	}
	if (butPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayButton, (ClientData) butPtr);
	}
	DestroyButton(butPtr);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    butPtr->flags |= GOT_FOCUS;
	    if (butPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    butPtr->flags &= ~GOT_FOCUS;
	    if (butPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    }
    return;

    redraw:
    if ((butPtr->tkwin != NULL) && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonCmdDeletedProc --
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
ButtonCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Button *butPtr = (Button *) clientData;
    Tk_Window tkwin = butPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	butPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeButtonGeometry --
 *
 *	After changes in a button's text or bitmap, this procedure
 *	recomputes the button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The button's window may change size.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeButtonGeometry(butPtr)
    register Button *butPtr;	/* Button whose geometry may have changed. */
{
    int width, height;

    if (butPtr->highlightWidth < 0) {
	butPtr->highlightWidth = 0;
    }
    butPtr->inset = butPtr->highlightWidth + butPtr->borderWidth;
    butPtr->indicatorSpace = 0;
    if (butPtr->image != NULL) {
	Tk_SizeOfImage(butPtr->image, &width, &height);
	imageOrBitmap:
	if (butPtr->width > 0) {
	    width = butPtr->width;
	}
	if (butPtr->height > 0) {
	    height = butPtr->height;
	}
	if ((butPtr->type >= TYPE_CHECK_BUTTON) && butPtr->indicatorOn) {
	    butPtr->indicatorSpace = height;
	    if (butPtr->type == TYPE_CHECK_BUTTON) {
		butPtr->indicatorDiameter = (65*height)/100;
	    } else {
		butPtr->indicatorDiameter = (75*height)/100;
	    }
	}
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	goto imageOrBitmap;
    } else {
	butPtr->numChars = strlen(butPtr->text);
	TkComputeTextGeometry(butPtr->fontPtr, butPtr->text,
		butPtr->numChars, butPtr->wrapLength, &butPtr->textWidth,
		&butPtr->textHeight);
	width = butPtr->textWidth;
	height = butPtr->textHeight;
	if (butPtr->width > 0) {
	    width = butPtr->width * XTextWidth(butPtr->fontPtr, "0", 1);
	}
	if (butPtr->height > 0) {
	    height = butPtr->height * (butPtr->fontPtr->ascent
		    + butPtr->fontPtr->descent);
	}
	if ((butPtr->type >= TYPE_CHECK_BUTTON) && butPtr->indicatorOn) {
	    butPtr->indicatorDiameter = butPtr->fontPtr->ascent
		    + butPtr->fontPtr->descent;
	    if (butPtr->type == TYPE_CHECK_BUTTON) {
		butPtr->indicatorDiameter = (80*butPtr->indicatorDiameter)/100;
	    }
	    butPtr->indicatorSpace = butPtr->indicatorDiameter
		+ XTextWidth(butPtr->fontPtr, "0", 1);
	}
    }

    /*
     * When issuing the geometry request, add extra space for the indicator,
     * if any, and for the border and padding, plus two extra pixels so the
     * display can be offset by 1 pixel in either direction for the raised
     * or lowered effect.
     */

    if ((butPtr->image == NULL) && (butPtr->bitmap == None)) {
	width += 2*butPtr->padX;
	height += 2*butPtr->padY;
    }
    if ((butPtr->type == TYPE_BUTTON) && !Tk_StrictMotif(butPtr->tkwin)) {
	width += 2;
	height += 2;
    }
    Tk_GeometryRequest(butPtr->tkwin, (int) (width + butPtr->indicatorSpace
	    + 2*butPtr->inset), (int) (height + 2*butPtr->inset));
    Tk_SetInternalBorder(butPtr->tkwin, butPtr->inset);
}

/*
 *----------------------------------------------------------------------
 *
 * InvokeButton --
 *
 *	This procedure is called to carry out the actions associated
 *	with a button, such as invoking a Tcl command or setting a
 *	variable.  This procedure is invoked, for example, when the
 *	button is invoked via the mouse.
 *
 * Results:
 *	A standard Tcl return value.  Information is also left in
 *	interp->result.
 *
 * Side effects:
 *	Depends on the button and its associated command.
 *
 *----------------------------------------------------------------------
 */

static int
InvokeButton(butPtr)
    register Button *butPtr;		/* Information about button. */
{
    if (butPtr->type == TYPE_CHECK_BUTTON) {
	if (butPtr->flags & SELECTED) {
	    if (Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    if (Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->onValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	}
    } else if (butPtr->type == TYPE_RADIO_BUTTON) {
	if (Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->onValue,
		TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	    return TCL_ERROR;
	}
    }
    if ((butPtr->type != TYPE_LABEL) && (butPtr->command != NULL)) {
	return TkCopyAndGlobalEval(butPtr->interp, butPtr->command);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonVarProc --
 *
 *	This procedure is invoked when someone changes the
 *	state variable associated with a radio button.  Depending
 *	on the new value of the button's variable, the button
 *	may be selected or deselected.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The button may become selected or deselected.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
ButtonVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register Button *butPtr = (Button *) clientData;
    char *value;

    /*
     * If the variable is being unset, then just re-establish the
     * trace unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	butPtr->flags &= ~SELECTED;
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar(interp, butPtr->selVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ButtonVarProc, clientData);
	}
	goto redisplay;
    }

    /*
     * Use the value of the variable to update the selected status of
     * the button.
     */

    value = Tcl_GetVar(interp, butPtr->selVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (strcmp(value, butPtr->onValue) == 0) {
	if (butPtr->flags & SELECTED) {
	    return (char *) NULL;
	}
	butPtr->flags |= SELECTED;
    } else if (butPtr->flags & SELECTED) {
	butPtr->flags &= ~SELECTED;
    } else {
	return (char *) NULL;
    }

    redisplay:
    if ((butPtr->tkwin != NULL) && Tk_IsMapped(butPtr->tkwin)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a button.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the button will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
ButtonTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Not used. */
    char *name2;		/* Not used. */
    int flags;			/* Information about what happened. */
{
    register Button *butPtr = (Button *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar(interp, butPtr->textVarName, butPtr->text,
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, butPtr->textVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ButtonTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    value = Tcl_GetVar(interp, butPtr->textVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (butPtr->text != NULL) {
	ckfree(butPtr->text);
    }
    butPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
    strcpy(butPtr->text, value);
    ComputeButtonGeometry(butPtr);

    if ((butPtr->tkwin != NULL) && Tk_IsMapped(butPtr->tkwin)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonImageProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the size of contents
 *	of an image displayed in a button.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the button to get redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
ButtonImageProc(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;		/* Pointer to widget record. */
    int x, y;				/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, height;			/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imgWidth, imgHeight;		/* New dimensions of image. */
{
    register Button *butPtr = (Button *) clientData;

    if (butPtr->tkwin != NULL) {
	ComputeButtonGeometry(butPtr);
	if (Tk_IsMapped(butPtr->tkwin) && !(butPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	    butPtr->flags |= REDRAW_PENDING;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonSelectImageProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the size of contents
 *	of the image displayed in a button when it is selected.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May arrange for the button to get redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
ButtonSelectImageProc(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;		/* Pointer to widget record. */
    int x, y;				/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, height;			/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imgWidth, imgHeight;		/* New dimensions of image. */
{
    register Button *butPtr = (Button *) clientData;

    /*
     * Don't recompute geometry:  it's controlled by the primary image.
     */

    if ((butPtr->flags & SELECTED) && (butPtr->tkwin != NULL)
	    && Tk_IsMapped(butPtr->tkwin)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
}
