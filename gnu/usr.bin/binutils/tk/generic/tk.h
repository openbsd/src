/*
 * tk.h --
 *
 *	Declarations for Tk-related things that are visible
 *	outside of the Tk module itself.
 *
 * Copyright (c) 1989-1994 The Regents of the University of California.
 * Copyright (c) 1994 The Australian National University.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tk.h 1.184 96/03/18 13:35:54
 */

#ifndef _TK
#define _TK

#define TK_VERSION "4.1"
#define TK_MAJOR_VERSION 4
#define TK_MINOR_VERSION 1

#ifndef _TCL
#include <tcl.h>
#endif
#ifndef _XLIB_H
#ifdef MAC_TCL
#include <Xlib.h>
#include <X.h>
#else
#include <X11/Xlib.h>
#endif
#endif
#ifdef __STDC__
#include <stddef.h>
#endif

/*
 * Decide whether or not to use input methods.
 */

#ifdef XNQueryInputStyle
#define TK_USE_INPUT_METHODS
#endif

/*
 * Dummy types that are used by clients:
 */

typedef struct Tk_BindingTable_ *Tk_BindingTable;
typedef struct Tk_Canvas_ *Tk_Canvas;
typedef struct Tk_Cursor_ *Tk_Cursor;
typedef struct Tk_ErrorHandler_ *Tk_ErrorHandler;
typedef struct Tk_Image__ *Tk_Image;
typedef struct Tk_ImageMaster_ *Tk_ImageMaster;
typedef struct Tk_Window_ *Tk_Window;
typedef struct Tk_3DBorder_ *Tk_3DBorder;

/*
 * Additional types exported to clients.
 */

typedef char *Tk_Uid;

/*
 * Structure used to specify how to handle argv options.
 */

typedef struct {
    char *key;		/* The key string that flags the option in the
			 * argv array. */
    int type;		/* Indicates option type;  see below. */
    char *src;		/* Value to be used in setting dst;  usage
			 * depends on type. */
    char *dst;		/* Address of value to be modified;  usage
			 * depends on type. */
    char *help;		/* Documentation message describing this option. */
} Tk_ArgvInfo;

/*
 * Legal values for the type field of a Tk_ArgvInfo: see the user
 * documentation for details.
 */

#define TK_ARGV_CONSTANT		15
#define TK_ARGV_INT			16
#define TK_ARGV_STRING			17
#define TK_ARGV_UID			18
#define TK_ARGV_REST			19
#define TK_ARGV_FLOAT			20
#define TK_ARGV_FUNC			21
#define TK_ARGV_GENFUNC			22
#define TK_ARGV_HELP			23
#define TK_ARGV_CONST_OPTION		24
#define TK_ARGV_OPTION_VALUE		25
#define TK_ARGV_OPTION_NAME_VALUE	26
#define TK_ARGV_VERSION			27
#define TK_ARGV_END			28

/*
 * Flag bits for passing to Tk_ParseArgv:
 */

#define TK_ARGV_NO_DEFAULTS		0x1
#define TK_ARGV_NO_LEFTOVERS		0x2
#define TK_ARGV_NO_ABBREV		0x4
#define TK_ARGV_DONT_SKIP_FIRST_ARG	0x8

/*
 * Structure used to describe application-specific configuration
 * options:  indicates procedures to call to parse an option and
 * to return a text string describing an option.
 */

typedef int (Tk_OptionParseProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
typedef char *(Tk_OptionPrintProc) _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

typedef struct Tk_CustomOption {
    Tk_OptionParseProc *parseProc;	/* Procedure to call to parse an
					 * option and store it in converted
					 * form. */
    Tk_OptionPrintProc *printProc;	/* Procedure to return a printable
					 * string describing an existing
					 * option. */
    ClientData clientData;		/* Arbitrary one-word value used by
					 * option parser:  passed to
					 * parseProc and printProc. */
} Tk_CustomOption;

/*
 * Structure used to specify information for Tk_ConfigureWidget.  Each
 * structure gives complete information for one option, including
 * how the option is specified on the command line, where it appears
 * in the option database, etc.
 */

typedef struct Tk_ConfigSpec {
    int type;			/* Type of option, such as TK_CONFIG_COLOR;
				 * see definitions below.  Last option in
				 * table must have type TK_CONFIG_END. */
    char *argvName;		/* Switch used to specify option in argv.
				 * NULL means this spec is part of a group. */
    char *dbName;		/* Name for option in option database. */
    char *dbClass;		/* Class for option in database. */
    char *defValue;		/* Default value for option if not
				 * specified in command line or database. */
    int offset;			/* Where in widget record to store value;
				 * use Tk_Offset macro to generate values
				 * for this. */
    int specFlags;		/* Any combination of the values defined
				 * below;  other bits are used internally
				 * by tkConfig.c. */
    Tk_CustomOption *customPtr;	/* If type is TK_CONFIG_CUSTOM then this is
				 * a pointer to info about how to parse and
				 * print the option.  Otherwise it is
				 * irrelevant. */
} Tk_ConfigSpec;

/*
 * Type values for Tk_ConfigSpec structures.  See the user
 * documentation for details.
 */

#define TK_CONFIG_BOOLEAN	1
#define TK_CONFIG_INT		2
#define TK_CONFIG_DOUBLE	3
#define TK_CONFIG_STRING	4
#define TK_CONFIG_UID		5
#define TK_CONFIG_COLOR		6
#define TK_CONFIG_FONT		7
#define TK_CONFIG_BITMAP	8
#define TK_CONFIG_BORDER	9
#define TK_CONFIG_RELIEF	10
#define TK_CONFIG_CURSOR	11
#define TK_CONFIG_ACTIVE_CURSOR	12
#define TK_CONFIG_JUSTIFY	13
#define TK_CONFIG_ANCHOR	14
#define TK_CONFIG_SYNONYM	15
#define TK_CONFIG_CAP_STYLE	16
#define TK_CONFIG_JOIN_STYLE	17
#define TK_CONFIG_PIXELS	18
#define TK_CONFIG_MM		19
#define TK_CONFIG_WINDOW	20
#define TK_CONFIG_CUSTOM	21
#define TK_CONFIG_END		22

/*
 * Macro to use to fill in "offset" fields of Tk_ConfigInfos.
 * Computes number of bytes from beginning of structure to a
 * given field.
 */

#ifdef offsetof
#define Tk_Offset(type, field) ((int) offsetof(type, field))
#else
#define Tk_Offset(type, field) ((int) ((char *) &((type *) 0)->field))
#endif

/*
 * Possible values for flags argument to Tk_ConfigureWidget:
 */

#define TK_CONFIG_ARGV_ONLY	1

/*
 * Possible flag values for Tk_ConfigInfo structures.  Any bits at
 * or above TK_CONFIG_USER_BIT may be used by clients for selecting
 * certain entries.  Before changing any values here, coordinate with
 * tkConfig.c (internal-use-only flags are defined there).
 */

#define TK_CONFIG_COLOR_ONLY		1
#define TK_CONFIG_MONO_ONLY		2
#define TK_CONFIG_NULL_OK		4
#define TK_CONFIG_DONT_SET_DEFAULT	8
#define TK_CONFIG_OPTION_SPECIFIED	0x10
#define TK_CONFIG_USER_BIT		0x100

/*
 * Enumerated type for describing actions to be taken in response
 * to a restrictProc established by Tk_RestrictEvents.
 */

typedef enum {
    TK_DEFER_EVENT, TK_PROCESS_EVENT, TK_DISCARD_EVENT
} Tk_RestrictAction;

/*
 * Priority levels to pass to Tk_AddOption:
 */

#define TK_WIDGET_DEFAULT_PRIO	20
#define TK_STARTUP_FILE_PRIO	40
#define TK_USER_DEFAULT_PRIO	60
#define TK_INTERACTIVE_PRIO	80
#define TK_MAX_PRIO		100

/*
 * Relief values returned by Tk_GetRelief:
 */

#define TK_RELIEF_RAISED	1
#define TK_RELIEF_FLAT		2
#define TK_RELIEF_SUNKEN	4
#define TK_RELIEF_GROOVE	8
#define TK_RELIEF_RIDGE		16

/*
 * "Which" argument values for Tk_3DBorderGC:
 */

#define TK_3D_FLAT_GC		1
#define TK_3D_LIGHT_GC		2
#define TK_3D_DARK_GC		3

/*
 * Special EnterNotify/LeaveNotify "mode" for use in events
 * generated by tkShare.c.  Pick a high enough value that it's
 * unlikely to conflict with existing values (like NotifyNormal)
 * or any new values defined in the future.
 */

#define TK_NOTIFY_SHARE		20

/*
 * Enumerated type for describing a point by which to anchor something:
 */

typedef enum {
    TK_ANCHOR_N, TK_ANCHOR_NE, TK_ANCHOR_E, TK_ANCHOR_SE,
    TK_ANCHOR_S, TK_ANCHOR_SW, TK_ANCHOR_W, TK_ANCHOR_NW,
    TK_ANCHOR_CENTER
} Tk_Anchor;

/*
 * Enumerated type for describing a style of justification:
 */

typedef enum {
    TK_JUSTIFY_LEFT, TK_JUSTIFY_RIGHT, TK_JUSTIFY_CENTER
} Tk_Justify;

/*
 * Each geometry manager (the packer, the placer, etc.) is represented
 * by a structure of the following form, which indicates procedures
 * to invoke in the geometry manager to carry out certain functions.
 */

typedef void (Tk_GeomRequestProc) _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));
typedef void (Tk_GeomLostSlaveProc) _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));

