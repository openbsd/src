/* 
 * tkBind.c --
 *
 *	This file provides procedures that associate Tcl commands
 *	with X events or sequences of X events.
 *
 * Copyright (c) 1989-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkBind.c 1.110 96/03/22 11:54:55
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * The structure below represents a binding table.  A binding table
 * represents a domain in which event bindings may occur.  It includes
 * a space of objects relative to which events occur (usually windows,
 * but not always), a history of recent events in the domain, and
 * a set of mappings that associate particular Tcl commands with sequences
 * of events in the domain.  Multiple binding tables may exist at once,
 * either because there are multiple applications open, or because there
 * are multiple domains within an application with separate event
 * bindings for each (for example, each canvas widget has a separate
 * binding table for associating events with the items in the canvas).
 *
 * Note: it is probably a bad idea to reduce EVENT_BUFFER_SIZE much
 * below 30.  To see this, consider a triple mouse button click while
 * the Shift key is down (and auto-repeating).  There may be as many
 * as 3 auto-repeat events after each mouse button press or release
 * (see the first large comment block within Tk_BindEvent for more on
 * this), for a total of 20 events to cover the three button presses
 * and two intervening releases.  If you reduce EVENT_BUFFER_SIZE too
 * much, shift multi-clicks will be lost.
 * 
 */

#define EVENT_BUFFER_SIZE 30
typedef struct BindingTable {
    XEvent eventRing[EVENT_BUFFER_SIZE];/* Circular queue of recent events
					 * (higher indices are for more recent
					 * events). */
    int detailRing[EVENT_BUFFER_SIZE];	/* "Detail" information (keySym or
					 * button or 0) for each entry in
					 * eventRing. */
    int curEvent;			/* Index in eventRing of most recent
					 * event.  Newer events have higher
					 * indices. */
    Tcl_HashTable patternTable;		/* Used to map from an event to a list
					 * of patterns that may match that
					 * event.  Keys are PatternTableKey
					 * structs, values are (PatSeq *). */
    Tcl_HashTable objectTable;		/* Used to map from an object to a list
					 * of patterns associated with that
					 * object.  Keys are ClientData,
					 * values are (PatSeq *). */
    Tcl_Interp *interp;			/* Interpreter in which commands are
					 * executed. */
} BindingTable;

/*
 * Structures of the following form are used as keys in the patternTable
 * for a binding table:
 */

typedef struct PatternTableKey {
    ClientData object;		/* Identifies object (or class of objects)
				 * relative to which event occurred.  For
				 * example, in the widget binding table for
				 * an application this is the path name of
				 * a widget, or a widget class, or "all". */
    int type;			/* Type of event (from X). */
    int detail;			/* Additional information, such as
				 * keysym or button, or 0 if nothing
				 * additional.*/
} PatternTableKey;

/*
 * The following structure defines a pattern, which is matched
 * against X events as part of the process of converting X events
 * into Tcl commands.
 */

typedef struct Pattern {
    int eventType;		/* Type of X event, e.g. ButtonPress. */
    int needMods;		/* Mask of modifiers that must be
				 * present (0 means no modifiers are
				 * required). */
    int detail;			/* Additional information that must
				 * match event.  Normally this is 0,
				 * meaning no additional information
				 * must match.  For KeyPress and
				 * KeyRelease events, a keySym may
				 * be specified to select a
				 * particular keystroke (0 means any
				 * keystrokes).  For button events,
				 * specifies a particular button (0
				 * means any buttons are OK). */
} Pattern;

/*
 * The structure below defines a pattern sequence, which consists
 * of one or more patterns.  In order to trigger, a pattern
 * sequence must match the most recent X events (first pattern
 * to most recent event, next pattern to next event, and so on).
 */

typedef struct PatSeq {
    int numPats;		/* Number of patterns in sequence
				 * (usually 1). */
    char *command;		/* Command to invoke when this
				 * pattern sequence matches (malloc-ed). */
    int flags;			/* Miscellaneous flag values;  see
				 * below for definitions. */
    struct PatSeq *nextSeqPtr;
				/* Next in list of all pattern
				 * sequences that have the same
				 * initial pattern.  NULL means
				 * end of list. */
    Tcl_HashEntry *hPtr;	/* Pointer to hash table entry for
				 * the initial pattern.  This is the
				 * head of the list of which nextSeqPtr
				 * forms a part. */
    ClientData object;		/* Identifies object with which event is
				 * associated (e.g. window). */
    struct PatSeq *nextObjPtr;
				/* Next in list of all pattern
				 * sequences for the same object
				 * (NULL for end of list).  Needed to
				 * implement Tk_DeleteAllBindings. */
    Pattern pats[1];		/* Array of "numPats" patterns.  Only
				 * one element is declared here but
				 * in actuality enough space will be
				 * allocated for "numPats" patterns.
				 * To match, pats[0] must match event
				 * n, pats[1] must match event n-1,
				 * etc. */
} PatSeq;

/*
 * Flag values for PatSeq structures:
 *
 * PAT_NEARBY		1 means that all of the events matching
 *			this sequence must occur with nearby X
 *			and Y mouse coordinates and close in time.
 *			This is typically used to restrict multiple
 *			button presses.
 */

#define PAT_NEARBY		1

/*
 * Constants that define how close together two events must be
 * in milliseconds or pixels to meet the PAT_NEARBY constraint:
 */

#define NEARBY_PIXELS		5
#define NEARBY_MS		500

/*
 * One of the following structures exists for each interpreter,
 * associated with the key "tkBind".  This structure keeps track
 * of the current display and screen in the interpreter, so that
 * a script can be invoked whenever the display/screen changes
 * (the script does things like point tkPriv at a display-specific
 * structure).
 */

typedef struct ScreenInfo {
    TkDisplay *curDispPtr;	/* Display for last binding command invoked
				 * in this application. */
    int curScreenIndex;		/* Index of screen for last binding command. */
    int bindingDepth;		/* Number of active instances of Tk_BindEvent
				 * in this application. */
} ScreenInfo;

/*
 * In X11R4 and earlier versions, XStringToKeysym is ridiculously
 * slow.  The data structure and hash table below, along with the
 * code that uses them, implement a fast mapping from strings to
 * keysyms.  In X11R5 and later releases XStringToKeysym is plenty
 * fast so this stuff isn't needed.  The #define REDO_KEYSYM_LOOKUP
 * is normally undefined, so that XStringToKeysym gets used.  It
 * can be set in the Makefile to enable the use of the hash table
 * below.
 */

#ifdef REDO_KEYSYM_LOOKUP
typedef struct {
    char *name;				/* Name of keysym. */
    KeySym value;			/* Numeric identifier for keysym. */
} KeySymInfo;
static KeySymInfo keyArray[] = {
#ifndef lint
#include "ks_names.h"
#endif
    {(char *) NULL, 0}
};
static Tcl_HashTable keySymTable;	/* keyArray hashed by keysym value. */
static Tcl_HashTable nameTable;		/* keyArray hashed by keysym name. */
#endif /* REDO_KEYSYM_LOOKUP */

static int initialized = 0;

/*
 * A hash table is kept to map from the string names of event
 * modifiers to information about those modifiers.  The structure
 * for storing this information, and the hash table built at
 * initialization time, are defined below.
 */

