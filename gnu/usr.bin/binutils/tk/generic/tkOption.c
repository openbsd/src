/* 
 * tkOption.c --
 *
 *	This module contains procedures to manage the option
 *	database, which allows various strings to be associated
 *	with windows either by name or by class or both.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkOption.c 1.54 96/02/27 15:41:25
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * The option database is stored as one tree for each main window.
 * Each name or class field in an option is associated with a node or
 * leaf of the tree.  For example, the options "x.y.z" and "x.y*a"
 * each correspond to three nodes in the tree;  they share the nodes
 * "x" and "x.y", but have different leaf nodes.  One of the following
 * structures exists for each node or leaf in the option tree.  It is
 * actually stored as part of the parent node, and describes a particular
 * child of the parent.
 */

typedef struct Element {
    Tk_Uid nameUid;			/* Name or class from one element of
					 * an option spec. */
    union {
	struct ElArray *arrayPtr;	/* If this is an intermediate node,
					 * a pointer to a structure describing
					 * the remaining elements of all
					 * options whose prefixes are the
					 * same up through this element. */
	Tk_Uid valueUid;		/* For leaf nodes, this is the string
					 * value of the option. */
    } child;
    int priority;			/* Used to select among matching
					 * options.  Includes both the
					 * priority level and a serial #.
					 * Greater value means higher
					 * priority.  Irrelevant except in
					 * leaf nodes. */
    int flags;				/* OR-ed combination of bits.  See
					 * below for values. */
} Element;

/*
 * Flags in Element structures:
 *
 * CLASS -		Non-zero means this element refers to a class,
 *			Zero means this element refers to a name.
 * NODE -		Zero means this is a leaf element (the child
 *			field is a value, not a pointer to another node).
 *			One means this is a node element.
 * WILDCARD -		Non-zero means this there was a star in the
 *			original specification just before this element.
 *			Zero means there was a dot.
 */

#define TYPE_MASK		0x7

#define CLASS			0x1
#define NODE			0x2
#define WILDCARD		0x4

#define EXACT_LEAF_NAME		0x0
#define EXACT_LEAF_CLASS	0x1
#define EXACT_NODE_NAME		0x2
#define EXACT_NODE_CLASS	0x3
#define WILDCARD_LEAF_NAME	0x4
#define WILDCARD_LEAF_CLASS	0x5
#define WILDCARD_NODE_NAME	0x6
#define WILDCARD_NODE_CLASS	0x7

/*
 * The following structure is used to manage a dynamic array of
 * Elements.  These structures are used for two purposes:  to store
 * the contents of a node in the option tree, and for the option
 * stacks described below.
 */

typedef struct ElArray {
    int arraySize;		/* Number of elements actually
				 * allocated in the "els" array. */
    int numUsed;		/* Number of elements currently in
				 * use out of els. */
    Element *nextToUse;		/* Pointer to &els[numUsed]. */
    Element els[1];		/* Array of structures describing
				 * children of this node.  The
				 * array will actually contain enough
				 * elements for all of the children
				 * (and even a few extras, perhaps).
				 * This must be the last field in
				 * the structure. */
} ElArray;

#define EL_ARRAY_SIZE(numEls) ((unsigned) (sizeof(ElArray) \
	+ ((numEls)-1)*sizeof(Element)))
#define INITIAL_SIZE 5

/*
 * In addition to the option tree, which is a relatively static structure,
 * there are eight additional structures called "stacks", which are used
 * to speed up queries into the option database.  The stack structures
 * are designed for the situation where an individual widget makes repeated
 * requests for its particular options.  The requests differ only in
 * their last name/class, so during the first request we extract all
 * the options pertaining to the particular widget and save them in a
 * stack-like cache;  subsequent requests for the same widget can search
 * the cache relatively quickly.  In fact, the cache is a hierarchical
 * one, storing a list of relevant options for this widget and all of
 * its ancestors up to the application root;  hence the name "stack".
 *
 * Each of the eight stacks consists of an array of Elements, ordered in
 * terms of levels in the window hierarchy.  All the elements relevant
 * for the top-level widget appear first in the array, followed by all
 * those from the next-level widget on the path to the current widget,
 * etc. down to those for the current widget.
 *
 * Cached information is divided into eight stacks according to the
 * CLASS, NODE, and WILDCARD flags.  Leaf and non-leaf information is
 * kept separate to speed up individual probes (non-leaf information is
 * only relevant when building the stacks, but isn't relevant when
 * making probes;  similarly, only non-leaf information is relevant
 * when the stacks are being extended to the next widget down in the
 * widget hierarchy).  Wildcard elements are handled separately from
 * "exact" elements because once they appear at a particular level in
 * the stack they remain active for all deeper levels;  exact elements
 * are only relevant at a particular level.  For example, when searching
 * for options relevant in a particular window, the entire wildcard
 * stacks get checked, but only the portions of the exact stacks that
 * pertain to the window's parent.  Lastly, name and class stacks are
 * kept separate because different search keys are used when searching
 * them;  keeping them separate speeds up the searches.
 */

