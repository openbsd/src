/*
 * tkText.h --
 *
 *	Declarations shared among the files that implement text
 *	widgets.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkText.h 1.44 96/02/15 18:51:31
 */

#ifndef _TKTEXT
#define _TKTEXT

#ifndef _TK
#include "tk.h"
#endif

/*
 * Opaque types for structures whose guts are only needed by a single
 * file:
 */

typedef struct TkTextBTree *TkTextBTree;

/*
 * The data structure below defines a single line of text (from newline
 * to newline, not necessarily what appears on one line of the screen).
 */

typedef struct TkTextLine {
    struct Node *parentPtr;		/* Pointer to parent node containing
					 * line. */
    struct TkTextLine *nextPtr;		/* Next in linked list of lines with
					 * same parent node in B-tree.  NULL
					 * means end of list. */
    struct TkTextSegment *segPtr;	/* First in ordered list of segments
					 * that make up the line. */
} TkTextLine;

/*
 * -----------------------------------------------------------------------
 * Segments: each line is divided into one or more segments, where each
 * segment is one of several things, such as a group of characters, a
 * tag toggle, a mark, or an embedded widget.  Each segment starts with
 * a standard header followed by a body that varies from type to type.
 * -----------------------------------------------------------------------
 */

/*
 * The data structure below defines the body of a segment that represents
 * a tag toggle.  There is one of these structures at both the beginning
 * and end of each tagged range.
 */

typedef struct TkTextToggle {
    struct TkTextTag *tagPtr;		/* Tag that starts or ends here. */
    int inNodeCounts;			/* 1 means this toggle has been
					 * accounted for in node toggle
					 * counts; 0 means it hasn't, yet. */
} TkTextToggle;

/*
 * The data structure below defines line segments that represent
 * marks.  There is one of these for each mark in the text.
 */

typedef struct TkTextMark {
    struct TkText *textPtr;		/* Overall information about text
					 * widget. */
    TkTextLine *linePtr;		/* Line structure that contains the
					 * segment. */
    Tcl_HashEntry *hPtr;		/* Pointer to hash table entry for mark
					 * (in textPtr->markTable). */
} TkTextMark;

/*
 * A structure of the following type holds information for each window
 * embedded in a text widget.  This information is only used by the
 * file tkTextWind.c
 */

typedef struct TkTextEmbWindow {
    struct TkText *textPtr;		/* Information about the overall text
					 * widget. */
    TkTextLine *linePtr;		/* Line structure that contains this
					 * window. */
    Tk_Window tkwin;			/* Window for this segment.  NULL
					 * means that the window hasn't
					 * been created yet. */
    char *create;			/* Script to create window on-demand.
					 * NULL means no such script.
					 * Malloc-ed. */
    int align;				/* How to align window in vertical
					 * space.  See definitions in
					 * tkTextWind.c. */
    int padX, padY;			/* Padding to leave around each side
					 * of window, in pixels. */
    int stretch;			/* Should window stretch to fill
					 * vertical space of line (except for
					 * pady)?  0 or 1. */
    int chunkCount;			/* Number of display chunks that
					 * refer to this window. */
    int displayed;			/* Non-zero means that the window
					 * has been displayed on the screen
					 * recently. */
} TkTextEmbWindow;

/*
 * The data structure below defines line segments.
 */

typedef struct TkTextSegment {
    struct Tk_SegType *typePtr;		/* Pointer to record describing
					 * segment's type. */
    struct TkTextSegment *nextPtr;	/* Next in list of segments for this
					 * line, or NULL for end of list. */
    int size;				/* Size of this segment (# of bytes
					 * of index space it occupies). */
    union {
	char chars[4];			/* Characters that make up character
					 * info.  Actual length varies to
					 * hold as many characters as needed.*/
	TkTextToggle toggle;		/* Information about tag toggle. */
	TkTextMark mark;		/* Information about mark. */
	TkTextEmbWindow ew;		/* Information about embedded
					 * window. */
    } body;
} TkTextSegment;