typedef struct {
    char *name;			/* Name of modifier. */
    int mask;			/* Button/modifier mask value,							 * such as Button1Mask. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} ModInfo;

/*
 * Flags for ModInfo structures:
 *
 * DOUBLE -		Non-zero means duplicate this event,
 *			e.g. for double-clicks.
 * TRIPLE -		Non-zero means triplicate this event,
 *			e.g. for triple-clicks.
 */

#define DOUBLE		1
#define TRIPLE		2

/*
 * The following special modifier mask bits are defined, to indicate
 * logical modifiers such as Meta and Alt that may float among the
 * actual modifier bits.
 */

#define META_MASK	(AnyModifier<<1)
#define ALT_MASK	(AnyModifier<<2)

static ModInfo modArray[] = {
    {"Control",		ControlMask,	0},
    {"Shift",		ShiftMask,	0},
    {"Lock",		LockMask,	0},
    {"Meta",		META_MASK,	0},
    {"M",		META_MASK,	0},
    {"Alt",		ALT_MASK,	0},
    {"B1",		Button1Mask,	0},
    {"Button1",		Button1Mask,	0},
    {"B2",		Button2Mask,	0},
    {"Button2",		Button2Mask,	0},
    {"B3",		Button3Mask,	0},
    {"Button3",		Button3Mask,	0},
    {"B4",		Button4Mask,	0},
    {"Button4",		Button4Mask,	0},
    {"B5",		Button5Mask,	0},
    {"Button5",		Button5Mask,	0},
    {"Mod1",		Mod1Mask,	0},
    {"M1",		Mod1Mask,	0},
    {"Command",		Mod1Mask,	0},
    {"Mod2",		Mod2Mask,	0},
    {"M2",		Mod2Mask,	0},
    {"Option",		Mod2Mask,	0},
    {"Mod3",		Mod3Mask,	0},
    {"M3",		Mod3Mask,	0},
    {"Mod4",		Mod4Mask,	0},
    {"M4",		Mod4Mask,	0},
    {"Mod5",		Mod5Mask,	0},
    {"M5",		Mod5Mask,	0},
    {"Double",		0,		DOUBLE},
    {"Triple",		0,		TRIPLE},
    {"Any",		0,		0},	/* Ignored: historical relic. */
    {NULL,		0,		0}
};
static Tcl_HashTable modTable;

/*
 * This module also keeps a hash table mapping from event names
 * to information about those events.  The structure, an array
 * to use to initialize the hash table, and the hash table are
 * all defined below.
 */

typedef struct {
    char *name;			/* Name of event. */
    int type;			/* Event type for X, such as
				 * ButtonPress. */
    int eventMask;		/* Mask bits (for XSelectInput)
				 * for this event type. */
} EventInfo;

/*
 * Note:  some of the masks below are an OR-ed combination of
 * several masks.  This is necessary because X doesn't report
 * up events unless you also ask for down events.  Also, X
 * doesn't report button state in motion events unless you've
 * asked about button events.
 */

static EventInfo eventArray[] = {
    {"Motion",		MotionNotify,
	    ButtonPressMask|PointerMotionMask},
    {"Button",		ButtonPress,		ButtonPressMask},
    {"ButtonPress",	ButtonPress,		ButtonPressMask},
    {"ButtonRelease",	ButtonRelease,
	    ButtonPressMask|ButtonReleaseMask},
    {"Colormap",	ColormapNotify,		ColormapChangeMask},
    {"Enter",		EnterNotify,		EnterWindowMask},
    {"Leave",		LeaveNotify,		LeaveWindowMask},
    {"Expose",		Expose,			ExposureMask},
    {"FocusIn",		FocusIn,		FocusChangeMask},
    {"FocusOut",	FocusOut,		FocusChangeMask},
    {"Key",		KeyPress,		KeyPressMask},
    {"KeyPress",	KeyPress,		KeyPressMask},
    {"KeyRelease",	KeyRelease,
	    KeyPressMask|KeyReleaseMask},
    {"Property",	PropertyNotify,		PropertyChangeMask},
    {"Circulate",	CirculateNotify,	StructureNotifyMask},
    {"Configure",	ConfigureNotify,	StructureNotifyMask},
    {"Destroy",		DestroyNotify,		StructureNotifyMask},
    {"Gravity",		GravityNotify,		StructureNotifyMask},
    {"Map",		MapNotify,		StructureNotifyMask},
    {"Reparent",	ReparentNotify,		StructureNotifyMask},
    {"Unmap",		UnmapNotify,		StructureNotifyMask},
    {"Visibility",	VisibilityNotify,	VisibilityChangeMask},
    {"Activate",	ActivateNotify,		ActivateMask},
    {"Deactivate",	DeactivateNotify,	ActivateMask},
    {(char *) NULL,	0,			0}
};
static Tcl_HashTable eventTable;

/*
 * The defines and table below are used to classify events into
 * various groups.  The reason for this is that logically identical
 * fields (e.g. "state") appear at different places in different
 * types of events.  The classification masks can be used to figure
 * out quickly where to extract information from events.
 */

#define KEY_BUTTON_MOTION	0x1
#define CROSSING		0x2
#define FOCUS			0x4
#define EXPOSE			0x8
#define VISIBILITY		0x10
#define CREATE			0x20
#define MAP			0x40
#define REPARENT		0x80
#define CONFIG			0x100
#define CONFIG_REQ		0x200
#define RESIZE_REQ		0x400
#define GRAVITY			0x800
#define PROP			0x1000
#define SEL_CLEAR		0x2000
#define SEL_REQ			0x4000
#define SEL_NOTIFY		0x8000
#define COLORMAP		0x10000
#define MAPPING			0x20000
#define ACTIVATE		0x40000

static int flagArray[TK_LASTEVENT] = {
   /* Not used */		0,
   /* Not used */		0,
   /* KeyPress */		KEY_BUTTON_MOTION,
   /* KeyRelease */		KEY_BUTTON_MOTION,
   /* ButtonPress */		KEY_BUTTON_MOTION,
   /* ButtonRelease */		KEY_BUTTON_MOTION,
   /* MotionNotify */		KEY_BUTTON_MOTION,
   /* EnterNotify */		CROSSING,
   /* LeaveNotify */		CROSSING,
   /* FocusIn */		FOCUS,
   /* FocusOut */		FOCUS,
   /* KeymapNotify */		0,
   /* Expose */			EXPOSE,
   /* GraphicsExpose */		EXPOSE,
   /* NoExpose */		0,
   /* VisibilityNotify */	VISIBILITY,
   /* CreateNotify */		CREATE,
   /* DestroyNotify */		0,
   /* UnmapNotify */		0,
   /* MapNotify */		MAP,
   /* MapRequest */		0,
   /* ReparentNotify */		REPARENT,
   /* ConfigureNotify */	CONFIG,
   /* ConfigureRequest */	CONFIG_REQ,
   /* GravityNotify */		0,
   /* ResizeRequest */		RESIZE_REQ,
   /* CirculateNotify */	0,
   /* CirculateRequest */	0,
   /* PropertyNotify */		PROP,
   /* SelectionClear */		SEL_CLEAR,
   /* SelectionRequest */	SEL_REQ,
   /* SelectionNotify */	SEL_NOTIFY,
   /* ColormapNotify */		COLORMAP,
   /* ClientMessage */		0,
   /* MappingNotify */		MAPPING,
   /* Activate */		ACTIVATE,	    
   /* Deactivate */		ACTIVATE
};

/*
 * Prototypes for local procedures defined in this file:
 */

static void		ChangeScreen _ANSI_ARGS_((Tcl_Interp *interp,
			    char *dispName, int screenIndex));
static void		ExpandPercents _ANSI_ARGS_((TkWindow *winPtr,
			    char *before, XEvent *eventPtr, KeySym keySym,
			    Tcl_DString *dsPtr));
static PatSeq *		FindSequence _ANSI_ARGS_((Tcl_Interp *interp,
			    BindingTable *bindPtr, ClientData object,
			    char *eventString, int create,
			    unsigned long *maskPtr));
static void		FreeScreenInfo _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static char *		GetField _ANSI_ARGS_((char *p, char *copy, int size));
static KeySym		GetKeySym _ANSI_ARGS_((TkDisplay *dispPtr,
			    XEvent *eventPtr));
static void		InitKeymapInfo _ANSI_ARGS_((TkDisplay *dispPtr));
static PatSeq *		MatchPatterns _ANSI_ARGS_((TkDisplay *dispPtr,
			    BindingTable *bindPtr, PatSeq *psPtr));

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateBindingTable --
 *
 *	Set up a new domain in which event bindings may be created.
 *
 * Results:
 *	The return value is a token for the new table, which must
 *	be passed to procedures like Tk_CreatBinding.
 *
 * Side effects:
 *	Memory is allocated for the new table.
 *
 *--------------------------------------------------------------
 */

Tk_BindingTable
Tk_CreateBindingTable(interp)
    Tcl_Interp *interp;		/* Interpreter to associate with the binding
				 * table:  commands are executed in this
				 * interpreter. */
{
    register BindingTable *bindPtr;
    int i;

    /*
     * If this is the first time a binding table has been created,
     * initialize the global data structures.
     */

    if (!initialized) {
	register Tcl_HashEntry *hPtr;
	register ModInfo *modPtr;
	register EventInfo *eiPtr;
	int dummy;

#ifdef REDO_KEYSYM_LOOKUP
	register KeySymInfo *kPtr;

	Tcl_InitHashTable(&keySymTable, TCL_STRING_KEYS);
	Tcl_InitHashTable(&nameTable, TCL_ONE_WORD_KEYS);
	for (kPtr = keyArray; kPtr->name != NULL; kPtr++) {
	    hPtr = Tcl_CreateHashEntry(&keySymTable, kPtr->name, &dummy);
	    Tcl_SetHashValue(hPtr, kPtr->value);
	    hPtr = Tcl_CreateHashEntry(&nameTable, (char *) kPtr->value,
		    &dummy);
	    Tcl_SetHashValue(hPtr, kPtr->name);
	}
#endif /* REDO_KEYSYM_LOOKUP */

	initialized = 1;
    
	Tcl_InitHashTable(&modTable, TCL_STRING_KEYS);
	for (modPtr = modArray; modPtr->name != NULL; modPtr++) {
	    hPtr = Tcl_CreateHashEntry(&modTable, modPtr->name, &dummy);
	    Tcl_SetHashValue(hPtr, modPtr);
	}
    
	Tcl_InitHashTable(&eventTable, TCL_STRING_KEYS);
	for (eiPtr = eventArray; eiPtr->name != NULL; eiPtr++) {
	    hPtr = Tcl_CreateHashEntry(&eventTable, eiPtr->name, &dummy);
	    Tcl_SetHashValue(hPtr, eiPtr);
	}
    }

    /*
     * Create and initialize a new binding table.
     */

    bindPtr = (BindingTable *) ckalloc(sizeof(BindingTable));
    for (i = 0; i < EVENT_BUFFER_SIZE; i++) {
	bindPtr->eventRing[i].type = -1;
    }
    bindPtr->curEvent = 0;
    Tcl_InitHashTable(&bindPtr->patternTable,
	    sizeof(PatternTableKey)/sizeof(int));
    Tcl_InitHashTable(&bindPtr->objectTable, TCL_ONE_WORD_KEYS);
    bindPtr->interp = interp;
    return (Tk_BindingTable) bindPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteBindingTable --
 *
 *	Destroy a binding table and free up all its memory.
 *	The caller should not use bindingTable again after
 *	this procedure returns.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteBindingTable(bindingTable)
    Tk_BindingTable bindingTable;	/* Token for the binding table to
					 * destroy. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    PatSeq *psPtr, *nextPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    /*
     * Find and delete all of the patterns associated with the binding
     * table.
     */

    for (hPtr = Tcl_FirstHashEntry(&bindPtr->patternTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
		psPtr != NULL; psPtr = nextPtr) {
	    nextPtr = psPtr->nextSeqPtr;
	    ckfree((char *) psPtr->command);
	    ckfree((char *) psPtr);
	}
    }

    /*
     * Clean up the rest of the information associated with the
     * binding table.
     */

    Tcl_DeleteHashTable(&bindPtr->patternTable);
    Tcl_DeleteHashTable(&bindPtr->objectTable);
    ckfree((char *) bindPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateBinding --
 *
 *	Add a binding to a binding table, so that future calls to
 *	Tk_BindEvent may execute the command in the binding.
 *
 * Results:
 *	The return value is 0 if an error occurred while setting
 *	up the binding.  In this case, an error message will be
 *	left in interp->result.  If all went well then the return
 *	value is a mask of the event types that must be made
 *	available to Tk_BindEvent in order to properly detect when
 *	this binding triggers.  This value can be used to determine
 *	what events to select for in a window, for example.
 *
 * Side effects:
 *	The new binding may cause future calls to Tk_BindEvent to
 *	behave differently than they did previously.
 *
 *--------------------------------------------------------------
 */

unsigned long
Tk_CreateBinding(interp, bindingTable, object, eventString, command, append)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_BindingTable bindingTable;	/* Table in which to create binding. */
    ClientData object;			/* Token for object with which binding
					 * is associated. */
    char *eventString;			/* String describing event sequence
					 * that triggers binding. */
    char *command;			/* Contains Tcl command to execute
					 * when binding triggers. */
    int append;				/* 0 means replace any existing
					 * binding for eventString;  1 means
					 * append to that binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    register PatSeq *psPtr;
    unsigned long eventMask;

    psPtr = FindSequence(interp, bindPtr, object, eventString, 1, &eventMask);
    if (psPtr == NULL) {
	return 0;
    }
    if (append && (psPtr->command != NULL)) {
	int length;
	char *new;

	length = strlen(psPtr->command) + strlen(command) + 2;
	new = (char *) ckalloc((unsigned) length);
	sprintf(new, "%s\n%s", psPtr->command, command);
	ckfree((char *) psPtr->command);
	psPtr->command = new;
    } else {
	if (psPtr->command != NULL) {
	    ckfree((char *) psPtr->command);
	}
	psPtr->command = (char *) ckalloc((unsigned) (strlen(command) + 1));
	strcpy(psPtr->command, command);
    }
    return eventMask;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteBinding --
 *
 *	Remove an event binding from a binding table.
 *
 * Results:
 *	The result is a standard Tcl return value.  If an error
 *	occurs then interp->result will contain an error message.
 *
 * Side effects:
 *	The binding given by object and eventString is removed
 *	from bindingTable.
 *
 *--------------------------------------------------------------
 */

int
Tk_DeleteBinding(interp, bindingTable, object, eventString)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_BindingTable bindingTable;	/* Table in which to delete binding. */
    ClientData object;			/* Token for object with which binding
					 * is associated. */
    char *eventString;			/* String describing event sequence
					 * that triggers binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    register PatSeq *psPtr, *prevPtr;
    unsigned long eventMask;
    Tcl_HashEntry *hPtr;

    psPtr = FindSequence(interp, bindPtr, object, eventString, 0, &eventMask);
    if (psPtr == NULL) {
	Tcl_ResetResult(interp);
	return TCL_OK;
    }

    /*
     * Unlink the binding from the list for its object, then from the
     * list for its pattern.
     */

    hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) object);
    if (hPtr == NULL) {
	panic("Tk_DeleteBinding couldn't find object table entry");
    }
    prevPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
    if (prevPtr == psPtr) {
	Tcl_SetHashValue(hPtr, psPtr->nextObjPtr);
    } else {
	for ( ; ; prevPtr = prevPtr->nextObjPtr) {
	    if (prevPtr == NULL) {
		panic("Tk_DeleteBinding couldn't find on object list");
	    }
	    if (prevPtr->nextObjPtr == psPtr) {
		prevPtr->nextObjPtr = psPtr->nextObjPtr;
		break;
	    }
	}
    }
    prevPtr = (PatSeq *) Tcl_GetHashValue(psPtr->hPtr);
    if (prevPtr == psPtr) {
	if (psPtr->nextSeqPtr == NULL) {
	    Tcl_DeleteHashEntry(psPtr->hPtr);
	} else {
	    Tcl_SetHashValue(psPtr->hPtr, psPtr->nextSeqPtr);
	}
    } else {
	for ( ; ; prevPtr = prevPtr->nextSeqPtr) {
	    if (prevPtr == NULL) {
		panic("Tk_DeleteBinding couldn't find on hash chain");
	    }
	    if (prevPtr->nextSeqPtr == psPtr) {
		prevPtr->nextSeqPtr = psPtr->nextSeqPtr;
		break;
	    }
	}
    }
    ckfree((char *) psPtr->command);
    ckfree((char *) psPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetBinding --
 *
 *	Return the command associated with a given event string.
 *
 * Results:
 *	The return value is a pointer to the command string
 *	associated with eventString for object in the domain
 *	given by bindingTable.  If there is no binding for
 *	eventString, or if eventString is improperly formed,
 *	then NULL is returned and an error message is left in
 *	interp->result.  The return value is semi-static:  it
 *	will persist until the binding is changed or deleted.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_GetBinding(interp, bindingTable, object, eventString)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tk_BindingTable bindingTable;	/* Table in which to look for
					 * binding. */
    ClientData object;			/* Token for object with which binding
					 * is associated. */
    char *eventString;			/* String describing event sequence
					 * that triggers binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    register PatSeq *psPtr;
    unsigned long eventMask;

    psPtr = FindSequence(interp, bindPtr, object, eventString, 0, &eventMask);
    if (psPtr == NULL) {
	return NULL;
    }
    return psPtr->command;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetAllBindings --
 *
 *	Return a list of event strings for all the bindings
 *	associated with a given object.
 *
 * Results:
 *	There is no return value.  Interp->result is modified to
 *	hold a Tcl list with one entry for each binding associated
 *	with object in bindingTable.  Each entry in the list
 *	contains the event string associated with one binding.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tk_GetAllBindings(interp, bindingTable, object)
    Tcl_Interp *interp;			/* Interpreter returning result or
					 * error. */
    Tk_BindingTable bindingTable;	/* Table in which to look for
					 * bindings. */
    ClientData object;			/* Token for object. */

{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    register PatSeq *psPtr;
    register Pattern *patPtr;
    Tcl_HashEntry *hPtr;
    Tcl_DString ds;
    char c, buffer[10];
    int patsLeft, needMods;
    register ModInfo *modPtr;
    register EventInfo *eiPtr;

    hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) object);
    if (hPtr == NULL) {
	return;
    }
    Tcl_DStringInit(&ds);
    for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr); psPtr != NULL;
	    psPtr = psPtr->nextObjPtr) {
	Tcl_DStringSetLength(&ds, 0);

	/*
	 * For each binding, output information about each of the
	 * patterns in its sequence.  The order of the patterns in
	 * the sequence is backwards from the order in which they
	 * must be output.
	 */

	for (patsLeft = psPtr->numPats,
		patPtr = &psPtr->pats[psPtr->numPats - 1];
		patsLeft > 0; patsLeft--, patPtr--) {

	    /*
	     * Check for simple case of an ASCII character.
	     */

	    if ((patPtr->eventType == KeyPress)
		    && (patPtr->needMods == 0)
		    && (patPtr->detail < 128)
		    && isprint(UCHAR(patPtr->detail))
		    && (patPtr->detail != '<')
		    && (patPtr->detail != ' ')) {

		c = patPtr->detail;
		Tcl_DStringAppend(&ds, &c, 1);
		continue;
	    }

	    /*
	     * It's a more general event specification.  First check
	     * for "Double" or "Triple", then modifiers, then event type,
	     * then keysym or button detail.
	     */

	    Tcl_DStringAppend(&ds, "<", 1);
	    if ((patsLeft > 1) && (memcmp((char *) patPtr,
		    (char *) (patPtr-1), sizeof(Pattern)) == 0)) {
		patsLeft--;
		patPtr--;
		if ((patsLeft > 1) && (memcmp((char *) patPtr,
			(char *) (patPtr-1), sizeof(Pattern)) == 0)) {
		    patsLeft--;
		    patPtr--;
		    Tcl_DStringAppend(&ds, "Triple-", 7);
		} else {
		    Tcl_DStringAppend(&ds, "Double-", 7);
		}
	    }

	    for (needMods = patPtr->needMods, modPtr = modArray;
		    needMods != 0; modPtr++) {
		if (modPtr->mask & needMods) {
		    needMods &= ~modPtr->mask;
		    Tcl_DStringAppend(&ds, modPtr->name, -1);
		    Tcl_DStringAppend(&ds, "-", 1);
		}
	    }

	    for (eiPtr = eventArray; eiPtr->name != NULL; eiPtr++) {
		if (eiPtr->type == patPtr->eventType) {
		    Tcl_DStringAppend(&ds, eiPtr->name, -1);
		    if (patPtr->detail != 0) {
			Tcl_DStringAppend(&ds, "-", 1);
		    }
		    break;
		}
	    }

	    if (patPtr->detail != 0) {
		if ((patPtr->eventType == KeyPress)
			|| (patPtr->eventType == KeyRelease)) {
		    char *string;

		    string = TkKeysymToString((KeySym) patPtr->detail);
		    if (string != NULL) {
			Tcl_DStringAppend(&ds, string, -1);
		    }
		} else {
		    sprintf(buffer, "%d", patPtr->detail);
		    Tcl_DStringAppend(&ds, buffer, -1);
		}
	    }
	    Tcl_DStringAppend(&ds, ">", 1);
	}
	Tcl_AppendElement(interp, Tcl_DStringValue(&ds));
    }
    Tcl_DStringFree(&ds);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteAllBindings --
 *
 *	Remove all bindings associated with a given object in a
 *	given binding table.
 *
 * Results:
 *	All bindings associated with object are removed from
 *	bindingTable.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteAllBindings(bindingTable, object)
    Tk_BindingTable bindingTable;	/* Table in which to delete
					 * bindings. */
    ClientData object;			/* Token for object. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    register PatSeq *psPtr, *prevPtr;
    PatSeq *nextPtr;
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) object);
    if (hPtr == NULL) {
	return;
    }
    for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr); psPtr != NULL;
	    psPtr = nextPtr) {
	nextPtr  = psPtr->nextObjPtr;

	/*
	 * Be sure to remove each binding from its hash chain in the
	 * pattern table.  If this is the last pattern in the chain,
	 * then delete the hash entry too.
	 */

	prevPtr = (PatSeq *) Tcl_GetHashValue(psPtr->hPtr);
	if (prevPtr == psPtr) {
	    if (psPtr->nextSeqPtr == NULL) {
		Tcl_DeleteHashEntry(psPtr->hPtr);
	    } else {
		Tcl_SetHashValue(psPtr->hPtr, psPtr->nextSeqPtr);
	    }
	} else {
	    for ( ; ; prevPtr = prevPtr->nextSeqPtr) {
		if (prevPtr == NULL) {
		    panic("Tk_DeleteAllBindings couldn't find on hash chain");
		}
		if (prevPtr->nextSeqPtr == psPtr) {
		    prevPtr->nextSeqPtr = psPtr->nextSeqPtr;
		    break;
		}
	    }
	}
	ckfree((char *) psPtr->command);
	ckfree((char *) psPtr);
    }
    Tcl_DeleteHashEntry(hPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_BindEvent --
 *
 *	This procedure is invoked to process an X event.  The
 *	event is added to those recorded for the binding table.
 *	Then each of the objects at *objectPtr is checked in
 *	order to see if it has a binding that matches the recent
 *	events.  If so, that binding is invoked and the rest of
 *	objects are skipped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the command associated with the matching
 *	binding.
 *
 *--------------------------------------------------------------
 */

void
Tk_BindEvent(bindingTable, eventPtr, tkwin, numObjects, objectPtr)
    Tk_BindingTable bindingTable;	/* Table in which to look for
					 * bindings. */
    XEvent *eventPtr;			/* What actually happened. */
    Tk_Window tkwin;			/* Window on display where event
					 * occurred (needed in order to
					 * locate display information). */
    int numObjects;			/* Number of objects at *objectPtr. */
    ClientData *objectPtr;		/* Array of one or more objects
					 * to check for a matching binding. */
{
    BindingTable *bindPtr = (BindingTable *) bindingTable;
    TkDisplay *dispPtr = ((TkWindow *) tkwin)->dispPtr;
    TkDisplay *oldDispPtr;
    ScreenInfo *screenPtr;
    XEvent *ringPtr;
    PatSeq *matchPtr;
    PatternTableKey key;
    Tcl_HashEntry *hPtr;
    int detail, code, oldScreen;
    Tcl_Interp *interp;
    Tcl_DString scripts, savedResult;
    char *p, *end;

    /*
     * Ignore the event completely if it is an Enter, Leave, FocusIn,
     * or FocusOut event with detail NotifyInferior.  The reason for
     * ignoring these events is that we don't want transitions between
     * a window and its children to visible to bindings on the parent:
     * this would cause problems for mega-widgets, since the internal
     * structure of a mega-widget isn't supposed to be visible to
     * people watching the parent.
     */

    if ((eventPtr->type == EnterNotify)  || (eventPtr->type == LeaveNotify)) {
	if (eventPtr->xcrossing.detail == NotifyInferior) {
	    return;
	}
    }
    if ((eventPtr->type == FocusIn)  || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail == NotifyInferior) {
	    return;
	}
    }

    /*
     * Add the new event to the ring of saved events for the
     * binding table.  Two tricky points:
     *
     * 1. Combine consecutive MotionNotify events.  Do this by putting
     *    the new event *on top* of the previous event.
     * 2. If a modifier key is held down, it auto-repeats to generate
     *    continuous KeyPress and KeyRelease events.  These can flush
     *    the event ring so that valuable information is lost (such
     *    as repeated button clicks).  To handle this, check for the
     *    special case of a modifier KeyPress arriving when the previous
     *    two events are a KeyRelease and KeyPress of the same key.
     *    If this happens, mark the most recent event (the KeyRelease)
     *    invalid and put the new event on top of the event before that
     *    (the KeyPress).
     */

    if ((eventPtr->type == MotionNotify)
	    && (bindPtr->eventRing[bindPtr->curEvent].type == MotionNotify)) {
	/*
	 * Don't advance the ring pointer.
	 */
    } else if (eventPtr->type == KeyPress) {
	int i;
	for (i = 0; ; i++) {
	    if (i >= dispPtr->numModKeyCodes) {
		goto advanceRingPointer;
	    }
	    if (dispPtr->modKeyCodes[i] == eventPtr->xkey.keycode) {
		break;
	    }
	}
	ringPtr = &bindPtr->eventRing[bindPtr->curEvent];
	if ((ringPtr->type != KeyRelease)
		|| (ringPtr->xkey.keycode != eventPtr->xkey.keycode)) {
	    goto advanceRingPointer;
	}
	if (bindPtr->curEvent <= 0) {
	    i = EVENT_BUFFER_SIZE - 1;
	} else {
	    i = bindPtr->curEvent - 1;
	}
	ringPtr = &bindPtr->eventRing[i];
	if ((ringPtr->type != KeyPress)
		|| (ringPtr->xkey.keycode != eventPtr->xkey.keycode)) {
	    goto advanceRingPointer;
	}
	bindPtr->eventRing[bindPtr->curEvent].type = -1;
	bindPtr->curEvent = i;
    } else {
	advanceRingPointer:
	bindPtr->curEvent++;
	if (bindPtr->curEvent >= EVENT_BUFFER_SIZE) {
	    bindPtr->curEvent = 0;
	}
    }
    ringPtr = &bindPtr->eventRing[bindPtr->curEvent];
    memcpy((VOID *) ringPtr, (VOID *) eventPtr, sizeof(XEvent));
    detail = 0;
    bindPtr->detailRing[bindPtr->curEvent] = 0;
    if ((ringPtr->type == KeyPress) || (ringPtr->type == KeyRelease)) {
	detail = (int) GetKeySym(dispPtr, ringPtr);
	if (detail == NoSymbol) {
	    detail = 0;
	}
    } else if ((ringPtr->type == ButtonPress)
	    || (ringPtr->type == ButtonRelease)) {
	detail = ringPtr->xbutton.button;
    }
    bindPtr->detailRing[bindPtr->curEvent] = detail;

    /*
     * Loop over all the objects, finding the binding script for each
     * one.  Append all of the binding scripts, with %-sequences expanded,
     * to "scripts", with null characters separating the scripts for
     * each object.
     */

    Tcl_DStringInit(&scripts);
    for ( ; numObjects > 0; numObjects--, objectPtr++) {

	/*
	 * Match the new event against those recorded in the
	 * pattern table, saving the longest matching pattern.
	 * For events with details (button and key events) first
	 * look for a binding for the specific key or button.
	 * If none is found, then look for a binding for all
	 * keys or buttons (detail of 0).
	 */
    
	matchPtr = NULL;
	key.object = *objectPtr;
	key.type = ringPtr->type;
	key.detail = detail;
	hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &key);
	if (hPtr != NULL) {
	    matchPtr = MatchPatterns(dispPtr, bindPtr,
		    (PatSeq *) Tcl_GetHashValue(hPtr));
	}
	if ((detail != 0) && (matchPtr == NULL)) {
	    key.detail = 0;
	    hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &key);
	    if (hPtr != NULL) {
		matchPtr = MatchPatterns(dispPtr, bindPtr,
			(PatSeq *) Tcl_GetHashValue(hPtr));
	    }
	}
    
	if (matchPtr != NULL) {
	    ExpandPercents((TkWindow *) tkwin, matchPtr->command, eventPtr,
		    (KeySym) detail, &scripts);
	    Tcl_DStringAppend(&scripts, "", 1);
	}
    }
    if (Tcl_DStringLength(&scripts) == 0) {
	return;
    }

    /*
     * Now go back through and evaluate the script for each object,
     * in order, dealing with "break" and "continue" exceptions
     * appropriately.
     *
     * There are two tricks here:
     * 1. Bindings can be invoked from in the middle of Tcl commands,
     *    where interp->result is significant (for example, a widget
     *    might be deleted because of an error in creating it, so the
     *    result contains an error message that is eventually going to
     *    be returned by the creating command).  To preserve the result,
     *    we save it in a dynamic string.
     * 2. The binding's action can potentially delete the binding,
     *    so bindPtr may not point to anything valid once the action
     *    completes.  Thus we have to save bindPtr->interp in a
     *    local variable in order to restore the result.
     */

    interp = bindPtr->interp;
    Tcl_DStringInit(&savedResult);

    /*
     * Save information about the current screen, then invoke a script
     * if the screen has changed.
     */

    Tcl_DStringGetResult(interp, &savedResult);
    screenPtr = (ScreenInfo *) Tcl_GetAssocData(interp, "tkBind",
	    (Tcl_InterpDeleteProc **) NULL);
    if (screenPtr == NULL) {
	screenPtr = (ScreenInfo *) ckalloc(sizeof(ScreenInfo));
	screenPtr->curDispPtr = NULL;
	screenPtr->curScreenIndex = -1;
	screenPtr->bindingDepth = 0;
	Tcl_SetAssocData(interp, "tkBind", FreeScreenInfo,
		(ClientData) screenPtr);
    }
    oldDispPtr = screenPtr->curDispPtr;
    oldScreen = screenPtr->curScreenIndex;
    if ((dispPtr != screenPtr->curDispPtr)
	    || (Tk_ScreenNumber(tkwin) != screenPtr->curScreenIndex)) {
	screenPtr->curDispPtr = dispPtr;
	screenPtr->curScreenIndex = Tk_ScreenNumber(tkwin);
	ChangeScreen(interp, dispPtr->name, screenPtr->curScreenIndex);
    }

    p = Tcl_DStringValue(&scripts);
    end = p + Tcl_DStringLength(&scripts);
    while (p != end) {
	screenPtr->bindingDepth += 1;
	Tcl_AllowExceptions(interp);
	code = Tcl_GlobalEval(interp, p);
	screenPtr->bindingDepth -= 1;
	if (code != TCL_OK) {
	    if (code == TCL_CONTINUE) {
		/*
		 * Do nothing:  just go on to the next script.
		 */
	    } else if (code == TCL_BREAK) {
		break;
	    } else {
		Tcl_AddErrorInfo(interp, "\n    (command bound to event)");
		Tcl_BackgroundError(interp);
		break;
	    }
	}

	/*
	 * Skip over the current script and its terminating null character.
	 */

	while (*p != 0) {
	    p++;
	}
	p++;
    }
    if ((screenPtr->bindingDepth != 0) &&
            ((oldDispPtr != screenPtr->curDispPtr)
                    || (oldScreen != screenPtr->curScreenIndex))) {

	/*
	 * Some other binding script is currently executing, but its
	 * screen is no longer current.  Change the current display
	 * back again.
	 */

	screenPtr->curDispPtr = oldDispPtr;
	screenPtr->curScreenIndex = oldScreen;
	ChangeScreen(interp, oldDispPtr->name, oldScreen);
    }
    Tcl_DStringResult(interp, &savedResult);
    Tcl_DStringFree(&scripts);
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeScreen --
 *
 *	This procedure is invoked whenever the current screen changes
 *	in an application.  It invokes a Tcl procedure named
 *	"tkScreenChanged", passing it the screen name as argument.
 *	tkScreenChanged does things like making the tkPriv variable
 *	point to an array for the current display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what tkScreenChanged does.  If an error occurs
 *	them tkError will be invoked.
 *
 *----------------------------------------------------------------------
 */