#define NUM_STACKS 8
static ElArray *stacks[NUM_STACKS];
static TkWindow *cachedWindow = NULL;	/* Lowest-level window currently
					 * loaded in stacks at present. 
					 * NULL means stacks have never
					 * been used, or have been
					 * invalidated because of a change
					 * to the database. */

/*
 * One of the following structures is used to keep track of each
 * level in the stacks.
 */

typedef struct StackLevel {
    TkWindow *winPtr;		/* Window corresponding to this stack
				 * level. */
    int bases[NUM_STACKS];	/* For each stack, index of first
				 * element on stack corresponding to
				 * this level (used to restore "numUsed"
				 * fields when popping out of a level. */
} StackLevel;

/*
 * Information about all of the stack levels that are currently
 * active.  This array grows dynamically to become as large as needed.
 */

static StackLevel *levels = NULL;
				/* Array describing current stack. */
static int numLevels = 0;	/* Total space allocated. */
static int curLevel = -1;	/* Highest level currently in use.  Note:
				 * curLevel is never 0!  (I don't remember
				 * why anymore...) */

/*
 * The variable below is a serial number for all options entered into
 * the database so far.  It increments on each addition to the option
 * database.  It is used in computing option priorities, so that the
 * most recent entry wins when choosing between options at the same
 * priority level.
 */

static int serial = 0;

/*
 * Special "no match" Element to use as default for searches.
 */

static Element defaultMatch;

/*
 * Forward declarations for procedures defined in this file:
 */

static int		AddFromString _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *string, int priority));
static void		ClearOptionTree _ANSI_ARGS_((ElArray *arrayPtr));
static ElArray *	ExtendArray _ANSI_ARGS_((ElArray *arrayPtr,
			    Element *elPtr));
static void		ExtendStacks _ANSI_ARGS_((ElArray *arrayPtr,
			    int leaf));
static int		GetDefaultOptions _ANSI_ARGS_((Tcl_Interp *interp,
			    TkWindow *winPtr));	
static ElArray *	NewArray _ANSI_ARGS_((int numEls));	
static void		OptionInit _ANSI_ARGS_((TkMainInfo *mainPtr));
static int		ParsePriority _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
static int		ReadOptionFile _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char *fileName, int priority));
static void		SetupStacks _ANSI_ARGS_((TkWindow *winPtr, int leaf));

/*
 *--------------------------------------------------------------
 *
 * Tk_AddOption --
 *
 *	Add a new option to the option database.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information is added to the option database.
 *
 *--------------------------------------------------------------
 */