/*
 * Data structures of the type defined below are used during the
 * execution of Tcl commands to keep track of various interesting
 * places in a text.  An index is only valid up until the next
 * modification to the character structure of the b-tree so they
 * can't be retained across Tcl commands.  However, mods to marks
 * or tags don't invalidate indices.
 */

typedef struct TkTextIndex {
    TkTextBTree tree;			/* Tree containing desired position. */
    TkTextLine *linePtr;		/* Pointer to line containing position
					 * of interest. */
    int charIndex;			/* Index within line of desired
					 * character (0 means first one). */
} TkTextIndex;

/*
 * Types for procedure pointers stored in TkTextDispChunk strutures:
 */

typedef struct TkTextDispChunk TkTextDispChunk;

typedef void 		Tk_ChunkDisplayProc _ANSI_ARGS_((
			    TkTextDispChunk *chunkPtr, int x, int y,
			    int height, int baseline, Display *display,
			    Drawable dst, int screenY));
typedef void		Tk_ChunkUndisplayProc _ANSI_ARGS_((
			    struct TkText *textPtr,
			    TkTextDispChunk *chunkPtr));
typedef int		Tk_ChunkMeasureProc _ANSI_ARGS_((
			    TkTextDispChunk *chunkPtr, int x));
typedef void		Tk_ChunkBboxProc _ANSI_ARGS_((
			    TkTextDispChunk *chunkPtr, int index, int y,
			    int lineHeight, int baseline, int *xPtr,
			    int *yPtr, int *widthPtr, int *heightPtr));

/*
 * The structure below represents a chunk of stuff that is displayed
 * together on the screen.  This structure is allocated and freed by
 * generic display code but most of its fields are filled in by
 * segment-type-specific code.
 */

struct TkTextDispChunk {
    /*
     * The fields below are set by the type-independent code before
     * calling the segment-type-specific layoutProc.  They should not
     * be modified by segment-type-specific code.
     */

    int x;				/* X position of chunk, in pixels.
					 * This position is measured from the
					 * left edge of the logical line,
					 * not from the left edge of the
					 * window (i.e. it doesn't change
					 * under horizontal scrolling). */
    struct TkTextDispChunk *nextPtr;	/* Next chunk in the display line
					 * or NULL for the end of the list. */
    struct TextStyle *stylePtr;		/* Display information, known only
					 * to tkTextDisp.c. */

    /*
     * The fields below are set by the layoutProc that creates the
     * chunk.
     */

    Tk_ChunkDisplayProc *displayProc;	/* Procedure to invoke to draw this
					 * chunk on the display or an
					 * off-screen pixmap. */
    Tk_ChunkUndisplayProc *undisplayProc;
					/* Procedure to invoke when segment
					 * ceases to be displayed on screen
					 * anymore. */
    Tk_ChunkMeasureProc *measureProc;	/* Procedure to find character under
					 * a given x-location. */
    Tk_ChunkBboxProc *bboxProc;		/* Procedure to find bounding box
					 * of character in chunk. */
    int numChars;			/* Number of characters that will be
					 * displayed in the chunk. */
    int minAscent;			/* Minimum space above the baseline
					 * needed by this chunk. */
    int minDescent;			/* Minimum space below the baseline
					 * needed by this chunk. */
    int minHeight;			/* Minimum total line height needed
					 * by this chunk. */
    int width;				/* Width of this chunk, in pixels.
					 * Initially set by chunk-specific
					 * code, but may be increased to
					 * include tab or extra space at end
					 * of line. */
    int breakIndex;			/* Index within chunk of last
					 * acceptable position for a line
					 * (break just before this character).
					 * <= 0 means don't break during or
					 * immediately after this chunk. */
    ClientData clientData;		/* Additional information for use
					 * of displayProc and undisplayProc. */
};

/*
 * One data structure of the following type is used for each tag in a
 * text widget.  These structures are kept in textPtr->tagTable and
 * referred to in other structures.
 */