typedef struct Tk_GeomMgr {
    char *name;			/* Name of the geometry manager (command
				 * used to invoke it, or name of widget
				 * class that allows embedded widgets). */
    Tk_GeomRequestProc *requestProc;
				/* Procedure to invoke when a slave's
				 * requested geometry changes. */
    Tk_GeomLostSlaveProc *lostSlaveProc;
				/* Procedure to invoke when a slave is
				 * taken away from one geometry manager
				 * by another.  NULL means geometry manager
				 * doesn't care when slaves are lost. */
} Tk_GeomMgr;

/*
 * Result values returned by Tk_GetScrollInfo:
 */

#define TK_SCROLL_MOVETO	1
#define TK_SCROLL_PAGES		2
#define TK_SCROLL_UNITS		3
#define TK_SCROLL_ERROR		4

/*---------------------------------------------------------------------------
 *
 * Extensions to the X event set
 *
 *---------------------------------------------------------------------------
 */

#define ActivateNotify	    (LASTEvent)
#define DeactivateNotify    (LASTEvent + 1)
#define TK_LASTEVENT	    (LASTEvent + 2)

#define ActivateMask	    (1L << 30)

typedef struct {
    int type;
    unsigned long serial;   /* # of last request processed by server */
    Bool send_event;	    /* True if this came from a SendEvent request */
    Display *display;	    /* Display the event was read from */
    Window window;	    /* Window in which event occurred. */
} XActivateDeactivateEvent;
typedef XActivateDeactivateEvent XActivateEvent;
typedef XActivateDeactivateEvent XDeactivateEvent;

/*
 *--------------------------------------------------------------
 *
 * Macros for querying Tk_Window structures.  See the
 * manual entries for documentation.
 *
 *--------------------------------------------------------------
 */

#define Tk_Display(tkwin)		(((Tk_FakeWin *) (tkwin))->display)
#define Tk_ScreenNumber(tkwin)		(((Tk_FakeWin *) (tkwin))->screenNum)
#define Tk_Screen(tkwin)		(ScreenOfDisplay(Tk_Display(tkwin), \
	Tk_ScreenNumber(tkwin)))
#define Tk_Depth(tkwin)			(((Tk_FakeWin *) (tkwin))->depth)
#define Tk_Visual(tkwin)		(((Tk_FakeWin *) (tkwin))->visual)
#define Tk_WindowId(tkwin)		(((Tk_FakeWin *) (tkwin))->window)
#define Tk_PathName(tkwin) 		(((Tk_FakeWin *) (tkwin))->pathName)
#define Tk_Name(tkwin)			(((Tk_FakeWin *) (tkwin))->nameUid)
#define Tk_Class(tkwin) 		(((Tk_FakeWin *) (tkwin))->classUid)
#define Tk_X(tkwin)			(((Tk_FakeWin *) (tkwin))->changes.x)
#define Tk_Y(tkwin)			(((Tk_FakeWin *) (tkwin))->changes.y)
#define Tk_Width(tkwin)			(((Tk_FakeWin *) (tkwin))->changes.width)
#define Tk_Height(tkwin) \
    (((Tk_FakeWin *) (tkwin))->changes.height)
#define Tk_Changes(tkwin)		(&((Tk_FakeWin *) (tkwin))->changes)
#define Tk_Attributes(tkwin)		(&((Tk_FakeWin *) (tkwin))->atts)
#define Tk_IsMapped(tkwin) \
    (((Tk_FakeWin *) (tkwin))->flags & TK_MAPPED)
#define Tk_IsTopLevel(tkwin) \
    (((Tk_FakeWin *) (tkwin))->flags & TK_TOP_LEVEL)
#define Tk_ReqWidth(tkwin)		(((Tk_FakeWin *) (tkwin))->reqWidth)
#define Tk_ReqHeight(tkwin)		(((Tk_FakeWin *) (tkwin))->reqHeight)
#define Tk_InternalBorderWidth(tkwin) \
    (((Tk_FakeWin *) (tkwin))->internalBorderWidth)
#define Tk_Parent(tkwin)		(((Tk_FakeWin *) (tkwin))->parentPtr)
#define Tk_Colormap(tkwin)		(((Tk_FakeWin *) (tkwin))->atts.colormap)

/*
 * The structure below is needed by the macros above so that they can
 * access the fields of a Tk_Window.  The fields not needed by the macros
 * are declared as "dummyX".  The structure has its own type in order to
 * prevent applications from accessing Tk_Window fields except using
 * official macros.  WARNING!! The structure definition must be kept
 * consistent with the TkWindow structure in tkInt.h.  If you change one,
 * then change the other.  See the declaration in tkInt.h for
 * documentation on what the fields are used for internally.
 */