void
Tk_AddOption(tkwin, name, value, priority)
    Tk_Window tkwin;		/* Window token;  option will be associated
				 * with main window for this window. */
    char *name;			/* Multi-element name of option. */
    char *value;		/* String value for option. */
    int priority;		/* Overall priority level to use for
				 * this option, such as TK_USER_DEFAULT_PRIO
				 * or TK_INTERACTIVE_PRIO.  Must be between
				 * 0 and TK_MAX_PRIO. */
{
    TkWindow *winPtr = ((TkWindow *) tkwin)->mainPtr->winPtr;
    register ElArray **arrayPtrPtr;
    register Element *elPtr;
    Element newEl;
    register char *p;
    char *field;
    int count, firstField, length;
#define TMP_SIZE 100
    char tmp[TMP_SIZE+1];

    if (winPtr->mainPtr->optionRootPtr == NULL) {
	OptionInit(winPtr->mainPtr);
    }
    cachedWindow = NULL;	/* Invalidate the cache. */

    /*
     * Compute the priority for the new element, including both the
     * overall level and the serial number (to disambiguate with the
     * level).
     */

    if (priority < 0) {
	priority = 0;
    } else if (priority > TK_MAX_PRIO) {
	priority = TK_MAX_PRIO;
    }
    newEl.priority = (priority << 24) + serial;
    serial++;

    /*
     * Parse the option one field at a time.
     */

    arrayPtrPtr = &(((TkWindow *) tkwin)->mainPtr->optionRootPtr);
    p = name;
    for (firstField = 1; ; firstField = 0) {

	/*
	 * Scan the next field from the name and convert it to a Tk_Uid.
	 * Must copy the field before calling Tk_Uid, so that a terminating
	 * NULL may be added without modifying the source string.
	 */

	if (*p == '*') {
	    newEl.flags = WILDCARD;
	    p++;
	} else {
	    newEl.flags = 0;
	}
	field = p;
	while ((*p != 0) && (*p != '.') && (*p != '*')) {
	    p++;
	}
	length = p - field;
	if (length > TMP_SIZE) {
	    length = TMP_SIZE;
	}
	strncpy(tmp, field, (size_t) length);
	tmp[length] = 0;
	newEl.nameUid = Tk_GetUid(tmp);
	if (isupper(UCHAR(*field))) {
	    newEl.flags |= CLASS;
	}

	if (*p != 0) {

	    /*
	     * New element will be a node.  If this option can't possibly
	     * apply to this main window, then just skip it.  Otherwise,
	     * add it to the parent, if it isn't already there, and descend
	     * into it.
	     */

	    newEl.flags |= NODE;
	    if (firstField && !(newEl.flags & WILDCARD)
		    && (newEl.nameUid != winPtr->nameUid)
		    && (newEl.nameUid != winPtr->classUid)) {
		return;
	    }
	    for (elPtr = (*arrayPtrPtr)->els, count = (*arrayPtrPtr)->numUsed;
		    ; elPtr++, count--) {
		if (count == 0) {
		    newEl.child.arrayPtr = NewArray(5);
		    *arrayPtrPtr = ExtendArray(*arrayPtrPtr, &newEl);
		    arrayPtrPtr = &((*arrayPtrPtr)->nextToUse[-1].child.arrayPtr);
		    break;
		}
		if ((elPtr->nameUid == newEl.nameUid)
			&& (elPtr->flags == newEl.flags)) {
		    arrayPtrPtr = &(elPtr->child.arrayPtr);
		    break;
		}
	    }
	    if (*p == '.') {
		p++;
	    }
	} else {

	    /*
	     * New element is a leaf.  Add it to the parent, if it isn't
	     * already there.  If it exists already, keep whichever value
	     * has highest priority.
	     */

	    newEl.child.valueUid = Tk_GetUid(value);
	    for (elPtr = (*arrayPtrPtr)->els, count = (*arrayPtrPtr)->numUsed;
		    ; elPtr++, count--) {
		if (count == 0) {
		    *arrayPtrPtr = ExtendArray(*arrayPtrPtr, &newEl);
		    return;
		}
		if ((elPtr->nameUid == newEl.nameUid)
			&& (elPtr->flags == newEl.flags)) {
		    if (elPtr->priority < newEl.priority) {
			elPtr->priority = newEl.priority;
			elPtr->child.valueUid = newEl.child.valueUid;
		    }
		    return;
		}
	    }
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetOption --
 *
 *	Retrieve an option from the option database.
 *
 * Results:
 *	The return value is the value specified in the option
 *	database for the given name and class on the given
 *	window.  If there is nothing specified in the database
 *	for that option, then NULL is returned.
 *
 * Side effects:
 *	The internal caches used to speed up option mapping
 *	may be modified, if this tkwin is different from the
 *	last tkwin used for option retrieval.
 *
 *--------------------------------------------------------------
 */

Tk_Uid
Tk_GetOption(tkwin, name, className)
    Tk_Window tkwin;		/* Token for window that option is
				 * associated with. */
    char *name;			/* Name of option. */
    char *className;		/* Class of option.  NULL means there
				 * is no class for this option:  just
				 * check for name. */
{
    Tk_Uid nameId, classId;
    register Element *elPtr, *bestPtr;
    register int count;

    /*
     * Note:  no need to call OptionInit here:  it will be done by
     * the SetupStacks call below (squeeze out those nanoseconds).
     */

    if (tkwin != (Tk_Window) cachedWindow) {
	SetupStacks((TkWindow *) tkwin, 1);
    }

    nameId = Tk_GetUid(name);
    bestPtr = &defaultMatch;
    for (elPtr = stacks[EXACT_LEAF_NAME]->els,
	    count = stacks[EXACT_LEAF_NAME]->numUsed; count > 0;
	    elPtr++, count--) {
	if ((elPtr->nameUid == nameId)
		&& (elPtr->priority > bestPtr->priority)) {
	    bestPtr = elPtr;
	}
    }
    for (elPtr = stacks[WILDCARD_LEAF_NAME]->els,
	    count = stacks[WILDCARD_LEAF_NAME]->numUsed; count > 0;
	    elPtr++, count--) {
	if ((elPtr->nameUid == nameId)
		&& (elPtr->priority > bestPtr->priority)) {
	    bestPtr = elPtr;
	}
    }
    if (className != NULL) {
	classId = Tk_GetUid(className);
	for (elPtr = stacks[EXACT_LEAF_CLASS]->els,
		count = stacks[EXACT_LEAF_CLASS]->numUsed; count > 0;
		elPtr++, count--) {
	    if ((elPtr->nameUid == classId)
		    && (elPtr->priority > bestPtr->priority)) {
		bestPtr = elPtr;
	    }
	}
	for (elPtr = stacks[WILDCARD_LEAF_CLASS]->els,
		count = stacks[WILDCARD_LEAF_CLASS]->numUsed; count > 0;
		elPtr++, count--) {
	    if ((elPtr->nameUid == classId)
		    && (elPtr->priority > bestPtr->priority)) {
		bestPtr = elPtr;
	    }
	}
    }
    return bestPtr->child.valueUid;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_OptionCmd --
 *
 *	This procedure is invoked to process the "option" Tcl command.
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

int
Tk_OptionCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    size_t length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" cmd arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "add", length) == 0)) {
	int priority;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " add pattern value ?priority?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 4) {
	    priority = TK_INTERACTIVE_PRIO;
	} else {
	    priority = ParsePriority(interp, argv[4]);
	    if (priority < 0) {
		return TCL_ERROR;
	    }
	}
	Tk_AddOption(tkwin, argv[2], argv[3], priority);
	return TCL_OK;
    } else if ((c == 'c') && (strncmp(argv[1], "clear", length) == 0)) {
	TkMainInfo *mainPtr;

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " clear\"", (char *) NULL);
	    return TCL_ERROR;
	}
	mainPtr = ((TkWindow *) tkwin)->mainPtr;
	if (mainPtr->optionRootPtr != NULL) {
	    ClearOptionTree(mainPtr->optionRootPtr);
	    mainPtr->optionRootPtr = NULL;
	}
	cachedWindow = NULL;
	return TCL_OK;
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	Tk_Window window;
	Tk_Uid value;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get window name class\"", (char *) NULL);
	    return TCL_ERROR;
	}
	window = Tk_NameToWindow(interp, argv[2], tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	value = Tk_GetOption(window, argv[3], argv[4]);
	if (value != NULL) {
	    interp->result = value;
	}
	return TCL_OK;
    } else if ((c == 'r') && (strncmp(argv[1], "readfile", length) == 0)) {
	int priority;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " readfile fileName ?priority?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 4) {
	    priority = ParsePriority(interp, argv[3]);
	    if (priority < 0) {
		return TCL_ERROR;
	    }
	} else {
	    priority = TK_INTERACTIVE_PRIO;
	}
	return ReadOptionFile(interp, tkwin, argv[2], priority);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be add, clear, get, or readfile", (char *) NULL);
	return TCL_ERROR;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkOptionDeadWindow --
 *
 *	This procedure is called whenever a window is deleted.
 *	It cleans up any option-related stuff associated with
 *	the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Option-related resources are freed.  See code below
 *	for details.
 *
 *--------------------------------------------------------------
 */