typedef struct TkTextTag {
    char *name;			/* Name of this tag.  This field is actually
				 * a pointer to the key from the entry in
				 * textPtr->tagTable, so it needn't be freed
				 * explicitly. */
    int priority;		/* Priority of this tag within widget.  0
				 * means lowest priority.  Exactly one tag
				 * has each integer value between 0 and
				 * numTags-1. */
    struct Node *tagRootPtr;	/* Pointer into the B-Tree at the lowest
				 * node that completely dominates the ranges
				 * of text occupied by the tag.  At this
				 * node there is no information about the
				 * tag.  One or more children of the node
				 * do contain information about the tag. */
    int toggleCount;		/* Total number of tag toggles */

    /*
     * Information for displaying text with this tag.  The information
     * belows acts as an override on information specified by lower-priority
     * tags.  If no value is specified, then the next-lower-priority tag
     * on the text determins the value.  The text widget itself provides
     * defaults if no tag specifies an override.
     */

    Tk_3DBorder border;		/* Used for drawing background.  NULL means
				 * no value specified here. */
    char *bdString;		/* -borderwidth option string (malloc-ed).
				 * NULL means option not specified. */
    int borderWidth;		/* Width of 3-D border for background. */
    char *reliefString;		/* -relief option string (malloc-ed).
				 * NULL means option not specified. */
    int relief;			/* 3-D relief for background. */
    Pixmap bgStipple;		/* Stipple bitmap for background.  None
				 * means no value specified here. */
    XColor *fgColor;		/* Foreground color for text.  NULL means
				 * no value specified here. */
    XFontStruct *fontPtr;	/* Font for displaying text.  NULL means
				 * no value specified here. */
    Pixmap fgStipple;		/* Stipple bitmap for text and other
				 * foreground stuff.   None means no value
				 * specified here.*/
    char *justifyString;	/* -justify option string (malloc-ed).
				 * NULL means option not specified. */
    Tk_Justify justify;		/* How to justify text: TK_JUSTIFY_LEFT,
				 * TK_JUSTIFY_RIGHT, or TK_JUSTIFY_CENTER.
				 * Only valid if justifyString is non-NULL. */
    char *lMargin1String;	/* -lmargin1 option string (malloc-ed).
				 * NULL means option not specified. */
    int lMargin1;		/* Left margin for first display line of
				 * each text line, in pixels.  Only valid
				 * if lMargin1String is non-NULL. */
    char *lMargin2String;	/* -lmargin2 option string (malloc-ed).
				 * NULL means option not specified. */
    int lMargin2;		/* Left margin for second and later display
				 * lines of each text line, in pixels.  Only
				 * valid if lMargin2String is non-NULL. */
    char *offsetString;		/* -offset option string (malloc-ed).
				 * NULL means option not specified. */
    int offset;			/* Vertical offset of text's baseline from
				 * baseline of line.  Used for superscripts
				 * and subscripts.  Only valid if
				 * offsetString is non-NULL. */
    char *overstrikeString;	/* -overstrike option string (malloc-ed).
				 * NULL means option not specified. */
    int overstrike;		/* Non-zero means draw horizontal line through
				 * middle of text.  Only valid if
				 * overstrikeString is non-NULL. */
    char *rMarginString;	/* -rmargin option string (malloc-ed).
				 * NULL means option not specified. */
    int rMargin;		/* Right margin for text, in pixels.  Only
				 * valid if rMarginString is non-NULL. */
    char *spacing1String;	/* -spacing1 option string (malloc-ed).
				 * NULL means option not specified. */
    int spacing1;		/* Extra spacing above first display
				 * line for text line.  Only valid if
				 * spacing1String is non-NULL. */
    char *spacing2String;	/* -spacing2 option string (malloc-ed).
				 * NULL means option not specified. */
    int spacing2;		/* Extra spacing between display
				 * lines for the same text line.  Only valid
				 * if spacing2String is non-NULL. */
    char *spacing3String;	/* -spacing2 option string (malloc-ed).
				 * NULL means option not specified. */
    int spacing3;		/* Extra spacing below last display
				 * line for text line.  Only valid if
				 * spacing3String is non-NULL. */
    char *tabString;		/* -tabs option string (malloc-ed).
				 * NULL means option not specified. */
    struct TkTextTabArray *tabArrayPtr;
				/* Info about tabs for tag (malloc-ed)
				 * or NULL.  Corresponds to tabString. */
    char *underlineString;	/* -underline option string (malloc-ed).
				 * NULL means option not specified. */
    int underline;		/* Non-zero means draw underline underneath
				 * text.  Only valid if underlineString is
				 * non-NULL. */
    Tk_Uid wrapMode;		/* How to handle wrap-around for this tag.
				 * Must be tkTextCharUid, tkTextNoneUid,
				 * tkTextWordUid, or NULL to use wrapMode
				 * for whole widget. */
    int affectsDisplay;		/* Non-zero means that this tag affects the
				 * way information is displayed on the screen
				 * (so need to redisplay if tag changes). */
} TkTextTag;