typedef struct Tk_FakeWin {
    Display *display;
    char *dummy1;
    int screenNum;
    Visual *visual;
    int depth;
    Window window;
    char *dummy2;
    char *dummy3;
    Tk_Window parentPtr;
    char *dummy4;
    char *dummy5;
    char *pathName;
    Tk_Uid nameUid;
    Tk_Uid classUid;
    XWindowChanges changes;
    unsigned int dummy6;
    XSetWindowAttributes atts;
    unsigned long dummy7;
    unsigned int flags;
    char *dummy8;
#ifdef TK_USE_INPUT_METHODS
    XIC dummy9;
#endif /* TK_USE_INPUT_METHODS */
    ClientData *dummy10;
    int dummy11;
    int dummy12;
    char *dummy13;
    char *dummy14;
    ClientData dummy15;
    int reqWidth, reqHeight;
    int internalBorderWidth;
    char *dummy16;
    char *dummy17;
} Tk_FakeWin;

/*
 * Flag values for TkWindow (and Tk_FakeWin) structures are:
 *
 * TK_MAPPED:			1 means window is currently mapped,
 *				0 means unmapped.
 * TK_TOP_LEVEL:		1 means this is a top-level window (it
 *				was or will be created as a child of
 *				a root window).
 * TK_ALREADY_DEAD:		1 means the window is in the process of
 *				being destroyed already.
 * TK_NEED_CONFIG_NOTIFY:	1 means that the window has been reconfigured
 *				before it was made to exist.  At the time of
 *				making it exist a ConfigureNotify event needs
 *				to be generated.
 * TK_GRAB_FLAG:		Used to manage grabs.  See tkGrab.c for
 *				details.
 * TK_CHECKED_IC:		1 means we've already tried to get an input
 *				context for this window;  if the ic field
 *				is NULL it means that there isn't a context
 *				for the field.
 * TK_PARENT_DESTROYED:		1 means that the window's parent has already
 *				been destroyed or is in the process of being
 *				destroyed.
 * TK_WM_COLORMAP_WINDOW:	1 means that this window has at some time
 *				appeared in the WM_COLORMAP_WINDOWS property
 *				for its toplevel, so we have to remove it
 *				from that property if the window is
 *				deleted and the toplevel isn't.
 */

#define TK_MAPPED		1
#define TK_TOP_LEVEL		2
#define TK_ALREADY_DEAD		4
#define TK_NEED_CONFIG_NOTIFY	8
#define TK_GRAB_FLAG		0x10
#define TK_CHECKED_IC		0x20
#define TK_PARENT_DESTROYED	0x40
#define TK_WM_COLORMAP_WINDOW	0x80

/*
 *--------------------------------------------------------------
 *
 * Procedure prototypes and structures used for defining new canvas
 * items:
 *
 *--------------------------------------------------------------
 */

/*
 * For each item in a canvas widget there exists one record with
 * the following structure.  Each actual item is represented by
 * a record with the following stuff at its beginning, plus additional
 * type-specific stuff after that.
 */

#define TK_TAG_SPACE 3

typedef struct Tk_Item  {
    int id;				/* Unique identifier for this item
					 * (also serves as first tag for
					 * item). */
    struct Tk_Item *nextPtr;		/* Next in display list of all
					 * items in this canvas.  Later items
					 * in list are drawn on top of earlier
					 * ones. */
    Tk_Uid staticTagSpace[TK_TAG_SPACE];/* Built-in space for limited # of
					 * tags. */
    Tk_Uid *tagPtr;			/* Pointer to array of tags.  Usually
					 * points to staticTagSpace, but
					 * may point to malloc-ed space if
					 * there are lots of tags. */
    int tagSpace;			/* Total amount of tag space available
					 * at tagPtr. */
    int numTags;			/* Number of tag slots actually used
					 * at *tagPtr. */
    struct Tk_ItemType *typePtr;	/* Table of procedures that implement
					 * this type of item. */
    int x1, y1, x2, y2;			/* Bounding box for item, in integer
					 * canvas units. Set by item-specific
					 * code and guaranteed to contain every
					 * pixel drawn in item.  Item area
					 * includes x1 and y1 but not x2
					 * and y2. */

    /*
     *------------------------------------------------------------------
     * Starting here is additional type-specific stuff;  see the
     * declarations for individual types to see what is part of
     * each type.  The actual space below is determined by the
     * "itemInfoSize" of the type's Tk_ItemType record.
     *------------------------------------------------------------------
     */
} Tk_Item;

/*
 * Records of the following type are used to describe a type of
 * item (e.g.  lines, circles, etc.) that can form part of a
 * canvas widget.
 */

typedef int	Tk_ItemCreateProc _ANSI_ARGS_((Tcl_Interp *interp,
		    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
		    char **argv));
typedef int	Tk_ItemConfigureProc _ANSI_ARGS_((Tcl_Interp *interp,
		    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
		    char **argv, int flags));
typedef int	Tk_ItemCoordProc _ANSI_ARGS_((Tcl_Interp *interp,
		    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
		    char **argv));
typedef void	Tk_ItemDeleteProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, Display *display));
typedef void	Tk_ItemDisplayProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, Display *display, Drawable dst,
		    int x, int y, int width, int height));
typedef double	Tk_ItemPointProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, double *pointPtr));
typedef int	Tk_ItemAreaProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, double *rectPtr));
typedef int	Tk_ItemPostscriptProc _ANSI_ARGS_((Tcl_Interp *interp,
		    Tk_Canvas canvas, Tk_Item *itemPtr, int prepass));
typedef void	Tk_ItemScaleProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, double originX, double originY,
		    double scaleX, double scaleY));
typedef void	Tk_ItemTranslateProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, double deltaX, double deltaY));
typedef int	Tk_ItemIndexProc _ANSI_ARGS_((Tcl_Interp *interp,
		    Tk_Canvas canvas, Tk_Item *itemPtr, char *indexString,
		    int *indexPtr));
typedef void	Tk_ItemCursorProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, int index));
typedef int	Tk_ItemSelectionProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, int offset, char *buffer,
		    int maxBytes));
typedef void	Tk_ItemInsertProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, int beforeThis, char *string));
typedef void	Tk_ItemDCharsProc _ANSI_ARGS_((Tk_Canvas canvas,
		    Tk_Item *itemPtr, int first, int last));