void
TkOptionDeadWindow(winPtr)
    register TkWindow *winPtr;		/* Window to be cleaned up. */
{
    /*
     * If this window is in the option stacks, then clear the stacks.
     */

    if (winPtr->optionLevel != -1) {
	int i;

	for (i = 1; i <= curLevel; i++) {
	    levels[i].winPtr->optionLevel = -1;
	}
	curLevel = -1;
	cachedWindow = NULL;
    }

    /*
     * If this window was a main window, then delete its option
     * database.
     */

    if ((winPtr->mainPtr->winPtr == winPtr)
	    && (winPtr->mainPtr->optionRootPtr != NULL)) {
	ClearOptionTree(winPtr->mainPtr->optionRootPtr);
	winPtr->mainPtr->optionRootPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkOptionClassChanged --
 *
 *	This procedure is invoked when a window's class changes.  If
 *	the window is on the option cache, this procedure flushes
 *	any information for the window, since the new class could change
 *	what is relevant.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The option cache may be flushed in part or in whole.
 *
 *----------------------------------------------------------------------
 */

void
TkOptionClassChanged(winPtr)
    TkWindow *winPtr;			/* Window whose class changed. */
{
    int i, j, *basePtr;
    ElArray *arrayPtr;

    if (winPtr->optionLevel == -1) {
	return;
    }

    /*
     * Find the lowest stack level that refers to this window, then
     * flush all of the levels above the matching one.
     */

    for (i = 1; i <= curLevel; i++) {
	if (levels[i].winPtr == winPtr) {
	    for (j = i; j <= curLevel; j++) {
		levels[j].winPtr->optionLevel = -1;
	    }
	    curLevel = i-1;
	    basePtr = levels[i].bases;
	    for (j = 0; j < NUM_STACKS; j++) {
		arrayPtr = stacks[j];
		arrayPtr->numUsed = basePtr[j];
		arrayPtr->nextToUse = &arrayPtr->els[arrayPtr->numUsed];
	    }
	    if (curLevel <= 0) {
		cachedWindow = NULL;
	    } else {
		cachedWindow = levels[curLevel].winPtr;
	    }
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ParsePriority --
 *
 *	Parse a string priority value.
 *
 * Results:
 *	The return value is the integer priority level corresponding
 *	to string, or -1 if string doesn't point to a valid priority level.
 *	In this case, an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ParsePriority(interp, string)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    char *string;		/* Describes a priority level, either
				 * symbolically or numerically. */
{
    int priority, c;
    size_t length;

    c = string[0];
    length = strlen(string);
    if ((c == 'w')
	    && (strncmp(string, "widgetDefault", length) == 0)) {
	return TK_WIDGET_DEFAULT_PRIO;
    } else if ((c == 's')
	    && (strncmp(string, "startupFile", length) == 0)) {
	return TK_STARTUP_FILE_PRIO;
    } else if ((c == 'u')
	    && (strncmp(string, "userDefault", length) == 0)) {
	return TK_USER_DEFAULT_PRIO;
    } else if ((c == 'i')
	    && (strncmp(string, "interactive", length) == 0)) {
	return TK_INTERACTIVE_PRIO;
    } else {
	char *end;

	priority = strtoul(string, &end, 0);
	if ((end == string) || (*end != 0) || (priority < 0)
		|| (priority > 100)) {
	    Tcl_AppendResult(interp,  "bad priority level \"", string,
		    "\": must be widgetDefault, startupFile, userDefault, ",
		    "interactive, or a number between 0 and 100",
		    (char *) NULL);
	    return -1;
	}
    }
    return priority;
}

/*
 *----------------------------------------------------------------------
 *
 * AddFromString --
 *
 *	Given a string containing lines in the standard format for
 *	X resources (see other documentation for details on what this
 *	is), parse the resource specifications and enter them as options
 *	for tkwin's main window.
 *
 * Results:
 *	The return value is a standard Tcl return code.  In the case of
 *	an error in parsing string, TCL_ERROR will be returned and an
 *	error message will be left in interp->result.  The memory at
 *	string is totally trashed by this procedure.  If you care about
 *	its contents, make a copy before calling here.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
AddFromString(interp, tkwin, string, priority)
    Tcl_Interp *interp;		/* Interpreter to use for reporting results. */
    Tk_Window tkwin;		/* Token for window:  options are entered
				 * for this window's main window. */
    char *string;		/* String containing option specifiers. */
    int priority;		/* Priority level to use for options in
				 * this string, such as TK_USER_DEFAULT_PRIO
				 * or TK_INTERACTIVE_PRIO.  Must be between
				 * 0 and TK_MAX_PRIO. */
{
    register char *src, *dst;
    char *name, *value;
    int lineNum;

    src = string;
    lineNum = 1;
    while (1) {

	/*
	 * Skip leading white space and empty lines and comment lines, and
	 * check for the end of the spec.
	 */

	while ((*src == ' ') || (*src == '\t')) {
	    src++;
	}
	if ((*src == '#') || (*src == '!')) {
	    do {
		src++;
		if ((src[0] == '\\') && (src[1] == '\n')) {
		    src += 2;
		    lineNum++;
		}
	    } while ((*src != '\n') && (*src != 0));
	}
	if (*src == '\n') {
	    src++;
	    lineNum++;
	    continue;
	} 
	if (*src == '\0') {
	    break;
	}

	/*
	 * Parse off the option name, collapsing out backslash-newline
	 * sequences of course.
	 */

	dst = name = src;
	while (*src != ':') {
	    if ((*src == '\0') || (*src == '\n')) {
		sprintf(interp->result, "missing colon on line %d",
			lineNum);
		return TCL_ERROR;
	    }
	    if ((src[0] == '\\') && (src[1] == '\n')) {
		src += 2;
		lineNum++;
	    } else {
		*dst = *src;
		dst++;
		src++;
	    }
	}

	/*
	 * Eliminate trailing white space on the name, and null-terminate
	 * it.
	 */

	while ((dst != name) && ((dst[-1] == ' ') || (dst[-1] == '\t'))) {
	    dst--;
	}
	*dst = '\0';

	/*
	 * Skip white space between the name and the value.
	 */

	src++;
	while ((*src == ' ') || (*src == '\t')) {
	    src++;
	}
	if (*src == '\0') {
	    sprintf(interp->result, "missing value on line %d", lineNum);
	    return TCL_ERROR;
	}

	/*
	 * Parse off the value, squeezing out backslash-newline sequences
	 * along the way.
	 */

	dst = value = src;
	while (*src != '\n') {
	    if (*src == '\0') {
		sprintf(interp->result, "missing newline on line %d",
			lineNum);
		return TCL_ERROR;
	    }
	    if ((src[0] == '\\') && (src[1] == '\n')) {
		src += 2;
		lineNum++;
	    } else {
		*dst = *src;
		dst++;
		src++;
	    }
	}
	*dst = 0;

	/*
	 * Enter the option into the database.
	 */

	Tk_AddOption(tkwin, name, value, priority);
	src++;
	lineNum++;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ReadOptionFile --
 *
 * 	Read a file of options ("resources" in the old X terminology)
 *	and load them into the option database.
 *
 * Results:
 *	The return value is a standard Tcl return code.  In the case of
 *	an error in parsing string, TCL_ERROR will be returned and an
 *	error message will be left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ReadOptionFile(interp, tkwin, fileName, priority)
    Tcl_Interp *interp;		/* Interpreter to use for reporting results. */
    Tk_Window tkwin;		/* Token for window:  options are entered
				 * for this window's main window. */
    char *fileName;		/* Name of file containing options. */
    int priority;		/* Priority level to use for options in
				 * this file, such as TK_USER_DEFAULT_PRIO
				 * or TK_INTERACTIVE_PRIO.  Must be between
				 * 0 and TK_MAX_PRIO. */
{
    char *realName, *buffer;
    int result, bufferSize;
    Tcl_Channel chan;
    Tcl_DString newName;

    realName = Tcl_TranslateFileName(interp, fileName, &newName);
    if (realName == NULL) {
	return TCL_ERROR;
    }
    chan = Tcl_OpenFileChannel(interp, realName, "r", 0);
    Tcl_DStringFree(&newName);
    if (chan == NULL) {
	return TCL_ERROR;
    }

    /*
     * Compute size of file by seeking to the end of the file.
     */
    
    bufferSize = Tcl_Seek(chan, 0L, SEEK_END);
    (void) Tcl_Seek(chan, 0L, SEEK_SET);
    
    buffer = (char *) ckalloc((unsigned) bufferSize+1);
    if (Tcl_Read(chan, buffer, bufferSize) != bufferSize) {
	Tcl_AppendResult(interp, "error reading file \"", fileName, "\"",
		(char *) NULL);
	Tcl_Close(NULL, chan);
	return TCL_ERROR;
    }
    Tcl_Close(NULL, chan);
    buffer[bufferSize] = 0;
    result = AddFromString(interp, tkwin, buffer, priority);
    ckfree(buffer);
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * NewArray --
 *
 *	Create a new ElArray structure of a given size.
 *
 * Results:
 *	The return value is a pointer to a properly initialized
 *	element array with "numEls" space.  The array is marked
 *	as having no active elements.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *--------------------------------------------------------------
 */

static ElArray *
NewArray(numEls)
    int numEls;			/* How many elements of space to allocate. */
{
    register ElArray *arrayPtr;

    arrayPtr = (ElArray *) ckalloc(EL_ARRAY_SIZE(numEls));
    arrayPtr->arraySize = numEls;
    arrayPtr->numUsed = 0;
    arrayPtr->nextToUse = arrayPtr->els;
    return arrayPtr;
}

/*
 *--------------------------------------------------------------
 *
 * ExtendArray --
 *
 *	Add a new element to an array, extending the array if
 *	necessary.
 *
 * Results:
 *	The return value is a pointer to the new array, which
 *	will be different from arrayPtr if the array got expanded.
 *
 * Side effects:
 *	Memory may be allocated or freed.
 *
 *--------------------------------------------------------------
 */

static ElArray *
ExtendArray(arrayPtr, elPtr)
    register ElArray *arrayPtr;		/* Array to be extended. */
    register Element *elPtr;		/* Element to be copied into array. */
{
    /*
     * If the current array has filled up, make it bigger.
     */

    if (arrayPtr->numUsed >= arrayPtr->arraySize) {
	register ElArray *newPtr;

	newPtr = (ElArray *) ckalloc(EL_ARRAY_SIZE(2*arrayPtr->arraySize));
	newPtr->arraySize = 2*arrayPtr->arraySize;
	newPtr->numUsed = arrayPtr->numUsed;
	newPtr->nextToUse = &newPtr->els[newPtr->numUsed];
	memcpy((VOID *) newPtr->els, (VOID *) arrayPtr->els,
		(arrayPtr->arraySize*sizeof(Element)));
	ckfree((char *) arrayPtr);
	arrayPtr = newPtr;
    }

    *arrayPtr->nextToUse = *elPtr;
    arrayPtr->nextToUse++;
    arrayPtr->numUsed++;
    return arrayPtr;
}

/*
 *--------------------------------------------------------------
 *
 * SetupStacks --
 *
 *	Arrange the stacks so that they cache all the option
 *	information for a particular window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The stacks are modified to hold information for tkwin
 *	and all its ancestors in the window hierarchy.
 *
 *--------------------------------------------------------------
 */

static void
SetupStacks(winPtr, leaf)
    TkWindow *winPtr;		/* Window for which information is to
				 * be cached. */
    int leaf;			/* Non-zero means this is the leaf
				 * window being probed.  Zero means this
				 * is an ancestor of the desired leaf. */
{
    int level, i, *iPtr;
    register StackLevel *levelPtr;
    register ElArray *arrayPtr;

    /*
     * The following array defines the order in which the current
     * stacks are searched to find matching entries to add to the
     * stacks.  Given the current priority-based scheme, the order
     * below is no longer relevant;  all that matters is that an
     * element is on the list *somewhere*.  The ordering is a relic
     * of the old days when priorities were determined differently.
     */

    static int searchOrder[] = {WILDCARD_NODE_CLASS, WILDCARD_NODE_NAME,
	    EXACT_NODE_CLASS, EXACT_NODE_NAME, -1};

    if (winPtr->mainPtr->optionRootPtr == NULL) {
	OptionInit(winPtr->mainPtr);
    }

    /*
     * Step 1:  make sure that options are cached for this window's
     * parent.
     */

    if (winPtr->parentPtr != NULL) {
	level = winPtr->parentPtr->optionLevel;
	if ((level == -1) || (cachedWindow == NULL)) {
	    SetupStacks(winPtr->parentPtr, 0);
	    level = winPtr->parentPtr->optionLevel;
	}
	level++;
    } else {
	level = 1;
    }

    /*
     * Step 2:  pop extra unneeded information off the stacks and
     * mark those windows as no longer having cached information.
     */

    if (curLevel >= level) {
	while (curLevel >= level) {
	    levels[curLevel].winPtr->optionLevel = -1;
	    curLevel--;
	}
	levelPtr = &levels[level];
	for (i = 0; i < NUM_STACKS; i++) {
	    arrayPtr = stacks[i];
	    arrayPtr->numUsed = levelPtr->bases[i];
	    arrayPtr->nextToUse = &arrayPtr->els[arrayPtr->numUsed];
	}
    }
    curLevel = winPtr->optionLevel = level;

    /*
     * Step 3:  if the root database information isn't loaded or
     * isn't valid, initialize level 0 of the stack from the
     * database root (this only happens if winPtr is a main window).
     */

    if ((curLevel == 1)
	    && ((cachedWindow == NULL)
	    || (cachedWindow->mainPtr != winPtr->mainPtr))) {
	for (i = 0; i < NUM_STACKS; i++) {
	    arrayPtr = stacks[i];
	    arrayPtr->numUsed = 0;
	    arrayPtr->nextToUse = arrayPtr->els;
	}
	ExtendStacks(winPtr->mainPtr->optionRootPtr, 0);
    }

    /*
     * Step 4: create a new stack level;  grow the level array if
     * we've run out of levels.  Clear the stacks for EXACT_LEAF_NAME
     * and EXACT_LEAF_CLASS (anything that was there is of no use
     * any more).
     */

    if (curLevel >= numLevels) {
	StackLevel *newLevels;

	newLevels = (StackLevel *) ckalloc((unsigned)
		(numLevels*2*sizeof(StackLevel)));
	memcpy((VOID *) newLevels, (VOID *) levels,
		(numLevels*sizeof(StackLevel)));
	ckfree((char *) levels);
	numLevels *= 2;
	levels = newLevels;
    }
    levelPtr = &levels[curLevel];
    levelPtr->winPtr = winPtr;
    arrayPtr = stacks[EXACT_LEAF_NAME];
    arrayPtr->numUsed = 0;
    arrayPtr->nextToUse = arrayPtr->els;
    arrayPtr = stacks[EXACT_LEAF_CLASS];
    arrayPtr->numUsed = 0;
    arrayPtr->nextToUse = arrayPtr->els;
    levelPtr->bases[EXACT_LEAF_NAME] = stacks[EXACT_LEAF_NAME]->numUsed;
    levelPtr->bases[EXACT_LEAF_CLASS] = stacks[EXACT_LEAF_CLASS]->numUsed;
    levelPtr->bases[EXACT_NODE_NAME] = stacks[EXACT_NODE_NAME]->numUsed;
    levelPtr->bases[EXACT_NODE_CLASS] = stacks[EXACT_NODE_CLASS]->numUsed;
    levelPtr->bases[WILDCARD_LEAF_NAME] = stacks[WILDCARD_LEAF_NAME]->numUsed;
    levelPtr->bases[WILDCARD_LEAF_CLASS] = stacks[WILDCARD_LEAF_CLASS]->numUsed;
    levelPtr->bases[WILDCARD_NODE_NAME] = stacks[WILDCARD_NODE_NAME]->numUsed;
    levelPtr->bases[WILDCARD_NODE_CLASS] = stacks[WILDCARD_NODE_CLASS]->numUsed;


    /*
     * Step 5: scan the current stack level looking for matches to this
     * window's name or class;  where found, add new information to the
     * stacks.
     */

    for (iPtr = searchOrder; *iPtr != -1; iPtr++) {
	register Element *elPtr;
	int count;
	Tk_Uid id;

	i = *iPtr;
	if (i & CLASS) {
	    id = winPtr->classUid;
	} else {
	    id = winPtr->nameUid;
	}
	elPtr = stacks[i]->els;
	count = levelPtr->bases[i];

	/*
	 * For wildcard stacks, check all entries;  for non-wildcard
	 * stacks, only check things that matched in the parent.
	 */

	if (!(i & WILDCARD)) {
	    elPtr += levelPtr[-1].bases[i];
	    count -= levelPtr[-1].bases[i];
	}
	for ( ; count > 0; elPtr++, count--) {
	    if (elPtr->nameUid != id) {
		continue;
	    }
	    ExtendStacks(elPtr->child.arrayPtr, leaf);
	}
    }
    cachedWindow = winPtr;
}

/*
 *--------------------------------------------------------------
 *
 * ExtendStacks --
 *
 *	Given an element array, copy all the elements from the
 *	array onto the system stacks (except for irrelevant leaf
 *	elements).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The option stacks are extended.
 *
 *--------------------------------------------------------------
 */

static void
ExtendStacks(arrayPtr, leaf)
    ElArray *arrayPtr;		/* Array of elements to copy onto stacks. */
    int leaf;			/* If zero, then don't copy exact leaf
				 * elements. */
{
    register int count;
    register Element *elPtr;

    for (elPtr = arrayPtr->els, count = arrayPtr->numUsed;
	    count > 0; elPtr++, count--) {
	if (!(elPtr->flags & (NODE|WILDCARD)) && !leaf) {
	    continue;
	}
	stacks[elPtr->flags] = ExtendArray(stacks[elPtr->flags], elPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * OptionInit --
 *
 *	Initialize data structures for option handling.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Option-related data structures get initialized.
 *
 *--------------------------------------------------------------
 */

static void
OptionInit(mainPtr)
    register TkMainInfo *mainPtr;	/* Top-level information about
					 * window that isn't initialized
					 * yet. */
{
    int i;
    Tcl_Interp *interp;

    /*
     * First, once-only initialization.
     */

    if (numLevels == 0) {

	numLevels = 5;
	levels = (StackLevel *) ckalloc((unsigned) (5*sizeof(StackLevel)));
	for (i = 0; i < NUM_STACKS; i++) {
	    stacks[i] = NewArray(10);
	    levels[0].bases[i] = 0;
	}
    
	defaultMatch.nameUid = NULL;
	defaultMatch.child.valueUid = NULL;
	defaultMatch.priority = -1;
	defaultMatch.flags = 0;
    }

    /*
     * Then, per-main-window initialization.  Create and delete dummy
     * interpreter for message logging.
     */

    mainPtr->optionRootPtr = NewArray(20);
    interp = Tcl_CreateInterp();
    (void) GetDefaultOptions(interp, mainPtr->winPtr);
    Tcl_DeleteInterp(interp);
}

/*
 *--------------------------------------------------------------
 *
 * ClearOptionTree --
 *
 *	This procedure is called to erase everything in a
 *	hierarchical option database.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All the options associated with arrayPtr are deleted,
 *	along with all option subtrees.  The space pointed to
 *	by arrayPtr is freed.
 *
 *--------------------------------------------------------------
 */

static void
ClearOptionTree(arrayPtr)
    ElArray *arrayPtr;		/* Array of options;  delete everything
				 * referred to recursively by this. */
{
    register Element *elPtr;
    int count;

    for (count = arrayPtr->numUsed, elPtr = arrayPtr->els;  count > 0;
	    count--, elPtr++) {
	if (elPtr->flags & NODE) {
	    ClearOptionTree(elPtr->child.arrayPtr);
	}
    }
    ckfree((char *) arrayPtr);
}

/*
 *--------------------------------------------------------------
 *
 * GetDefaultOptions --
 *
 *	This procedure is invoked to load the default set of options
 *	for a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Options are added to those for winPtr's main window.  If
 *	there exists a RESOURCE_MANAGER proprety for winPtr's
 *	display, that is used.  Otherwise, the .Xdefaults file in
 *	the user's home directory is used.
 *
 *--------------------------------------------------------------
 */

static int
GetDefaultOptions(interp, winPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    TkWindow *winPtr;		/* Fetch option defaults for main window
				 * associated with this. */
{
    char *regProp;
    int result, actualFormat;
    unsigned long numItems, bytesAfter;
    Atom actualType;

    /*
     * Try the RESOURCE_MANAGER property on the root window first.
     */

    regProp = NULL;
    result = XGetWindowProperty(winPtr->display,
	    RootWindow(winPtr->display, 0),
	    XA_RESOURCE_MANAGER, 0, 100000,
	    False, XA_STRING, &actualType, &actualFormat,
	    &numItems, &bytesAfter, (unsigned char **) &regProp);

    if ((result == Success) && (actualType == XA_STRING)
	    && (actualFormat == 8)) {
	result = AddFromString(interp, (Tk_Window) winPtr, regProp,
		TK_USER_DEFAULT_PRIO);
	XFree(regProp);
	return result;
    }

    /*
     * No luck there.  Try a .Xdefaults file in the user's home
     * directory.
     */

    if (regProp != NULL) {
	XFree(regProp);
    }
    result = ReadOptionFile(interp, (Tk_Window) winPtr, "~/.Xdefaults",
	    TK_USER_DEFAULT_PRIO);
    return result;
}