#define TK_TAG_AFFECTS_DISPLAY	0x1
#define TK_TAG_UNDERLINE	0x2
#define TK_TAG_JUSTIFY		0x4
#define TK_TAG_OFFSET		0x10

/*
 * The data structure below is used for searching a B-tree for transitions
 * on a single tag (or for all tag transitions).  No code outside of
 * tkTextBTree.c should ever modify any of the fields in these structures,
 * but it's OK to use them for read-only information.
 */

typedef struct TkTextSearch {
    TkTextIndex curIndex;		/* Position of last tag transition
					 * returned by TkBTreeNextTag, or
					 * index of start of segment
					 * containing starting position for
					 * search if TkBTreeNextTag hasn't
					 * been called yet, or same as
					 * stopIndex if search is over. */
    TkTextSegment *segPtr;		/* Actual tag segment returned by last
					 * call to TkBTreeNextTag, or NULL if
					 * TkBTreeNextTag hasn't returned
					 * anything yet. */
    TkTextSegment *nextPtr;		/* Where to resume search in next
					 * call to TkBTreeNextTag. */
    TkTextSegment *lastPtr;		/* Stop search before just before
					 * considering this segment. */
    TkTextTag *tagPtr;			/* Tag to search for (or tag found, if
					 * allTags is non-zero). */
    int linesLeft;			/* Lines left to search (including
					 * curIndex and stopIndex).  When
					 * this becomes <= 0 the search is
					 * over. */
    int allTags;			/* Non-zero means ignore tag check:
					 * search for transitions on all
					 * tags. */
} TkTextSearch;

/*
 * The following data structure describes a single tab stop.
 */

typedef enum {LEFT, RIGHT, CENTER, NUMERIC} TkTextTabAlign;

typedef struct TkTextTab {
    int location;			/* Offset in pixels of this tab stop
					 * from the left margin (lmargin2) of
					 * the text. */
    TkTextTabAlign alignment;		/* Where the tab stop appears relative
					 * to the text. */
} TkTextTab;

typedef struct TkTextTabArray {
    int numTabs;			/* Number of tab stops. */
    TkTextTab tabs[1];			/* Array of tabs.  The actual size
					 * will be numTabs.  THIS FIELD MUST
					 * BE THE LAST IN THE STRUCTURE. */
} TkTextTabArray;

/*
 * A data structure of the following type is kept for each text widget that
 * currently exists for this process:
 */