typedef struct Tk_ItemType {
    char *name;				/* The name of this type of item, such
					 * as "line". */
    int itemSize;			/* Total amount of space needed for
					 * item's record. */
    Tk_ItemCreateProc *createProc;	/* Procedure to create a new item of
					 * this type. */
    Tk_ConfigSpec *configSpecs;		/* Pointer to array of configuration
					 * specs for this type.  Used for
					 * returning configuration info. */
    Tk_ItemConfigureProc *configProc;	/* Procedure to call to change
					 * configuration options. */
    Tk_ItemCoordProc *coordProc;	/* Procedure to call to get and set
					 * the item's coordinates. */
    Tk_ItemDeleteProc *deleteProc;	/* Procedure to delete existing item of
					 * this type. */
    Tk_ItemDisplayProc *displayProc;	/* Procedure to display items of
					 * this type. */
    int alwaysRedraw;			/* Non-zero means displayProc should
					 * be called even when the item has
					 * been moved off-screen. */
    Tk_ItemPointProc *pointProc;	/* Computes distance from item to
					 * a given point. */
    Tk_ItemAreaProc *areaProc;		/* Computes whether item is inside,
					 * outside, or overlapping an area. */
    Tk_ItemPostscriptProc *postscriptProc;
					/* Procedure to write a Postscript
					 * description for items of this
					 * type. */
    Tk_ItemScaleProc *scaleProc;	/* Procedure to rescale items of
					 * this type. */
    Tk_ItemTranslateProc *translateProc;/* Procedure to translate items of
					 * this type. */
    Tk_ItemIndexProc *indexProc;	/* Procedure to determine index of
					 * indicated character.  NULL if
					 * item doesn't support indexing. */
    Tk_ItemCursorProc *icursorProc;	/* Procedure to set insert cursor pos.
					 * to just before a given position. */
    Tk_ItemSelectionProc *selectionProc;/* Procedure to return selection (in
					 * STRING format) when it is in this
					 * item. */
    Tk_ItemInsertProc *insertProc;	/* Procedure to insert something into
					 * an item. */
    Tk_ItemDCharsProc *dCharsProc;	/* Procedure to delete characters
					 * from an item. */
    struct Tk_ItemType *nextPtr;	/* Used to link types together into
					 * a list. */
} Tk_ItemType;

/*
 * The following structure provides information about the selection and
 * the insertion cursor.  It is needed by only a few items, such as
 * those that display text.  It is shared by the generic canvas code
 * and the item-specific code, but most of the fields should be written
 * only by the canvas generic code.
 */

typedef struct Tk_CanvasTextInfo {
    Tk_3DBorder selBorder;	/* Border and background for selected
				 * characters.  Read-only to items.*/
    int selBorderWidth;		/* Width of border around selection. 
				 * Read-only to items. */
    XColor *selFgColorPtr;	/* Foreground color for selected text.
				 * Read-only to items. */
    Tk_Item *selItemPtr;	/* Pointer to selected item.  NULL means
				 * selection isn't in this canvas.
				 * Writable by items. */
    int selectFirst;		/* Index of first selected character. 
				 * Writable by items. */
    int selectLast;		/* Index of last selected character. 
				 * Writable by items. */
    Tk_Item *anchorItemPtr;	/* Item corresponding to "selectAnchor":
				 * not necessarily selItemPtr.   Read-only
				 * to items. */
    int selectAnchor;		/* Fixed end of selection (i.e. "select to"
				 * operation will use this as one end of the
				 * selection).  Writable by items. */
    Tk_3DBorder insertBorder;	/* Used to draw vertical bar for insertion
				 * cursor.  Read-only to items. */
    int insertWidth;		/* Total width of insertion cursor.  Read-only
				 * to items. */
    int insertBorderWidth;	/* Width of 3-D border around insert cursor.
				 * Read-only to items. */
    Tk_Item *focusItemPtr;	/* Item that currently has the input focus,
				 * or NULL if no such item.  Read-only to
				 * items.  */
    int gotFocus;		/* Non-zero means that the canvas widget has
				 * the input focus.  Read-only to items.*/
    int cursorOn;		/* Non-zero means that an insertion cursor
				 * should be displayed in focusItemPtr.
				 * Read-only to items.*/
} Tk_CanvasTextInfo;

/*
 *--------------------------------------------------------------
 *
 * Procedure prototypes and structures used for managing images:
 *
 *--------------------------------------------------------------
 */

typedef struct Tk_ImageType Tk_ImageType;
typedef int (Tk_ImageCreateProc) _ANSI_ARGS_((Tcl_Interp *interp,
	char *name, int argc, char **argv, Tk_ImageType *typePtr,
	Tk_ImageMaster master, ClientData *masterDataPtr));
typedef ClientData (Tk_ImageGetProc) _ANSI_ARGS_((Tk_Window tkwin,
	ClientData masterData));
typedef void (Tk_ImageDisplayProc) _ANSI_ARGS_((ClientData instanceData,
	Display *display, Drawable drawable, int imageX, int imageY,
	int width, int height, int drawableX, int drawableY));
typedef void (Tk_ImageFreeProc) _ANSI_ARGS_((ClientData instanceData,
	Display *display));
typedef void (Tk_ImageDeleteProc) _ANSI_ARGS_((ClientData masterData));
typedef void (Tk_ImageChangedProc) _ANSI_ARGS_((ClientData clientData,
	int x, int y, int width, int height, int imageWidth,
	int imageHeight));

/*
 * The following structure represents a particular type of image
 * (bitmap, xpm image, etc.).  It provides information common to
 * all images of that type, such as the type name and a collection
 * of procedures in the image manager that respond to various
 * events.  Each image manager is represented by one of these
 * structures.
 */

struct Tk_ImageType {
    char *name;			/* Name of image type. */
    Tk_ImageCreateProc *createProc;
				/* Procedure to call to create a new image
				 * of this type. */
    Tk_ImageGetProc *getProc;	/* Procedure to call the first time
				 * Tk_GetImage is called in a new way
				 * (new visual or screen). */
    Tk_ImageDisplayProc *displayProc;
				/* Call to draw image, in response to
				 * Tk_RedrawImage calls. */
    Tk_ImageFreeProc *freeProc;	/* Procedure to call whenever Tk_FreeImage
				 * is called to release an instance of an
				 * image. */
    Tk_ImageDeleteProc *deleteProc;
				/* Procedure to call to delete image.  It
				 * will not be called until after freeProc
				 * has been called for each instance of the
				 * image. */
    struct Tk_ImageType *nextPtr;
				/* Next in list of all image types currently
				 * known.  Filled in by Tk, not by image
				 * manager. */
};

/*
 *--------------------------------------------------------------
 *
 * Additional definitions used to manage images of type "photo".
 *
 *--------------------------------------------------------------
 */

/*
 * The following type is used to identify a particular photo image
 * to be manipulated:
 */

typedef void *Tk_PhotoHandle;

/*
 * The following structure describes a block of pixels in memory:
 */

typedef struct Tk_PhotoImageBlock {
    unsigned char *pixelPtr;	/* Pointer to the first pixel. */
    int		width;		/* Width of block, in pixels. */
    int		height;		/* Height of block, in pixels. */
    int		pitch;		/* Address difference between corresponding
				 * pixels in successive lines. */
    int		pixelSize;	/* Address difference between successive
				 * pixels in the same line. */
    int		offset[3];	/* Address differences between the red, green
				 * and blue components of the pixel and the
				 * pixel as a whole. */
} Tk_PhotoImageBlock;

/*
 * Procedure prototypes and structures used in reading and
 * writing photo images:
 */

typedef struct Tk_PhotoImageFormat Tk_PhotoImageFormat;
typedef int (Tk_ImageFileMatchProc) _ANSI_ARGS_((FILE *f, char *fileName,
	char *formatString, int *widthPtr, int *heightPtr));