static void
ChangeScreen(interp, dispName, screenIndex)
    Tcl_Interp *interp;			/* Interpreter in which to invoke
					 * command. */
    char *dispName;			/* Name of new display. */
    int screenIndex;			/* Index of new screen. */
{
    Tcl_DString cmd;
    int code;
    char screen[30];

    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, "tkScreenChanged ", 16);
    Tcl_DStringAppend(&cmd, dispName, -1);
    sprintf(screen, ".%d", screenIndex);
    Tcl_DStringAppend(&cmd, screen, -1);
    code = Tcl_GlobalEval(interp, Tcl_DStringValue(&cmd));
    if (code != TCL_OK) {
	Tcl_AddErrorInfo(interp,
		"\n    (changing screen in event binding)");
	Tcl_BackgroundError(interp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeScreenInfo --
 *
 *	This procedure is invoked when an interpreter is deleted in
 *	order to free the ScreenInfo structure associated with the
 *	"tkBind" AssocData.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage is freed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeScreenInfo(clientData, interp)
    ClientData clientData;	/* Pointer to ScreenInfo structure. */
    Tcl_Interp *interp;		/* Interpreter that is being deleted. */
{
    ckfree((char *) clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * FindSequence --
 *
 *	Find the entry in a binding table that corresponds to a
 *	particular pattern string, and return a pointer to that
 *	entry.
 *
 * Results:
 *	The return value is normally a pointer to the PatSeq
 *	in patternTable that corresponds to eventString.  If an error
 *	was found while parsing eventString, or if "create" is 0 and
 *	no pattern sequence previously existed, then NULL is returned
 *	and interp->result contains a message describing the problem.
 *	If no pattern sequence previously existed for eventString, then
 *	a new one is created with a NULL command field.  In a successful
 *	return, *maskPtr is filled in with a mask of the event types
 *	on which the pattern sequence depends.
 *
 * Side effects:
 *	A new pattern sequence may be created.
 *
 *----------------------------------------------------------------------
 */

static PatSeq *
FindSequence(interp, bindPtr, object, eventString, create, maskPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error
				 * reporting. */
    BindingTable *bindPtr;	/* Table to use for lookup. */
    ClientData object;		/* Token for object(s) with which binding
				 * is associated. */
    char *eventString;		/* String description of pattern to
				 * match on.  See user documentation
				 * for details. */
    int create;			/* 0 means don't create the entry if
				 * it doesn't already exist.   Non-zero
				 * means create. */
    unsigned long *maskPtr;	/* *maskPtr is filled in with the event
				 * types on which this pattern sequence
				 * depends. */

{
    Pattern pats[EVENT_BUFFER_SIZE];
    int numPats;
    register char *p;
    register Pattern *patPtr;
    register PatSeq *psPtr;
    register Tcl_HashEntry *hPtr;
#define FIELD_SIZE 48
    char field[FIELD_SIZE];
    int flags, count, new;
    size_t sequenceSize;
    unsigned long eventMask;
    PatternTableKey key;

    /*
     *-------------------------------------------------------------
     * Step 1: parse the pattern string to produce an array
     * of Patterns.  The array is generated backwards, so
     * that the lowest-indexed pattern corresponds to the last
     * event that must occur.
     *-------------------------------------------------------------
     */

    p = eventString;
    flags = 0;
    eventMask = 0;
    for (numPats = 0, patPtr = &pats[EVENT_BUFFER_SIZE-1];
	    numPats < EVENT_BUFFER_SIZE;
	    numPats++, patPtr--) {
	patPtr->eventType = -1;
	patPtr->needMods = 0;
	patPtr->detail = 0;
	while (isspace(UCHAR(*p))) {
	    p++;
	}
	if (*p == '\0') {
	    break;
	}

	/*
	 * Handle simple ASCII characters.
	 */

	if (*p != '<') {
	    char string[2];

	    patPtr->eventType = KeyPress;
	    eventMask |= KeyPressMask;
	    string[0] = *p;
	    string[1] = 0;
	    patPtr->detail = TkStringToKeysym(string);
	    if (patPtr->detail == NoSymbol) {
		if (isprint(UCHAR(*p))) {
		    patPtr->detail = *p;
		} else {
		    sprintf(interp->result,
			    "bad ASCII character 0x%x", (unsigned char) *p);
		    return NULL;
		}
	    }
	    p++;
	    continue;
	}

	/*
	 * A fancier event description.  Must consist of
	 * 1. open angle bracket.
	 * 2. any number of modifiers, each followed by spaces
	 *    or dashes.
	 * 3. an optional event name.
	 * 4. an option button or keysym name.  Either this or
	 *    item 3 *must* be present;  if both are present
	 *    then they are separated by spaces or dashes.
	 * 5. a close angle bracket.
	 */

	count = 1;
	p++;
	while (1) {
	    register ModInfo *modPtr;
	    p = GetField(p, field, FIELD_SIZE);
	    hPtr = Tcl_FindHashEntry(&modTable, field);
	    if (hPtr == NULL) {
		break;
	    }
	    modPtr = (ModInfo *) Tcl_GetHashValue(hPtr);
	    patPtr->needMods |= modPtr->mask;
	    if (modPtr->flags & (DOUBLE|TRIPLE)) {
		flags |= PAT_NEARBY;
		if (modPtr->flags & DOUBLE) {
		    count = 2;
		} else {
		    count = 3;
		}
	    }
	    while ((*p == '-') || isspace(UCHAR(*p))) {
		p++;
	    }
	}
	hPtr = Tcl_FindHashEntry(&eventTable, field);
	if (hPtr != NULL) {
	    register EventInfo *eiPtr;
	    eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);
	    patPtr->eventType = eiPtr->type;
	    eventMask |= eiPtr->eventMask;
	    while ((*p == '-') || isspace(UCHAR(*p))) {
		p++;
	    }
	    p = GetField(p, field, FIELD_SIZE);
	}
	if (*field != '\0') {
	    if ((*field >= '1') && (*field <= '5') && (field[1] == '\0')) {
		if (patPtr->eventType == -1) {
		    patPtr->eventType = ButtonPress;
		    eventMask |= ButtonPressMask;
		} else if ((patPtr->eventType == KeyPress)
			|| (patPtr->eventType == KeyRelease)) {
		    goto getKeysym;
		} else if ((patPtr->eventType != ButtonPress)
			&& (patPtr->eventType != ButtonRelease)) {
		    Tcl_AppendResult(interp, "specified button \"", field,
			    "\" for non-button event", (char *) NULL);
		    return NULL;
		}
		patPtr->detail = (*field - '0');
	    } else {
		getKeysym:
		patPtr->detail = TkStringToKeysym(field);
		if (patPtr->detail == NoSymbol) {
		    Tcl_AppendResult(interp, "bad event type or keysym \"",
			    field, "\"", (char *) NULL);
		    return NULL;
		}
		if (patPtr->eventType == -1) {
		    patPtr->eventType = KeyPress;
		    eventMask |= KeyPressMask;
		} else if ((patPtr->eventType != KeyPress)
			&& (patPtr->eventType != KeyRelease)) {
		    Tcl_AppendResult(interp, "specified keysym \"", field,
			    "\" for non-key event", (char *) NULL);
		    return NULL;
		}
	    }
	} else if (patPtr->eventType == -1) {
	    interp->result = "no event type or button # or keysym";
	    return NULL;
	}
	while ((*p == '-') || isspace(UCHAR(*p))) {
	    p++;
	}
	if (*p != '>') {
	    interp->result = "missing \">\" in binding";
	    return NULL;
	}
	p++;

	/*
	 * Replicate events for DOUBLE and TRIPLE.
	 */

	if ((count > 1) && (numPats < EVENT_BUFFER_SIZE-1)) {
	    patPtr[-1] = patPtr[0];
	    patPtr--;
	    numPats++;
	    if ((count == 3) && (numPats < EVENT_BUFFER_SIZE-1)) {
		patPtr[-1] = patPtr[0];
		patPtr--;
		numPats++;
	    }
	}
    }

    /*
     *-------------------------------------------------------------
     * Step 2: find the sequence in the binding table if it exists,
     * and add a new sequence to the table if it doesn't.
     *-------------------------------------------------------------
     */

    if (numPats == 0) {
	interp->result = "no events specified in binding";
	return NULL;
    }
    patPtr = &pats[EVENT_BUFFER_SIZE-numPats];
    key.object = object;
    key.type = patPtr->eventType;
    key.detail = patPtr->detail;
    hPtr = Tcl_CreateHashEntry(&bindPtr->patternTable, (char *) &key, &new);
    sequenceSize = numPats*sizeof(Pattern);
    if (!new) {
	for (psPtr = (PatSeq *) Tcl_GetHashValue(hPtr); psPtr != NULL;
		psPtr = psPtr->nextSeqPtr) {
	    if ((numPats == psPtr->numPats)
		    && ((flags & PAT_NEARBY) == (psPtr->flags & PAT_NEARBY))
		    && (memcmp((char *) patPtr, (char *) psPtr->pats,
		    sequenceSize) == 0)) {
		goto done;
	    }
	}
    }
    if (!create) {
	if (new) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	Tcl_AppendResult(interp, "no binding exists for \"",
		eventString, "\"", (char *) NULL);
	return NULL;
    }
    psPtr = (PatSeq *) ckalloc((unsigned) (sizeof(PatSeq)
	    + (numPats-1)*sizeof(Pattern)));
    psPtr->numPats = numPats;
    psPtr->command = NULL;
    psPtr->flags = flags;
    psPtr->nextSeqPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
    psPtr->hPtr = hPtr;
    Tcl_SetHashValue(hPtr, psPtr);

    /*
     * Link the pattern into the list associated with the object.
     */

    psPtr->object = object;
    hPtr = Tcl_CreateHashEntry(&bindPtr->objectTable, (char *) object, &new);
    if (new) {
	psPtr->nextObjPtr = NULL;
    } else {
	psPtr->nextObjPtr = (PatSeq *) Tcl_GetHashValue(hPtr);
    }
    Tcl_SetHashValue(hPtr, psPtr);

    memcpy((VOID *) psPtr->pats, (VOID *) patPtr, sequenceSize);

    done:
    *maskPtr = eventMask;
    return psPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetField --
 *
 *	Used to parse pattern descriptions.  Copies up to
 *	size characters from p to copy, stopping at end of
 *	string, space, "-", ">", or whenever size is
 *	exceeded.
 *
 * Results:
 *	The return value is a pointer to the character just
 *	after the last one copied (usually "-" or space or
 *	">", but could be anything if size was exceeded).
 *	Also places NULL-terminated string (up to size
 *	character, including NULL), at copy.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
GetField(p, copy, size)
    register char *p;		/* Pointer to part of pattern. */
    register char *copy;	/* Place to copy field. */
    int size;			/* Maximum number of characters to
				 * copy. */
{
    while ((*p != '\0') && !isspace(UCHAR(*p)) && (*p != '>')
	    && (*p != '-') && (size > 1)) {
	*copy = *p;
	p++;
	copy++;
	size--;
    }
    *copy = '\0';
    return p;
}

/*
 *----------------------------------------------------------------------
 *
 * GetKeySym --
 *
 *	Given an X KeyPress or KeyRelease event, map the
 *	keycode in the event into a KeySym.
 *
 * Results:
 *	The return value is the KeySym corresponding to
 *	eventPtr, or NoSymbol if no matching Keysym could be
 *	found.
 *
 * Side effects:
 *	In the first call for a given display, keycode-to-
 *	KeySym maps get loaded.
 *
 *----------------------------------------------------------------------
 */

static KeySym
GetKeySym(dispPtr, eventPtr)
    register TkDisplay *dispPtr;	/* Display in which to
					 * map keycode. */
    register XEvent *eventPtr;		/* Description of X event. */
{
    KeySym sym;
    int index;

    /*
     * Refresh the mapping information if it's stale
     */

    if (dispPtr->bindInfoStale) {
	InitKeymapInfo(dispPtr);
    }

    /*
     * Figure out which of the four slots in the keymap vector to
     * use for this key.  Refer to Xlib documentation for more info
     * on how this computation works.
     */

    index = 0;
    if (eventPtr->xkey.state & dispPtr->modeModMask) {
	index = 2;
    }
    if ((eventPtr->xkey.state & ShiftMask)
	    || ((dispPtr->lockUsage != LU_IGNORE)
	    && (eventPtr->xkey.state & LockMask))) {
	index += 1;
    }
    sym = XKeycodeToKeysym(dispPtr->display, eventPtr->xkey.keycode, index);

    /*
     * Special handling:  if the key was shifted because of Lock, but
     * lock is only caps lock, not shift lock, and the shifted keysym
     * isn't upper-case alphabetic, then switch back to the unshifted
     * keysym.
     */

    if ((index & 1) && !(eventPtr->xkey.state & ShiftMask)
	    && (dispPtr->lockUsage == LU_CAPS)) {
	if (!(((sym >= XK_A) && (sym <= XK_Z))
		|| ((sym >= XK_Agrave) && (sym <= XK_Odiaeresis))
		|| ((sym >= XK_Ooblique) && (sym <= XK_Thorn)))) {
	    index &= ~1;
	    sym = XKeycodeToKeysym(dispPtr->display, eventPtr->xkey.keycode,
		    index);
	}
    }

    /*
     * Another bit of special handling:  if this is a shifted key and there
     * is no keysym defined, then use the keysym for the unshifted key.
     */

    if ((index & 1) && (sym == NoSymbol)) {
	sym = XKeycodeToKeysym(dispPtr->display, eventPtr->xkey.keycode,
		    index & ~1);
    }
    return sym;
}

/*
 *----------------------------------------------------------------------
 *
 * MatchPatterns --
 *
 *	Given a list of pattern sequences and a list of
 *	recent events, return a pattern sequence that matches
 *	the event list.
 *
 * Results:
 *	The return value is NULL if no pattern matches the
 *	recent events from bindPtr.  If one or more patterns
 *	matches, then the longest (or most specific) matching
 *	pattern is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static PatSeq *
MatchPatterns(dispPtr, bindPtr, psPtr)
    TkDisplay *dispPtr;		/* Display from which the event came. */
    BindingTable *bindPtr;	/* Information about binding table, such
				 * as ring of recent events. */
    register PatSeq *psPtr;	/* List of pattern sequences. */
{
    register PatSeq *bestPtr = NULL;

    /*
     * Iterate over all the pattern sequences.
     */

    for ( ; psPtr != NULL; psPtr = psPtr->nextSeqPtr) {
	register XEvent *eventPtr;
	register Pattern *patPtr;
	Window window;
	int *detailPtr;
	int patCount, ringCount, flags, state;
	int modMask;

	/*
	 * Iterate over all the patterns in a sequence to be
	 * sure that they all match.
	 */

	eventPtr = &bindPtr->eventRing[bindPtr->curEvent];
	detailPtr = &bindPtr->detailRing[bindPtr->curEvent];
	window = eventPtr->xany.window;
	patPtr = psPtr->pats;
	patCount = psPtr->numPats;
	ringCount = EVENT_BUFFER_SIZE;
	while (patCount > 0) {
	    if (ringCount <= 0) {
		goto nextSequence;
	    }
	    if (eventPtr->xany.type != patPtr->eventType) {
		/*
		 * Most of the event types are considered superfluous
		 * in that they are ignored if they occur in the middle
		 * of a pattern sequence and have mismatching types.  The
		 * only ones that cannot be ignored are ButtonPress and
		 * ButtonRelease events (if the next event in the pattern
		 * is a KeyPress or KeyRelease) and KeyPress and KeyRelease
		 * events (if the next pattern event is a ButtonPress or
		 * ButtonRelease).  Here are some tricky cases to consider:
		 * 1. Double-Button or Double-Key events.
		 * 2. Double-ButtonRelease or Double-KeyRelease events.
		 * 3. The arrival of various events like Enter and Leave
		 *    and FocusIn and GraphicsExpose between two button
		 *    presses or key presses.
		 * 4. Modifier keys like Shift and Control shouldn't
		 *    generate conflicts with button events.
		 */

		if ((patPtr->eventType == KeyPress)
			|| (patPtr->eventType == KeyRelease)) {
		    if ((eventPtr->xany.type == ButtonPress)
			    || (eventPtr->xany.type == ButtonRelease)) {
			goto nextSequence;
		    }
		} else if ((patPtr->eventType == ButtonPress)
			|| (patPtr->eventType == ButtonRelease)) {
		    if ((eventPtr->xany.type == KeyPress)
			    || (eventPtr->xany.type == KeyRelease)) {
			int i;

			/*
			 * Ignore key events if they are modifier keys.
			 */

			for (i = 0; i < dispPtr->numModKeyCodes; i++) {
			    if (dispPtr->modKeyCodes[i]
				    == eventPtr->xkey.keycode) {
				/*
				 * This key is a modifier key, so ignore it.
				 */
				goto nextEvent;
			    }
			}
			goto nextSequence;
		    }
		}
		goto nextEvent;
	    }
	    if (eventPtr->xany.window != window) {
		goto nextSequence;
	    }

	    /*
	     * Note: it's important for the keysym check to go before
	     * the modifier check, so we can ignore unwanted modifier
	     * keys before choking on the modifier check.
	     */

	    if ((patPtr->detail != 0)
		    && (patPtr->detail != *detailPtr)) {
		/*
		 * The detail appears not to match.  However, if the event
		 * is a KeyPress for a modifier key then just ignore the
		 * event.  Otherwise event sequences like "aD" never match
		 * because the shift key goes down between the "a" and the
		 * "D".
		 */

		if (eventPtr->xany.type == KeyPress) {
		    int i;

		    for (i = 0; i < dispPtr->numModKeyCodes; i++) {
			if (dispPtr->modKeyCodes[i] == eventPtr->xkey.keycode) {
			    goto nextEvent;
			}
		    }
		}
		goto nextSequence;
	    }
	    flags = flagArray[eventPtr->type];
	    if (flags & KEY_BUTTON_MOTION) {
		state = eventPtr->xkey.state;
	    } else if (flags & CROSSING) {
		state = eventPtr->xcrossing.state;
	    } else {
		state = 0;
	    }
	    if (patPtr->needMods != 0) {
		modMask = patPtr->needMods;
		if ((modMask & META_MASK) && (dispPtr->metaModMask != 0)) {
		    modMask = (modMask & ~META_MASK) | dispPtr->metaModMask;
		}
		if ((modMask & ALT_MASK) && (dispPtr->altModMask != 0)) {
		    modMask = (modMask & ~ALT_MASK) | dispPtr->altModMask;
		}
		if ((state & modMask) != modMask) {
		    goto nextSequence;
		}
	    }
	    if (psPtr->flags & PAT_NEARBY) {
		register XEvent *firstPtr;
		int timeDiff;

		firstPtr = &bindPtr->eventRing[bindPtr->curEvent];
		timeDiff = (Time) firstPtr->xkey.time - eventPtr->xkey.time;
		if ((firstPtr->xkey.x_root
			    < (eventPtr->xkey.x_root - NEARBY_PIXELS))
			|| (firstPtr->xkey.x_root
			    > (eventPtr->xkey.x_root + NEARBY_PIXELS))
			|| (firstPtr->xkey.y_root
			    < (eventPtr->xkey.y_root - NEARBY_PIXELS))
			|| (firstPtr->xkey.y_root
			    > (eventPtr->xkey.y_root + NEARBY_PIXELS))
			|| (timeDiff > NEARBY_MS)) {
		    goto nextSequence;
		}
	    }
	    patPtr++;
	    patCount--;
	    nextEvent:
	    if (eventPtr == bindPtr->eventRing) {
		eventPtr = &bindPtr->eventRing[EVENT_BUFFER_SIZE-1];
		detailPtr = &bindPtr->detailRing[EVENT_BUFFER_SIZE-1];
	    } else {
		eventPtr--;
		detailPtr--;
	    }
	    ringCount--;
	}

	/*
	 * This sequence matches.  If we've already got another match,
	 * pick whichever is most specific.  Detail is most important,
	 * then needMods.
	 */

	if (bestPtr != NULL) {
	    register Pattern *patPtr2;
	    int i;

	    if (psPtr->numPats != bestPtr->numPats) {
		if (bestPtr->numPats > psPtr->numPats) {
		    goto nextSequence;
		} else {
		    goto newBest;
		}
	    }
	    for (i = 0, patPtr = psPtr->pats, patPtr2 = bestPtr->pats;
		    i < psPtr->numPats; i++, patPtr++, patPtr2++) {
		if (patPtr->detail != patPtr2->detail) {
		    if (patPtr->detail == 0) {
			goto nextSequence;
		    } else {
			goto newBest;
		    }
		}
		if (patPtr->needMods != patPtr2->needMods) {
		    if ((patPtr->needMods & patPtr2->needMods)
			    == patPtr->needMods) {
			goto nextSequence;
		    } else if ((patPtr->needMods & patPtr2->needMods)
			    == patPtr2->needMods) {
			goto newBest;
		    }
		}
	    }
	    goto nextSequence;	/* Tie goes to newest pattern. */
	}
	newBest:
	bestPtr = psPtr;

	nextSequence: continue;
    }
    return bestPtr;
}

/*
 *--------------------------------------------------------------
 *
 * ExpandPercents --
 *
 *	Given a command and an event, produce a new command
 *	by replacing % constructs in the original command
 *	with information from the X event.
 *
 * Results:
 *	The new expanded command is appended to the dynamic string
 *	given by dsPtr.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
ExpandPercents(winPtr, before, eventPtr, keySym, dsPtr)
    TkWindow *winPtr;		/* Window where event occurred:  needed to
				 * get input context. */
    register char *before;	/* Command containing percent
				 * expressions to be replaced. */
    register XEvent *eventPtr;	/* X event containing information
				 * to be used in % replacements. */
    KeySym keySym;		/* KeySym: only relevant for
				 * KeyPress and KeyRelease events). */
    Tcl_DString *dsPtr;		/* Dynamic string in which to append
				 * new command. */
{
    int spaceNeeded, cvtFlags;	/* Used to substitute string as proper Tcl
				 * list element. */
    int number, flags, length;
#define NUM_SIZE 40
    register char *string;
    char numStorage[NUM_SIZE+1];

    if (eventPtr->type < TK_LASTEVENT) {
	flags = flagArray[eventPtr->type];
    } else {
	flags = 0;
    }
    while (1) {
	/*
	 * Find everything up to the next % character and append it
	 * to the result string.
	 */

	for (string = before; (*string != 0) && (*string != '%'); string++) {
	    /* Empty loop body. */
	}
	if (string != before) {
	    Tcl_DStringAppend(dsPtr, before, string-before);
	    before = string;
	}
	if (*before == 0) {
	    break;
	}

	/*
	 * There's a percent sequence here.  Process it.
	 */

	number = 0;
	string = "??";
	switch (before[1]) {
	    case '#':
		number = eventPtr->xany.serial;
		goto doNumber;
	    case 'a':
		sprintf(numStorage, "0x%x", (int) eventPtr->xconfigure.above);
		string = numStorage;
		goto doString;
	    case 'b':
		number = eventPtr->xbutton.button;
		goto doNumber;
	    case 'c':
		if (flags & EXPOSE) {
		    number = eventPtr->xexpose.count;
		} else if (flags & MAPPING) {
		    number = eventPtr->xmapping.count;
		}
		goto doNumber;
	    case 'd':
		if (flags & (CROSSING|FOCUS)) {
		    if (flags & FOCUS) {
			number = eventPtr->xfocus.detail;
		    } else {
			number = eventPtr->xcrossing.detail;
		    }
		    switch (number) {
			case NotifyAncestor:
			    string = "NotifyAncestor";
			    break;
			case NotifyVirtual:
			    string = "NotifyVirtual";
			    break;
			case NotifyInferior:
			    string = "NotifyInferior";
			    break;
			case NotifyNonlinear:
			    string = "NotifyNonlinear";
			    break;
			case NotifyNonlinearVirtual:
			    string = "NotifyNonlinearVirtual";
			    break;
			case NotifyPointer:
			    string = "NotifyPointer";
			    break;
			case NotifyPointerRoot:
			    string = "NotifyPointerRoot";
			    break;
			case NotifyDetailNone:
			    string = "NotifyDetailNone";
			    break;
		    }
		} else if (flags & CONFIG_REQ) {
		    switch (eventPtr->xconfigurerequest.detail) {
			case Above:
			    string = "Above";
			    break;
			case Below:
			    string = "Below";
			    break;
			case TopIf:
			    string = "TopIf";
			    break;
			case BottomIf:
			    string = "BottomIf";
			    break;
			case Opposite:
			    string = "Opposite";
			    break;
		    }
		}
		goto doString;
	    case 'f':
		number = eventPtr->xcrossing.focus;
		goto doNumber;
	    case 'h':
		if (flags & EXPOSE) {
		    number = eventPtr->xexpose.height;
		} else if (flags & (CONFIG|CONFIG_REQ)) {
		    number = eventPtr->xconfigure.height;
		} else if (flags & RESIZE_REQ) {
		    number = eventPtr->xresizerequest.height;
		}
		goto doNumber;
	    case 'k':
		number = eventPtr->xkey.keycode;
		goto doNumber;
	    case 'm':
		if (flags & CROSSING) {
		    number = eventPtr->xcrossing.mode;
		} else if (flags & FOCUS) {
		    number = eventPtr->xfocus.mode;
		}
		switch (number) {
		    case NotifyNormal:
			string = "NotifyNormal";
			break;
		    case NotifyGrab:
			string = "NotifyGrab";
			break;
		    case NotifyUngrab:
			string = "NotifyUngrab";
			break;
		    case NotifyWhileGrabbed:
			string = "NotifyWhileGrabbed";
			break;
		}
		goto doString;
	    case 'o':
		if (flags & CREATE) {
		    number = eventPtr->xcreatewindow.override_redirect;
		} else if (flags & MAP) {
		    number = eventPtr->xmap.override_redirect;
		} else if (flags & REPARENT) {
		    number = eventPtr->xreparent.override_redirect;
		} else if (flags & CONFIG) {
		    number = eventPtr->xconfigure.override_redirect;
		}
		goto doNumber;
	    case 'p':
		switch (eventPtr->xcirculate.place) {
		    case PlaceOnTop:
			string = "PlaceOnTop";
			break;
		    case PlaceOnBottom:
			string = "PlaceOnBottom";
			break;
		}
		goto doString;
	    case 's':
		if (flags & KEY_BUTTON_MOTION) {
		    number = eventPtr->xkey.state;
		} else if (flags & CROSSING) {
		    number = eventPtr->xcrossing.state;
		} else if (flags & VISIBILITY) {
		    switch (eventPtr->xvisibility.state) {
			case VisibilityUnobscured:
			    string = "VisibilityUnobscured";
			    break;
			case VisibilityPartiallyObscured:
			    string = "VisibilityPartiallyObscured";
			    break;
			case VisibilityFullyObscured:
			    string = "VisibilityFullyObscured";
			    break;
		    }
		    goto doString;
		}
		goto doNumber;
	    case 't':
		if (flags & PROP) {
		    number = (int) eventPtr->xproperty.time;
		} else if (flags & SEL_CLEAR) {
		    number = (int) eventPtr->xselectionclear.time;
		} else if (flags & KEY_BUTTON_MOTION) {
		    number = (int) eventPtr->xkey.time;
		} else if (flags & SEL_REQ) {
		    number = (int) eventPtr->xselectionrequest.time;
		} else if (flags & SEL_NOTIFY) {
		    number = (int) eventPtr->xselection.time;
		} else if (flags & CROSSING) {
		    number = (int) eventPtr->xcrossing.time;
		}
		goto doNumber;
	    case 'v':
		number = eventPtr->xconfigurerequest.value_mask;
		goto doNumber;
	    case 'w':
		if (flags & EXPOSE) {
		    number = eventPtr->xexpose.width;
		} else if (flags & (CONFIG|CONFIG_REQ)) {
		    number = eventPtr->xconfigure.width;
		} else if (flags & RESIZE_REQ) {
		    number = eventPtr->xresizerequest.width;
		}
		goto doNumber;
	    case 'x':
		if (flags & KEY_BUTTON_MOTION) {
		    number = eventPtr->xkey.x;
		} else if (flags & EXPOSE) {
		    number = eventPtr->xexpose.x;
		} else if (flags & (CREATE|CONFIG|GRAVITY|CONFIG_REQ)) {
		    number = eventPtr->xcreatewindow.x;
		} else if (flags & REPARENT) {
		    number = eventPtr->xreparent.x;
		} else if (flags & CROSSING) {
		    number = eventPtr->xcrossing.x;
		}
		goto doNumber;
	    case 'y':
		if (flags & KEY_BUTTON_MOTION) {
		    number = eventPtr->xkey.y;
		} else if (flags & EXPOSE) {
		    number = eventPtr->xexpose.y;
		} else if (flags & (CREATE|CONFIG|GRAVITY|CONFIG_REQ)) {
		    number = eventPtr->xcreatewindow.y;
		} else if (flags & REPARENT) {
		    number = eventPtr->xreparent.y;
		} else if (flags & CROSSING) {
		    number = eventPtr->xcrossing.y;

		}
		goto doNumber;
	    case 'A':
		if ((eventPtr->type == KeyPress)
			|| (eventPtr->type == KeyRelease)) {
		    int numChars;

		    /*
		     * If we're using input methods and this is a keypress
		     * event, invoke XmbLookupString.  Otherwise just use
		     * the older XLookupString.
		     */

#ifdef TK_USE_INPUT_METHODS
		    Status status;
		    if ((winPtr->inputContext != NULL)
			    && (eventPtr->type == KeyPress)) {
                        numChars = XmbLookupString(winPtr->inputContext,
                                &eventPtr->xkey, numStorage, NUM_SIZE,
                                (KeySym *) NULL, &status);
			if ((status != XLookupChars)
				&& (status != XLookupBoth)) {
			    numChars = 0;
			}
                    } else {
                        numChars = XLookupString(&eventPtr->xkey, numStorage,
                                NUM_SIZE, (KeySym *) NULL,
                                (XComposeStatus *) NULL);
		    }
#else /* TK_USE_INPUT_METHODS */
		    numChars = XLookupString(&eventPtr->xkey, numStorage,
			    NUM_SIZE, (KeySym *) NULL,
			    (XComposeStatus *) NULL);
#endif /* TK_USE_INPUT_METHODS */
		    numStorage[numChars] = '\0';
		    string = numStorage;
		}
		goto doString;
	    case 'B':
		number = eventPtr->xcreatewindow.border_width;
		goto doNumber;
	    case 'E':
		number = (int) eventPtr->xany.send_event;
		goto doNumber;
	    case 'K':
		if ((eventPtr->type == KeyPress)
			|| (eventPtr->type == KeyRelease)) {
		    char *name;

		    name = TkKeysymToString(keySym);
		    if (name != NULL) {
			string = name;
		    }
		}
		goto doString;
	    case 'N':
		number = (int) keySym;
		goto doNumber;
	    case 'R':
		number = (int) eventPtr->xkey.root;
		goto doNumber;
	    case 'S':
		sprintf(numStorage, "0x%x", (int) eventPtr->xkey.subwindow);
		string = numStorage;
		goto doString;
	    case 'T':
		number = eventPtr->type;
		goto doNumber;
	    case 'W': {
		Tk_Window tkwin;

		tkwin = Tk_IdToWindow(eventPtr->xany.display,
			eventPtr->xany.window);
		if (tkwin != NULL) {
		    string = Tk_PathName(tkwin);
		} else {
		    string = "??";
		}
		goto doString;
	    }
	    case 'X': {
		Tk_Window tkwin;
		int x, y;
		int width, height;

		number = eventPtr->xkey.x_root;
		tkwin = Tk_IdToWindow(eventPtr->xany.display,
			eventPtr->xany.window);
		if (tkwin != NULL) {
		    Tk_GetVRootGeometry(tkwin, &x, &y, &width, &height);
		    number -= x;
		}
		goto doNumber;
	    }
	    case 'Y': {
		Tk_Window tkwin;
		int x, y;
		int width, height;

		number = eventPtr->xkey.y_root;
		tkwin = Tk_IdToWindow(eventPtr->xany.display,
			eventPtr->xany.window);
		if (tkwin != NULL) {
		    Tk_GetVRootGeometry(tkwin, &x, &y, &width, &height);
		    number -= y;
		}
		goto doNumber;
	    }
	    default:
		numStorage[0] = before[1];
		numStorage[1] = '\0';
		string = numStorage;
		goto doString;
	}

	doNumber:
	sprintf(numStorage, "%d", number);
	string = numStorage;

	doString:
	spaceNeeded = Tcl_ScanElement(string, &cvtFlags);
	length = Tcl_DStringLength(dsPtr);
	Tcl_DStringSetLength(dsPtr, length + spaceNeeded);
	spaceNeeded = Tcl_ConvertElement(string,
		Tcl_DStringValue(dsPtr) + length,
		cvtFlags | TCL_DONT_USE_BRACES);
	Tcl_DStringSetLength(dsPtr, length + spaceNeeded);
	before += 2;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCopyAndGlobalEval --
 *
 *	This procedure makes a copy of a script then calls Tcl_GlobalEval
 *	to evaluate it.  It's used in situations where the execution of
 *	a command may cause the original command string to be reallocated.
 *
 * Results:
 *	Returns the result of evaluating script, including both a standard
 *	Tcl completion code and a string in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkCopyAndGlobalEval(interp, script)
    Tcl_Interp *interp;			/* Interpreter in which to evaluate
					 * script. */
    char *script;			/* Script to evaluate. */
{
    Tcl_DString buffer;
    int code;

    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, script, -1);
    code = Tcl_GlobalEval(interp, Tcl_DStringValue(&buffer));
    Tcl_DStringFree(&buffer);
    return code;
}

/*
 *--------------------------------------------------------------
 *
 * InitKeymapInfo --
 *
 *	This procedure is invoked to scan keymap information
 *	to recompute stuff that's important for binding, such
 *	as the modifier key (if any) that corresponds to "mode
 *	switch".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Keymap-related information in dispPtr is updated.
 *
 *--------------------------------------------------------------
 */

static void
InitKeymapInfo(dispPtr)
    TkDisplay *dispPtr;		/* Display for which to recompute keymap
				 * information. */
{
    XModifierKeymap *modMapPtr;
    register KeyCode *codePtr;
    KeySym keysym;
    int count, i, j, max, arraySize;
#define KEYCODE_ARRAY_SIZE 20

    dispPtr->bindInfoStale = 0;
    modMapPtr = XGetModifierMapping(dispPtr->display);

    /*
     * Check the keycodes associated with the Lock modifier.  If
     * any of them is associated with the XK_Shift_Lock modifier,
     * then Lock has to be interpreted as Shift Lock, not Caps Lock.
     */

    dispPtr->lockUsage = LU_IGNORE;
    codePtr = modMapPtr->modifiermap + modMapPtr->max_keypermod*LockMapIndex;
    for (count = modMapPtr->max_keypermod; count > 0; count--, codePtr++) {
	if (*codePtr == 0) {
	    continue;
	}
	keysym = XKeycodeToKeysym(dispPtr->display, *codePtr, 0);
	if (keysym == XK_Shift_Lock) {
	    dispPtr->lockUsage = LU_SHIFT;
	    break;
	}
	if (keysym == XK_Caps_Lock) {
	    dispPtr->lockUsage = LU_CAPS;
	    break;
	}
    }

    /*
     * Look through the keycodes associated with modifiers to see if
     * the the "mode switch", "meta", or "alt" keysyms are associated
     * with any modifiers.  If so, remember their modifier mask bits.
     */

    dispPtr->modeModMask = 0;
    dispPtr->metaModMask = 0;
    dispPtr->altModMask = 0;
    codePtr = modMapPtr->modifiermap;
    max = 8*modMapPtr->max_keypermod;
    for (i = 0; i < max; i++, codePtr++) {
	if (*codePtr == 0) {
	    continue;
	}
	keysym = XKeycodeToKeysym(dispPtr->display, *codePtr, 0);
	if (keysym == XK_Mode_switch) {
	    dispPtr->modeModMask |= ShiftMask << (i/modMapPtr->max_keypermod);
	}
	if ((keysym == XK_Meta_L) || (keysym == XK_Meta_R)) {
	    dispPtr->metaModMask |= ShiftMask << (i/modMapPtr->max_keypermod);
	}
	if ((keysym == XK_Alt_L) || (keysym == XK_Alt_R)) {
	    dispPtr->altModMask |= ShiftMask << (i/modMapPtr->max_keypermod);
	}
    }

    /*
     * Create an array of the keycodes for all modifier keys.
     */

    if (dispPtr->modKeyCodes != NULL) {
	ckfree((char *) dispPtr->modKeyCodes);
    }
    dispPtr->numModKeyCodes = 0;
    arraySize = KEYCODE_ARRAY_SIZE;
    dispPtr->modKeyCodes = (KeyCode *) ckalloc((unsigned)
	    (KEYCODE_ARRAY_SIZE * sizeof(KeyCode)));
    for (i = 0, codePtr = modMapPtr->modifiermap; i < max; i++, codePtr++) {
	if (*codePtr == 0) {
	    continue;
	}

	/*
	 * Make sure that the keycode isn't already in the array.
	 */

	for (j = 0; j < dispPtr->numModKeyCodes; j++) {
	    if (dispPtr->modKeyCodes[j] == *codePtr) {
		goto nextModCode;
	    }
	}
	if (dispPtr->numModKeyCodes >= arraySize) {
	    KeyCode *new;

	    /*
	     * Ran out of space in the array;  grow it.
	     */

	    arraySize *= 2;
	    new = (KeyCode *) ckalloc((unsigned)
		    (arraySize * sizeof(KeyCode)));
	    memcpy((VOID *) new, (VOID *) dispPtr->modKeyCodes,
		    (dispPtr->numModKeyCodes * sizeof(KeyCode)));
	    ckfree((char *) dispPtr->modKeyCodes);
	    dispPtr->modKeyCodes = new;
	}
	dispPtr->modKeyCodes[dispPtr->numModKeyCodes] = *codePtr;
	dispPtr->numModKeyCodes++;
	nextModCode: continue;
    }
    XFreeModifiermap(modMapPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkStringToKeysym --
 *
 *	This procedure finds the keysym associated with a given keysym
 *	name.
 *
 * Results:
 *	The return value is the keysym that corresponds to name, or
 *	NoSymbol if there is no such keysym.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeySym
TkStringToKeysym(name)
    char *name;			/* Name of a keysym. */
{
#ifdef REDO_KEYSYM_LOOKUP
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&keySymTable, name);
    if (hPtr != NULL) {
	return (KeySym) Tcl_GetHashValue(hPtr);
    }
#endif /* REDO_KEYSYM_LOOKUP */
    return XStringToKeysym(name);
}

/*
 *----------------------------------------------------------------------
 *
 * TkKeysymToString --
 *
 *	This procedure finds the keysym name associated with a given
 *	keysym.
 *
 * Results:
 *	The return value is a pointer to a static string containing
 *	the name of the given keysym, or NULL if there is no known name.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TkKeysymToString(keysym)
    KeySym keysym;
{
#ifdef REDO_KEYSYM_LOOKUP
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&nameTable, (char *)keysym);
    if (hPtr != NULL) {
	return (char *) Tcl_GetHashValue(hPtr);
    }
#endif /* REDO_KEYSYM_LOOKUP */
    return XKeysymToString(keysym);
}