typedef struct TkText {
    Tk_Window tkwin;		/* Window that embodies the text.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Display *display;		/* Display for widget.  Needed, among other
				 * things, to allow resources to be freed
				 * even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with widget.  Used
				 * to delete widget command.  */
    Tcl_Command widgetCmd;	/* Token for text's widget command. */
    TkTextBTree tree;		/* B-tree representation of text and tags for
				 * widget. */
    Tcl_HashTable tagTable;	/* Hash table that maps from tag names to
				 * pointers to TkTextTag structures. */
    int numTags;		/* Number of tags currently defined for
				 * widget;  needed to keep track of
				 * priorities. */
    Tcl_HashTable markTable;	/* Hash table that maps from mark names to
				 * pointers to mark segments. */
    Tcl_HashTable windowTable;	/* Hash table that maps from window names
				 * to pointers to window segments.  If a
				 * window segment doesn't yet have an
				 * associated window, there is no entry for
				 * it here. */
    Tk_Uid state;		/* Normal or disabled.  Text is read-only
				 * when disabled. */

    /*
     * Default information for displaying (may be overridden by tags
     * applied to ranges of characters).
     */

    Tk_3DBorder border;		/* Structure used to draw 3-D border and
				 * default background. */
    int borderWidth;		/* Width of 3-D border to draw around entire
				 * widget. */
    int padX, padY;		/* Padding between text and window border. */
    int relief;			/* 3-d effect for border around entire
				 * widget: TK_RELIEF_RAISED etc. */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColorPtr;
				/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */
    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    XColor *fgColor;		/* Default foreground color for text. */
    XFontStruct *fontPtr;	/* Default font for displaying text. */
    int charWidth;		/* Width of average character in default
				 * font. */
    int spacing1;		/* Default extra spacing above first display
				 * line for each text line. */
    int spacing2;		/* Default extra spacing between display lines
				 * for the same text line. */
    int spacing3;		/* Default extra spacing below last display
				 * line for each text line. */
    char *tabOptionString;	/* Value of -tabs option string (malloc'ed). */
    TkTextTabArray *tabArrayPtr;
				/* Information about tab stops (malloc'ed).
				 * NULL means perform default tabbing
				 * behavior. */

    /*
     * Additional information used for displaying:
     */

    Tk_Uid wrapMode;		/* How to handle wrap-around.  Must be
				 * tkTextCharUid, tkTextNoneUid, or
				 * tkTextWordUid. */
    int width, height;		/* Desired dimensions for window, measured
				 * in characters. */
    int setGrid;		/* Non-zero means pass gridding information
				 * to window manager. */
    int prevWidth, prevHeight;	/* Last known dimensions of window;  used to
				 * detect changes in size. */
    TkTextIndex topIndex;	/* Identifies first character in top display
				 * line of window. */
    struct TextDInfo *dInfoPtr;	/* Information maintained by tkTextDisp.c. */

    /*
     * Information related to selection.
     */

    TkTextTag *selTagPtr;	/* Pointer to "sel" tag.  Used to tell when
				 * a new selection has been made. */
    Tk_3DBorder selBorder;	/* Border and background for selected
				 * characters.  This is a copy of information
				 * in *cursorTagPtr, so it shouldn't be
				 * explicitly freed. */
    char *selBdString;		/* Value of -selectborderwidth option, or NULL
				 * if not specified (malloc'ed). */
    XColor *selFgColorPtr;	/* Foreground color for selected text.
				 * This is a copy of information in
				 * *cursorTagPtr, so it shouldn't be
				 * explicitly freed. */
    int exportSelection;	/* Non-zero means tie "sel" tag to X
				 * selection. */
    TkTextIndex selIndex;	/* Used during multi-pass selection retrievals.
				 * This index identifies the next character
				 * to be returned from the selection. */
    int abortSelections;	/* Set to 1 whenever the text is modified
				 * in a way that interferes with selection
				 * retrieval:  used to abort incremental
				 * selection retrievals. */
    int selOffset;		/* Offset in selection corresponding to
				 * selLine and selCh.  -1 means neither
				 * this information nor selIndex is of any
				 * use. */

    /*
     * Information related to insertion cursor:
     */

    TkTextSegment *insertMarkPtr;
				/* Points to segment for "insert" mark. */
    Tk_3DBorder insertBorder;	/* Used to draw vertical bar for insertion
				 * cursor. */
    int insertWidth;		/* Total width of insert cursor. */
    int insertBorderWidth;	/* Width of 3-D border around insert cursor. */
    int insertOnTime;		/* Number of milliseconds cursor should spend
				 * in "on" state for each blink. */
    int insertOffTime;		/* Number of milliseconds cursor should spend
				 * in "off" state for each blink. */
    Tcl_TimerToken insertBlinkHandler;
				/* Timer handler used to blink cursor on and
				 * off. */

    /*
     * Information used for event bindings associated with tags:
     */

    Tk_BindingTable bindingTable;
				/* Table of all bindings currently defined
				 * for this widget.  NULL means that no
				 * bindings exist, so the table hasn't been
				 * created.  Each "object" used for this
				 * table is the address of a tag. */
    TkTextSegment *currentMarkPtr;
				/* Pointer to segment for "current" mark,
				 * or NULL if none. */
    XEvent pickEvent;		/* The event from which the current character
				 * was chosen.  Must be saved so that we
				 * can repick after modifications to the
				 * text. */
    int numCurTags;		/* Number of tags associated with character
				 * at current mark. */
    TkTextTag **curTagArrayPtr;	/* Pointer to array of tags for current
				 * mark, or NULL if none. */

    /*
     * Miscellaneous additional information:
     */

    char *takeFocus;		/* Value of -takeFocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *xScrollCmd;		/* Prefix of command to issue to update
				 * horizontal scrollbar when view changes. */
    char *yScrollCmd;		/* Prefix of command to issue to update
				 * vertical scrollbar when view changes. */
    int flags;			/* Miscellaneous flags;  see below for
				 * definitions. */
} TkText;