typedef int (Tk_ImageStringMatchProc) _ANSI_ARGS_((char *string,
	char *formatString, int *widthPtr, int *heightPtr));
typedef int (Tk_ImageFileReadProc) _ANSI_ARGS_((Tcl_Interp *interp,
	FILE *f, char *fileName, char *formatString, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY));
typedef int (Tk_ImageStringReadProc) _ANSI_ARGS_((Tcl_Interp *interp,
	char *string, char *formatString, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY));
typedef int (Tk_ImageFileWriteProc) _ANSI_ARGS_((Tcl_Interp *interp,
	char *fileName, char *formatString, Tk_PhotoImageBlock *blockPtr));
typedef int (Tk_ImageStringWriteProc) _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_DString *dataPtr, char *formatString,
	Tk_PhotoImageBlock *blockPtr));

/*
 * The following structure represents a particular file format for
 * storing images (e.g., PPM, GIF, JPEG, etc.).  It provides information
 * to allow image files of that format to be recognized and read into
 * a photo image.
 */

struct Tk_PhotoImageFormat {
    char *name;			/* Name of image file format */
    Tk_ImageFileMatchProc *fileMatchProc;
				/* Procedure to call to determine whether
				 * an image file matches this format. */
    Tk_ImageStringMatchProc *stringMatchProc;
				/* Procedure to call to determine whether
				 * the data in a string matches this format. */
    Tk_ImageFileReadProc *fileReadProc;
				/* Procedure to call to read data from
				 * an image file into a photo image. */
    Tk_ImageStringReadProc *stringReadProc;
				/* Procedure to call to read data from
				 * a string into a photo image. */
    Tk_ImageFileWriteProc *fileWriteProc;
				/* Procedure to call to write data from
				 * a photo image to a file. */
    Tk_ImageStringWriteProc *stringWriteProc;
				/* Procedure to call to obtain a string
				 * representation of the data in a photo
				 * image.*/
    struct Tk_PhotoImageFormat *nextPtr;
				/* Next in list of all photo image formats
				 * currently known.  Filled in by Tk, not
				 * by image format handler. */
};

/*
 *--------------------------------------------------------------
 *
 * The definitions below provide backward compatibility for
 * functions and types related to event handling that used to
 * be in Tk but have moved to Tcl.
 *
 *--------------------------------------------------------------
 */

#define TK_READABLE		TCL_READABLE
#define TK_WRITABLE		TCL_WRITABLE
#define TK_EXCEPTION		TCL_EXCEPTION

#define TK_DONT_WAIT		TCL_DONT_WAIT
#define TK_X_EVENTS		TCL_WINDOW_EVENTS
#define TK_WINDOW_EVENTS	TCL_WINDOW_EVENTS
#define TK_FILE_EVENTS		TCL_FILE_EVENTS
#define TK_TIMER_EVENTS		TCL_TIMER_EVENTS
#define TK_IDLE_EVENTS		TCL_IDLE_EVENTS
#define TK_ALL_EVENTS		TCL_ALL_EVENTS

#define Tk_IdleProc		Tcl_IdleProc
#define Tk_FileProc		Tcl_FileProc
#define Tk_TimerProc		Tcl_TimerProc
#define Tk_TimerToken		Tcl_TimerToken

#define Tk_BackgroundError	Tcl_BackgroundError
#define Tk_CancelIdleCall	Tcl_CancelIdleCall
#define Tk_CreateFileHandler(file,mask,proc,data) \
    Tcl_CreateFileHandler(Tcl_GetFile((file),TCL_UNIX_FD),(mask),(proc),(data))
#define Tk_CreateTimerHandler	Tcl_CreateTimerHandler
#define Tk_DeleteFileHandler(file) \
    Tcl_DeleteFileHandler(Tcl_GetFile((file),TCL_UNIX_FD))
#define Tk_DeleteTimerHandler	Tcl_DeleteTimerHandler
#define Tk_DoOneEvent		Tcl_DoOneEvent
#define Tk_DoWhenIdle		Tcl_DoWhenIdle
#define Tk_Sleep		Tcl_Sleep

/* Additional stuff that has moved to Tcl: */

#define Tk_AfterCmd		Tcl_AfterCmd
#define Tk_EventuallyFree	Tcl_EventuallyFree
#define Tk_FreeProc		Tcl_FreeProc
#define Tk_Preserve		Tcl_Preserve
#define Tk_Release		Tcl_Release

/*
 *--------------------------------------------------------------
 *
 * Additional procedure types defined by Tk.
 *
 *--------------------------------------------------------------
 */

typedef int (Tk_ErrorProc) _ANSI_ARGS_((ClientData clientData,
	XErrorEvent *errEventPtr));
typedef void (Tk_EventProc) _ANSI_ARGS_((ClientData clientData,
	XEvent *eventPtr));
typedef int (Tk_GenericProc) _ANSI_ARGS_((ClientData clientData,
	XEvent *eventPtr));
typedef int (Tk_GetSelProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, char *portion));
typedef void (Tk_LostSelProc) _ANSI_ARGS_((ClientData clientData));
typedef Tk_RestrictAction (Tk_RestrictProc) _ANSI_ARGS_((
	ClientData clientData, XEvent *eventPtr));
typedef int (Tk_SelectionProc) _ANSI_ARGS_((ClientData clientData,
	int offset, char *buffer, int maxBytes));

/*
 *--------------------------------------------------------------
 *
 * Exported procedures and variables.
 *
 *--------------------------------------------------------------
 */

EXTERN XColor *		Tk_3DBorderColor _ANSI_ARGS_((Tk_3DBorder border));
EXTERN GC		Tk_3DBorderGC _ANSI_ARGS_((Tk_Window tkwin,
			    Tk_3DBorder border, int which));
EXTERN void		Tk_3DHorizontalBevel _ANSI_ARGS_((Tk_Window tkwin,
			    Drawable drawable, Tk_3DBorder border, int x,
			    int y, int width, int height, int leftIn,
			    int rightIn, int topBevel, int relief));
EXTERN void		Tk_3DVerticalBevel _ANSI_ARGS_((Tk_Window tkwin,
			    Drawable drawable, Tk_3DBorder border, int x,
			    int y, int width, int height, int leftBevel,
			    int relief));
EXTERN void		Tk_AddOption _ANSI_ARGS_((Tk_Window tkwin, char *name,
			    char *value, int priority));
EXTERN void		Tk_BindEvent _ANSI_ARGS_((Tk_BindingTable bindingTable,
			    XEvent *eventPtr, Tk_Window tkwin, int numObjects,
			    ClientData *objectPtr));
EXTERN void		Tk_CanvasDrawableCoords _ANSI_ARGS_((Tk_Canvas canvas,
			    double x, double y, short *drawableXPtr,
			    short *drawableYPtr));
EXTERN void		Tk_CanvasEventuallyRedraw _ANSI_ARGS_((
			    Tk_Canvas canvas, int x1, int y1, int x2,
			    int y2));