/*
 * Flag values for TkText records:
 *
 * GOT_SELECTION:		Non-zero means we've already claimed the
 *				selection.
 * INSERT_ON:			Non-zero means insertion cursor should be
 *				displayed on screen.
 * GOT_FOCUS:			Non-zero means this window has the input
 *				focus.
 * BUTTON_DOWN:			1 means that a mouse button is currently
 *				down;  this is used to implement grabs
 *				for the duration of button presses.
 * UPDATE_SCROLLBARS:		Non-zero means scrollbar(s) should be updated
 *				during next redisplay operation.
 */

#define GOT_SELECTION		1
#define INSERT_ON		2
#define GOT_FOCUS		4
#define BUTTON_DOWN		8
#define UPDATE_SCROLLBARS	0x10
#define NEED_REPICK		0x20

/*
 * Records of the following type define segment types in terms of
 * a collection of procedures that may be called to manipulate
 * segments of that type.
 */

typedef TkTextSegment *	Tk_SegSplitProc _ANSI_ARGS_((
			    struct TkTextSegment *segPtr, int index));
typedef int		Tk_SegDeleteProc _ANSI_ARGS_((
			    struct TkTextSegment *segPtr,
			    TkTextLine *linePtr, int treeGone));
typedef TkTextSegment *	Tk_SegCleanupProc _ANSI_ARGS_((
			    struct TkTextSegment *segPtr, TkTextLine *linePtr));
typedef void		Tk_SegLineChangeProc _ANSI_ARGS_((
			    struct TkTextSegment *segPtr, TkTextLine *linePtr));
typedef int		Tk_SegLayoutProc _ANSI_ARGS_((struct TkText *textPtr,
			    struct TkTextIndex *indexPtr, TkTextSegment *segPtr,
			    int offset, int maxX, int maxChars,
			    int noCharsYet, Tk_Uid wrapMode,
			    struct TkTextDispChunk *chunkPtr));
typedef void		Tk_SegCheckProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));

typedef struct Tk_SegType {
    char *name;				/* Name of this kind of segment. */
    int leftGravity;			/* If a segment has zero size (e.g. a
					 * mark or tag toggle), does it
					 * attach to character to its left
					 * or right?  1 means left, 0 means
					 * right. */
    Tk_SegSplitProc *splitProc;		/* Procedure to split large segment
					 * into two smaller ones. */
    Tk_SegDeleteProc *deleteProc;	/* Procedure to call to delete
					 * segment. */
    Tk_SegCleanupProc *cleanupProc;	/* After any change to a line, this
					 * procedure is invoked for all
					 * segments left in the line to
					 * perform any cleanup they wish
					 * (e.g. joining neighboring
					 * segments). */
    Tk_SegLineChangeProc *lineChangeProc;
					/* Invoked when a segment is about
					 * to be moved from its current line
					 * to an earlier line because of
					 * a deletion.  The linePtr is that
					 * for the segment's old line.
					 * CleanupProc will be invoked after
					 * the deletion is finished. */
    Tk_SegLayoutProc *layoutProc;	/* Returns size information when
					 * figuring out what to display in
					 * window. */
    Tk_SegCheckProc *checkProc;		/* Called during consistency checks
					 * to check internal consistency of
					 * segment. */
} Tk_SegType;

/*
 * The constant below is used to specify a line when what is really
 * wanted is the entire text.  For now, just use a very big number.
 */