EXTERN int		Tk_CanvasGetCoord _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, char *string,
			    double *doublePtr));
EXTERN Tk_CanvasTextInfo *Tk_CanvasGetTextInfo _ANSI_ARGS_((Tk_Canvas canvas));
EXTERN int		Tk_CanvasPsBitmap _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Pixmap bitmap, int x, int y,
			    int width, int height));
EXTERN int		Tk_CanvasPsColor _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, XColor *colorPtr));
EXTERN int		Tk_CanvasPsFont _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, XFontStruct *fontStructPtr));
EXTERN void		Tk_CanvasPsPath _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, double *coordPtr, int numPoints));
EXTERN int		Tk_CanvasPsStipple _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Pixmap bitmap));
EXTERN double		Tk_CanvasPsY _ANSI_ARGS_((Tk_Canvas canvas, double y));
EXTERN void		Tk_CanvasSetStippleOrigin _ANSI_ARGS_((
			    Tk_Canvas canvas, GC gc));
EXTERN int		Tk_CanvasTagsParseProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    Tk_Window tkwin, char *value, char *widgRec,
			    int offset));
EXTERN char *		Tk_CanvasTagsPrintProc _ANSI_ARGS_((
			    ClientData clientData, Tk_Window tkwin,
			    char *widgRec, int offset,
			    Tcl_FreeProc **freeProcPtr));
EXTERN Tk_Window	Tk_CanvasTkwin _ANSI_ARGS_((Tk_Canvas canvas));
EXTERN void		Tk_CanvasWindowCoords _ANSI_ARGS_((Tk_Canvas canvas,
			    double x, double y, short *screenXPtr,
			    short *screenYPtr));
EXTERN void		Tk_ChangeWindowAttributes _ANSI_ARGS_((Tk_Window tkwin,
			    unsigned long valueMask,
			    XSetWindowAttributes *attsPtr));
EXTERN void		Tk_ClearSelection _ANSI_ARGS_((Tk_Window tkwin,
			    Atom selection));
EXTERN int		Tk_ClipboardAppend _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Atom target, Atom format,
			    char* buffer));
EXTERN int		Tk_ClipboardClear _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin));
EXTERN int		Tk_ConfigureInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_ConfigSpec *specs,
			    char *widgRec, char *argvName, int flags));
EXTERN int		Tk_ConfigureValue _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_ConfigSpec *specs,
			    char *widgRec, char *argvName, int flags));
EXTERN int		Tk_ConfigureWidget _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_ConfigSpec *specs,
			    int argc, char **argv, char *widgRec,
			    int flags));
EXTERN void		Tk_ConfigureWindow _ANSI_ARGS_((Tk_Window tkwin,
			    unsigned int valueMask, XWindowChanges *valuePtr));
EXTERN Tk_Window	Tk_CoordsToWindow _ANSI_ARGS_((int rootX, int rootY,
			    Tk_Window tkwin));
EXTERN unsigned long	Tk_CreateBinding _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_BindingTable bindingTable, ClientData object,
			    char *eventString, char *command, int append));
EXTERN Tk_BindingTable	Tk_CreateBindingTable _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Tk_ErrorHandler	Tk_CreateErrorHandler _ANSI_ARGS_((Display *display,
			    int errNum, int request, int minorCode,
			    Tk_ErrorProc *errorProc, ClientData clientData));
EXTERN void		Tk_CreateEventHandler _ANSI_ARGS_((Tk_Window token,
			    unsigned long mask, Tk_EventProc *proc,
			    ClientData clientData));
EXTERN void		Tk_CreateGenericHandler _ANSI_ARGS_((
			    Tk_GenericProc *proc, ClientData clientData));
EXTERN void		Tk_CreateImageType _ANSI_ARGS_((
			    Tk_ImageType *typePtr));
EXTERN void		Tk_CreateItemType _ANSI_ARGS_((Tk_ItemType *typePtr));
EXTERN void		Tk_CreatePhotoImageFormat _ANSI_ARGS_((
			    Tk_PhotoImageFormat *formatPtr));
EXTERN void		Tk_CreateSelHandler _ANSI_ARGS_((Tk_Window tkwin,
			    Atom selection, Atom target,
			    Tk_SelectionProc *proc, ClientData clientData,
			    Atom format));
EXTERN Tk_Window	Tk_CreateWindow _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window parent, char *name, char *screenName));
EXTERN Tk_Window	Tk_CreateWindowFromPath _ANSI_ARGS_((
			    Tcl_Interp *interp, Tk_Window tkwin,
			    char *pathName, char *screenName));
EXTERN int		Tk_DefineBitmap _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Uid name, char *source, int width,
			    int height));
EXTERN void		Tk_DefineCursor _ANSI_ARGS_((Tk_Window window,
			    Tk_Cursor cursor));
EXTERN void		Tk_DeleteAllBindings _ANSI_ARGS_((
			    Tk_BindingTable bindingTable, ClientData object));
EXTERN int		Tk_DeleteBinding _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_BindingTable bindingTable, ClientData object,
			    char *eventString));
EXTERN void		Tk_DeleteBindingTable _ANSI_ARGS_((
			    Tk_BindingTable bindingTable));
EXTERN void		Tk_DeleteErrorHandler _ANSI_ARGS_((
			    Tk_ErrorHandler handler));
EXTERN void		Tk_DeleteEventHandler _ANSI_ARGS_((Tk_Window token,
			    unsigned long mask, Tk_EventProc *proc,
			    ClientData clientData));
EXTERN void		Tk_DeleteGenericHandler _ANSI_ARGS_((
			    Tk_GenericProc *proc, ClientData clientData));
EXTERN void		Tk_DeleteImage _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name));
EXTERN void		Tk_DeleteSelHandler _ANSI_ARGS_((Tk_Window tkwin,
			    Atom selection, Atom target));
EXTERN void		Tk_DestroyWindow _ANSI_ARGS_((Tk_Window tkwin));
EXTERN char *		Tk_DisplayName _ANSI_ARGS_((Tk_Window tkwin));
EXTERN void		Tk_Draw3DPolygon _ANSI_ARGS_((Tk_Window tkwin,
			    Drawable drawable, Tk_3DBorder border,
			    XPoint *pointPtr, int numPoints, int borderWidth,
			    int leftRelief));
EXTERN void		Tk_Draw3DRectangle _ANSI_ARGS_((Tk_Window tkwin,
			    Drawable drawable, Tk_3DBorder border, int x,
			    int y, int width, int height, int borderWidth,
			    int relief));
EXTERN void		Tk_DrawFocusHighlight _ANSI_ARGS_((Tk_Window tkwin,
			    GC gc, int width, Drawable drawable));
EXTERN void		Tk_Fill3DPolygon _ANSI_ARGS_((Tk_Window tkwin,
			    Drawable drawable, Tk_3DBorder border,
			    XPoint *pointPtr, int numPoints, int borderWidth,
			    int leftRelief));
EXTERN void		Tk_Fill3DRectangle _ANSI_ARGS_((Tk_Window tkwin,
			    Drawable drawable, Tk_3DBorder border, int x,
			    int y, int width, int height, int borderWidth,
			    int relief));