#define TK_END_OF_TEXT 1000000

/*
 * The following definition specifies the maximum number of characters
 * needed in a string to hold a position specifier.
 */

#define TK_POS_CHARS 30

/*
 * Declarations for variables shared among the text-related files:
 */

extern int		tkBTreeDebug;
extern int		tkTextDebug;
extern Tk_SegType	tkTextCharType;
extern Tk_Uid		tkTextCharUid;
extern Tk_Uid		tkTextDisabledUid;
extern Tk_SegType	tkTextLeftMarkType;
extern Tk_Uid		tkTextNoneUid;
extern Tk_Uid 		tkTextNormalUid;
extern Tk_SegType	tkTextRightMarkType;
extern Tk_SegType	tkTextToggleOnType;
extern Tk_SegType	tkTextToggleOffType;
extern Tk_Uid		tkTextWordUid;

/*
 * Declarations for procedures that are used by the text-related files
 * but shouldn't be used anywhere else in Tk (or by Tk clients):
 */

extern int		TkBTreeCharTagged _ANSI_ARGS_((TkTextIndex *indexPtr,
			    TkTextTag *tagPtr));
extern void		TkBTreeCheck _ANSI_ARGS_((TkTextBTree tree));
extern int		TkBTreeCharsInLine _ANSI_ARGS_((TkTextLine *linePtr));
extern TkTextBTree	TkBTreeCreate _ANSI_ARGS_((TkText *textPtr));
extern void		TkBTreeDestroy _ANSI_ARGS_((TkTextBTree tree));
extern void		TkBTreeDeleteChars _ANSI_ARGS_((TkTextIndex *index1Ptr,
			    TkTextIndex *index2Ptr));
extern TkTextLine *	TkBTreeFindLine _ANSI_ARGS_((TkTextBTree tree,
			    int line));
extern TkTextTag **	TkBTreeGetTags _ANSI_ARGS_((TkTextIndex *indexPtr,
			    int *numTagsPtr));
extern void		TkBTreeInsertChars _ANSI_ARGS_((TkTextIndex *indexPtr,
			    char *string));
extern int		TkBTreeLineIndex _ANSI_ARGS_((TkTextLine *linePtr));
extern void		TkBTreeLinkSegment _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextIndex *indexPtr));
extern TkTextLine *	TkBTreeNextLine _ANSI_ARGS_((TkTextLine *linePtr));
extern int		TkBTreeNextTag _ANSI_ARGS_((TkTextSearch *searchPtr));
extern int		TkBTreeNumLines _ANSI_ARGS_((TkTextBTree tree));
extern TkTextLine *	TkBTreePreviousLine _ANSI_ARGS_((TkTextLine *linePtr));
extern int		TkBTreePrevTag _ANSI_ARGS_((TkTextSearch *searchPtr));
extern void		TkBTreeStartSearch _ANSI_ARGS_((TkTextIndex *index1Ptr,
			    TkTextIndex *index2Ptr, TkTextTag *tagPtr,
			    TkTextSearch *searchPtr));
extern void		TkBTreeStartSearchBack _ANSI_ARGS_((TkTextIndex *index1Ptr,
			    TkTextIndex *index2Ptr, TkTextTag *tagPtr,
			    TkTextSearch *searchPtr));
extern void		TkBTreeTag _ANSI_ARGS_((TkTextIndex *index1Ptr,
			    TkTextIndex *index2Ptr, TkTextTag *tagPtr,
			    int add));
extern void		TkBTreeUnlinkSegment _ANSI_ARGS_((TkTextBTree tree,
			    TkTextSegment *segPtr, TkTextLine *linePtr));
extern void		TkTextBindProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
extern void		TkTextChanged _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *index1Ptr, TkTextIndex *index2Ptr));
extern int		TkTextCharBbox _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *indexPtr, int *xPtr, int *yPtr,
			    int *widthPtr, int *heightPtr));
extern int		TkTextCharLayoutProc _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *indexPtr, TkTextSegment *segPtr,
			    int offset, int maxX, int maxChars, int noBreakYet,
			    Tk_Uid wrapMode, TkTextDispChunk *chunkPtr));