EXTERN Tk_PhotoHandle	Tk_FindPhoto _ANSI_ARGS_((char *imageName));
EXTERN void		Tk_Free3DBorder _ANSI_ARGS_((Tk_3DBorder border));
EXTERN void		Tk_FreeBitmap _ANSI_ARGS_((Display *display,
			    Pixmap bitmap));
EXTERN void		Tk_FreeColor _ANSI_ARGS_((XColor *colorPtr));
EXTERN void		Tk_FreeColormap _ANSI_ARGS_((Display *display,
			    Colormap colormap));
EXTERN void		Tk_FreeCursor _ANSI_ARGS_((Display *display,
			    Tk_Cursor cursor));
EXTERN void		Tk_FreeFontStruct _ANSI_ARGS_((
			    XFontStruct *fontStructPtr));
EXTERN void		Tk_FreeGC _ANSI_ARGS_((Display *display, GC gc));
EXTERN void		Tk_FreeImage _ANSI_ARGS_((Tk_Image image));
EXTERN void		Tk_FreeOptions _ANSI_ARGS_((Tk_ConfigSpec *specs,
			    char *widgRec, Display *display, int needFlags));
EXTERN void		Tk_FreePixmap _ANSI_ARGS_((Display *display,
			    Pixmap pixmap));
EXTERN void		Tk_FreeXId _ANSI_ARGS_((Display *display, XID xid));
EXTERN GC		Tk_GCForColor _ANSI_ARGS_((XColor *colorPtr,
			    Drawable drawable));
EXTERN void		Tk_GeometryRequest _ANSI_ARGS_((Tk_Window tkwin,
			    int reqWidth,  int reqHeight));
EXTERN Tk_3DBorder	Tk_Get3DBorder _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_Uid colorName));
EXTERN void		Tk_GetAllBindings _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_BindingTable bindingTable, ClientData object));
EXTERN int		Tk_GetAnchor _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Tk_Anchor *anchorPtr));
EXTERN char *		Tk_GetAtomName _ANSI_ARGS_((Tk_Window tkwin,
			    Atom atom));
EXTERN char *		Tk_GetBinding _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_BindingTable bindingTable, ClientData object,
			    char *eventString));
EXTERN Pixmap		Tk_GetBitmap _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_Uid string));
EXTERN Pixmap		Tk_GetBitmapFromData _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *source,
			    int width, int height));
EXTERN int		Tk_GetCapStyle _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *capPtr));
EXTERN XColor *		Tk_GetColor _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_Uid name));
EXTERN XColor *		Tk_GetColorByValue _ANSI_ARGS_((Tk_Window tkwin,
			    XColor *colorPtr));
EXTERN Colormap		Tk_GetColormap _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *string));
EXTERN Tk_Cursor	Tk_GetCursor _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_Uid string));
EXTERN Tk_Cursor	Tk_GetCursorFromData _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *source, char *mask,
			    int width, int height, int xHot, int yHot,
			    Tk_Uid fg, Tk_Uid bg));
EXTERN XFontStruct *	Tk_GetFontStruct _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Tk_Uid name));
EXTERN GC		Tk_GetGC _ANSI_ARGS_((Tk_Window tkwin,
			    unsigned long valueMask, XGCValues *valuePtr));
EXTERN Tk_Image		Tk_GetImage _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *name,
			    Tk_ImageChangedProc *changeProc,
			    ClientData clientData));
EXTERN Tk_ItemType *	Tk_GetItemTypes _ANSI_ARGS_((void));
EXTERN int		Tk_GetJoinStyle _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *joinPtr));
EXTERN int		Tk_GetJustify _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Tk_Justify *justifyPtr));
EXTERN int		Tk_GetNumMainWindows _ANSI_ARGS_((void));
EXTERN Tk_Uid		Tk_GetOption _ANSI_ARGS_((Tk_Window tkwin, char *name,
			    char *className));
EXTERN int		Tk_GetPixels _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *string, int *intPtr));
EXTERN Pixmap		Tk_GetPixmap _ANSI_ARGS_((Display *display, Drawable d,
			    int width, int height, int depth));
EXTERN int		Tk_GetRelief _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, int *reliefPtr));
EXTERN void		Tk_GetRootCoords _ANSI_ARGS_ ((Tk_Window tkwin,
			    int *xPtr, int *yPtr));
EXTERN int		Tk_GetScrollInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, double *dblPtr,
			    int *intPtr));
EXTERN int		Tk_GetScreenMM _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *string, double *doublePtr));
EXTERN int		Tk_GetSelection _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, Atom selection, Atom target,
			    Tk_GetSelProc *proc, ClientData clientData));
EXTERN Tk_Uid		Tk_GetUid _ANSI_ARGS_((char *string));
EXTERN Visual *		Tk_GetVisual _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *string, int *depthPtr,
			    Colormap *colormapPtr));
EXTERN void		Tk_GetVRootGeometry _ANSI_ARGS_((Tk_Window tkwin,
			    int *xPtr, int *yPtr, int *widthPtr,
			    int *heightPtr));
EXTERN int		Tk_Grab _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, int grabGlobal));
EXTERN void		Tk_HandleEvent _ANSI_ARGS_((XEvent *eventPtr));
EXTERN Tk_Window      	Tk_IdToWindow _ANSI_ARGS_((Display *display,
			    Window window));
EXTERN void		Tk_ImageChanged _ANSI_ARGS_((
			    Tk_ImageMaster master, int x, int y,
			    int width, int height, int imageWidth,
			    int imageHeight));
EXTERN int		Tk_Init _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Atom		Tk_InternAtom _ANSI_ARGS_((Tk_Window tkwin,
			    char *name));
EXTERN void		Tk_Main _ANSI_ARGS_((int argc, char **argv,
			    Tcl_AppInitProc *appInitProc));
EXTERN void		Tk_MainLoop _ANSI_ARGS_((void));
EXTERN void		Tk_MaintainGeometry _ANSI_ARGS_((Tk_Window slave,
			    Tk_Window master, int x, int y, int width,
			    int height));
EXTERN Tk_Window	Tk_MainWindow _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tk_MakeWindowExist _ANSI_ARGS_((Tk_Window tkwin));
EXTERN void		Tk_ManageGeometry _ANSI_ARGS_((Tk_Window tkwin,
			    Tk_GeomMgr *mgrPtr, ClientData clientData));
EXTERN void		Tk_MapWindow _ANSI_ARGS_((Tk_Window tkwin));
EXTERN void		Tk_MoveResizeWindow _ANSI_ARGS_((Tk_Window tkwin,
			    int x, int y, int width, int height));
EXTERN void		Tk_MoveWindow _ANSI_ARGS_((Tk_Window tkwin, int x,
			    int y));
EXTERN void		Tk_MoveToplevelWindow _ANSI_ARGS_((Tk_Window tkwin,
			    int x, int y));
EXTERN char *		Tk_NameOf3DBorder _ANSI_ARGS_((Tk_3DBorder border));
EXTERN char *		Tk_NameOfAnchor _ANSI_ARGS_((Tk_Anchor anchor));
EXTERN char *		Tk_NameOfBitmap _ANSI_ARGS_((Display *display,
			    Pixmap bitmap));
EXTERN char *		Tk_NameOfCapStyle _ANSI_ARGS_((int cap));
EXTERN char *		Tk_NameOfColor _ANSI_ARGS_((XColor *colorPtr));
EXTERN char *		Tk_NameOfCursor _ANSI_ARGS_((Display *display,
			    Tk_Cursor cursor));
EXTERN char *		Tk_NameOfFontStruct _ANSI_ARGS_((
			    XFontStruct *fontStructPtr));
EXTERN char *		Tk_NameOfImage _ANSI_ARGS_((
			    Tk_ImageMaster imageMaster));
EXTERN char *		Tk_NameOfJoinStyle _ANSI_ARGS_((int join));
EXTERN char *		Tk_NameOfJustify _ANSI_ARGS_((Tk_Justify justify));
EXTERN char *		Tk_NameOfRelief _ANSI_ARGS_((int relief));
EXTERN Tk_Window	Tk_NameToWindow _ANSI_ARGS_((Tcl_Interp *interp,
			    char *pathName, Tk_Window tkwin));
EXTERN void		Tk_OwnSelection _ANSI_ARGS_((Tk_Window tkwin,
			    Atom selection, Tk_LostSelProc *proc,
			    ClientData clientData));
EXTERN int		Tk_ParseArgv _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, int *argcPtr, char **argv,
			    Tk_ArgvInfo *argTable, int flags));
EXTERN void		Tk_PhotoPutBlock _ANSI_ARGS_((Tk_PhotoHandle handle,
			    Tk_PhotoImageBlock *blockPtr, int x, int y,
			    int width, int height));
EXTERN void		Tk_PhotoPutZoomedBlock _ANSI_ARGS_((
			    Tk_PhotoHandle handle,
			    Tk_PhotoImageBlock *blockPtr, int x, int y,
			    int width, int height, int zoomX, int zoomY,
			    int subsampleX, int subsampleY));
EXTERN int		Tk_PhotoGetImage _ANSI_ARGS_((Tk_PhotoHandle handle,
			    Tk_PhotoImageBlock *blockPtr));
EXTERN void		Tk_PhotoBlank _ANSI_ARGS_((Tk_PhotoHandle handle));
EXTERN void		Tk_PhotoExpand _ANSI_ARGS_((Tk_PhotoHandle handle,
			    int width, int height ));
EXTERN void		Tk_PhotoGetSize _ANSI_ARGS_((Tk_PhotoHandle handle,
			    int *widthPtr, int *heightPtr));
EXTERN void		Tk_PhotoSetSize _ANSI_ARGS_((Tk_PhotoHandle handle,
			    int width, int height));
EXTERN void		Tk_PreserveColormap _ANSI_ARGS_((Display *display,
			    Colormap colormap));
EXTERN void		Tk_QueueWindowEvent _ANSI_ARGS_((XEvent *eventPtr,
			    Tcl_QueuePosition position));
EXTERN void		Tk_RedrawImage _ANSI_ARGS_((Tk_Image image, int imageX,
			    int imageY, int width, int height,
			    Drawable drawable, int drawableX, int drawableY));
EXTERN void		Tk_ResizeWindow _ANSI_ARGS_((Tk_Window tkwin,
			    int width, int height));
EXTERN int		Tk_RestackWindow _ANSI_ARGS_((Tk_Window tkwin,
			    int aboveBelow, Tk_Window other));
EXTERN Tk_RestrictProc *Tk_RestrictEvents _ANSI_ARGS_((Tk_RestrictProc *proc,
			    ClientData arg, ClientData *prevArgPtr));
EXTERN char *		Tk_SetAppName _ANSI_ARGS_((Tk_Window tkwin,
			    char *name));
EXTERN void		Tk_SetBackgroundFromBorder _ANSI_ARGS_((
			    Tk_Window tkwin, Tk_3DBorder border));
EXTERN void		Tk_SetClass _ANSI_ARGS_((Tk_Window tkwin,
			    char *className));
EXTERN void		Tk_SetGrid _ANSI_ARGS_((Tk_Window tkwin,
			    int reqWidth, int reqHeight, int gridWidth,
			    int gridHeight));
EXTERN void		Tk_SetInternalBorder _ANSI_ARGS_((Tk_Window tkwin,
			    int width));
EXTERN void		Tk_SetWindowBackground _ANSI_ARGS_((Tk_Window tkwin,
			    unsigned long pixel));
EXTERN void		Tk_SetWindowBackgroundPixmap _ANSI_ARGS_((
			    Tk_Window tkwin, Pixmap pixmap));
EXTERN void		Tk_SetWindowBorder _ANSI_ARGS_((Tk_Window tkwin,
			    unsigned long pixel));
EXTERN void		Tk_SetWindowBorderWidth _ANSI_ARGS_((Tk_Window tkwin,
			    int width));
EXTERN void		Tk_SetWindowBorderPixmap _ANSI_ARGS_((Tk_Window tkwin,
			    Pixmap pixmap));
EXTERN void		Tk_SetWindowColormap _ANSI_ARGS_((Tk_Window tkwin,
			    Colormap colormap));
EXTERN int		Tk_SetWindowVisual _ANSI_ARGS_((Tk_Window tkwin,
			    Visual *visual, int depth,
			    Colormap colormap));
EXTERN void		Tk_SizeOfBitmap _ANSI_ARGS_((Display *display,
			    Pixmap bitmap, int *widthPtr,
			    int *heightPtr));
EXTERN void		Tk_SizeOfImage _ANSI_ARGS_((Tk_Image image,
			    int *widthPtr, int *heightPtr));
EXTERN int		Tk_StrictMotif _ANSI_ARGS_((Tk_Window tkwin));
EXTERN void		Tk_UndefineCursor _ANSI_ARGS_((Tk_Window window));
EXTERN void		Tk_Ungrab _ANSI_ARGS_((Tk_Window tkwin));
EXTERN void		Tk_UnmaintainGeometry _ANSI_ARGS_((Tk_Window slave,
			    Tk_Window master));
EXTERN void		Tk_UnmapWindow _ANSI_ARGS_((Tk_Window tkwin));
EXTERN void		Tk_UnsetGrid _ANSI_ARGS_((Tk_Window tkwin));

/*
 * Tcl commands exported by Tk:
 */

EXTERN int		Tk_AfterCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_BellCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_BindCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_BindtagsCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_ButtonCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_CanvasCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_CheckbuttonCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_ClipboardCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_DestroyCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_EntryCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_FileeventCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_FrameCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_FocusCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_GrabCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_GridCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_ImageCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_LabelCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_ListboxCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_LowerCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_MenuCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_MenubuttonCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_MessageCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_OptionCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_PackCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_PlaceCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_RadiobuttonCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_RaiseCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_ScaleCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_ScrollbarCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_SelectionCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_SendCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_TextCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_TkCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_TkwaitCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_ToplevelCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_UpdateCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_WinfoCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Tk_WmCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));

#endif /* _TK */