extern void		TkTextCreateDInfo _ANSI_ARGS_((TkText *textPtr));
extern int		TkTextDLineInfo _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *indexPtr, int *xPtr, int *yPtr,
			    int *widthPtr, int *heightPtr, int *basePtr));
extern TkTextTag *	TkTextCreateTag _ANSI_ARGS_((TkText *textPtr,
			    char *tagName));
extern void		TkTextFreeDInfo _ANSI_ARGS_((TkText *textPtr));
extern void		TkTextFreeTag _ANSI_ARGS_((TkText *textPtr,
			    TkTextTag *tagPtr));
extern int		TkTextGetIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    TkText *textPtr, char *string,
			    TkTextIndex *indexPtr));
extern TkTextTabArray *	TkTextGetTabs _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *string));
extern void		TkTextIndexBackChars _ANSI_ARGS_((TkTextIndex *srcPtr,
			    int count, TkTextIndex *dstPtr));
extern int		TkTextIndexCmp _ANSI_ARGS_((TkTextIndex *index1Ptr,
			    TkTextIndex *index2Ptr));
extern void		TkTextIndexForwChars _ANSI_ARGS_((TkTextIndex *srcPtr,
			    int count, TkTextIndex *dstPtr));
extern TkTextSegment *	TkTextIndexToSeg _ANSI_ARGS_((TkTextIndex *indexPtr,
			    int *offsetPtr));
extern void		TkTextInsertDisplayProc _ANSI_ARGS_((
			    TkTextDispChunk *chunkPtr, int x, int y, int height,
			    int baseline, Display *display, Drawable dst,
			    int screenY));
extern void		TkTextLostSelection _ANSI_ARGS_((
			    ClientData clientData));
extern TkTextIndex *	TkTextMakeIndex _ANSI_ARGS_((TkTextBTree tree,
			    int lineIndex, int charIndex,
			    TkTextIndex *indexPtr));
extern int		TkTextMarkCmd _ANSI_ARGS_((TkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		TkTextMarkNameToIndex _ANSI_ARGS_((TkText *textPtr,
			    char *name, TkTextIndex *indexPtr));
extern void		TkTextMarkSegToIndex _ANSI_ARGS_((TkText *textPtr,
			    TkTextSegment *markPtr, TkTextIndex *indexPtr));
extern void		TkTextEventuallyRepick _ANSI_ARGS_((TkText *textPtr));
extern void		TkTextPickCurrent _ANSI_ARGS_((TkText *textPtr,
			    XEvent *eventPtr));
extern void		TkTextPixelIndex _ANSI_ARGS_((TkText *textPtr,
			    int x, int y, TkTextIndex *indexPtr));
extern void		TkTextPrintIndex _ANSI_ARGS_((TkTextIndex *indexPtr,
			    char *string));
extern void		TkTextRedrawRegion _ANSI_ARGS_((TkText *textPtr,
			    int x, int y, int width, int height));
extern void		TkTextRedrawTag _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *index1Ptr, TkTextIndex *index2Ptr,
			    TkTextTag *tagPtr, int withTag));
extern void		TkTextRelayoutWindow _ANSI_ARGS_((TkText *textPtr));
extern int		TkTextScanCmd _ANSI_ARGS_((TkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		TkTextSeeCmd _ANSI_ARGS_((TkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		TkTextSegToOffset _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
extern TkTextSegment *	TkTextSetMark _ANSI_ARGS_((TkText *textPtr, char *name,
			    TkTextIndex *indexPtr));
extern void		TkTextSetYView _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *indexPtr, int pickPlace));
extern int		TkTextTagCmd _ANSI_ARGS_((TkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		TkTextWindowCmd _ANSI_ARGS_((TkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		TkTextWindowIndex _ANSI_ARGS_((TkText *textPtr,
			    char *name, TkTextIndex *indexPtr));
extern int		TkTextXviewCmd _ANSI_ARGS_((TkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));
extern int		TkTextYviewCmd _ANSI_ARGS_((TkText *textPtr,
			    Tcl_Interp *interp, int argc, char **argv));

#endif /* _TKTEXT */
