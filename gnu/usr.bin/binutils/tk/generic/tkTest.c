/* 
 * tkTest.c --
 *
 *	This file contains C command procedures for a bunch of additional
 *	Tcl commands that are used for testing out Tcl's C interfaces.
 *	These commands are not normally included in Tcl applications;
 *	they're only used for testing.
 *
 * Copyright (c) 1993-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkTest.c 1.33 96/03/26 16:46:45
 */

#include "tkInt.h"
#include "tkPort.h"	

/*
 * The table below describes events and is used by the "testevent"
 * command.
 */

typedef struct {
    char *name;			/* Name of event. */
    int type;			/* Event type for X, such as
				 * ButtonPress. */
} EventInfo;

static EventInfo eventArray[] = {
    {"Motion",		MotionNotify},
    {"Button",		ButtonPress},
    {"ButtonPress",	ButtonPress},
    {"ButtonRelease",	ButtonRelease},
    {"Colormap",	ColormapNotify},
    {"Enter",		EnterNotify},
    {"Leave",		LeaveNotify},
    {"Expose",		Expose},
    {"FocusIn",		FocusIn},
    {"FocusOut",	FocusOut},
    {"Keymap",		KeymapNotify},
    {"Key",		KeyPress},
    {"KeyPress",	KeyPress},
    {"KeyRelease",	KeyRelease},
    {"Property",	PropertyNotify},
    {"ResizeRequest",	ResizeRequest},
    {"Circulate",	CirculateNotify},
    {"Configure",	ConfigureNotify},
    {"Destroy",		DestroyNotify},
    {"Gravity",		GravityNotify},
    {"Map",		MapNotify},
    {"Reparent",	ReparentNotify},
    {"Unmap",		UnmapNotify},
    {"Visibility",	VisibilityNotify},
    {"CirculateRequest",CirculateRequest},
    {"ConfigureRequest",ConfigureRequest},
    {"MapRequest",	MapRequest},
    {(char *) NULL,	0}
};

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

static int flagArray[LASTEvent] = {
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
   /* MappingNotify */		MAPPING
};

/*
 * The following data structure represents the master for a test
 * image:
 */

typedef struct TImageMaster {
    Tk_ImageMaster master;	/* Tk's token for image master. */
    Tcl_Interp *interp;		/* Interpreter for application. */
    int width, height;		/* Dimensions of image. */
    char *imageName;		/* Name of image (malloc-ed). */
    char *varName;		/* Name of variable in which to log
				 * events for image (malloc-ed). */
} TImageMaster;

/*
 * The following data structure represents a particular use of a
 * particular test image.
 */

typedef struct TImageInstance {
    TImageMaster *masterPtr;	/* Pointer to master for image. */
    XColor *fg;			/* Foreground color for drawing in image. */
    GC gc;			/* Graphics context for drawing in image. */
} TImageInstance;

/*
 * The type record for test images:
 */

static int		ImageCreate _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, int argc, char **argv,
			    Tk_ImageType *typePtr, Tk_ImageMaster master,
			    ClientData *clientDataPtr));
static ClientData	ImageGet _ANSI_ARGS_((Tk_Window tkwin,
			    ClientData clientData));
static void		ImageDisplay _ANSI_ARGS_((ClientData clientData,
			    Display *display, Drawable drawable, 
			    int imageX, int imageY, int width,
			    int height, int drawableX,
			    int drawableY));
static void		ImageFree _ANSI_ARGS_((ClientData clientData,
			    Display *display));
static void		ImageDelete _ANSI_ARGS_((ClientData clientData));

static Tk_ImageType imageType = {
    "test",			/* name */
    ImageCreate,		/* createProc */
    ImageGet,			/* getProc */
    ImageDisplay,		/* displayProc */
    ImageFree,			/* freeProc */
    ImageDelete,		/* deleteProc */
    (Tk_ImageType *) NULL	/* nextPtr */
};

/*
 * One of the following structures describes each of the interpreters
 * created by the "testnewapp" command.  This information is used by
 * the "testdeleteinterps" command to destroy all of those interpreters.
 */

typedef struct NewApp {
    Tcl_Interp *interp;		/* Token for interpreter. */
    struct NewApp *nextPtr;	/* Next in list of new interpreters. */
} NewApp;

static NewApp *newAppPtr = NULL;
				/* First in list of all new interpreters. */

/*
 * Declaration for the square widget's class command procedure:
 */

extern int SquareCmd _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int argc, char *argv[]));

/*
 * Forward declarations for procedures defined later in this file:
 */

int			Tktest_Init _ANSI_ARGS_((Tcl_Interp *interp));
static int		ImageCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestdeleteappsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TesteventCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestmakeexistCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestsendCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));

/*
 *----------------------------------------------------------------------
 *
 * Tktest_Init --
 *
 *	This procedure performs intialization for the Tk test
 *	suite exensions.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Creates several test commands.
 *
 *----------------------------------------------------------------------
 */

int
Tktest_Init(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    static int initialized = 0;

    /*
     * Create additional commands for testing Tk.
     */

    if (Tcl_PkgProvide(interp, "Tktest", "4.1") == TCL_ERROR) {
        return TCL_ERROR;
    }
    
    Tcl_CreateCommand(interp, "square", SquareCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testdeleteapps", TestdeleteappsCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testevent", TesteventCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testmakeexist", TestmakeexistCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testsend", TestsendCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);

    /*
     * Create test image type.
     */

    if (!initialized) {
	initialized = 1;
	Tk_CreateImageType(&imageType);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdeleteappsCmd --
 *
 *	This procedure implements the "testdeleteapps" command.  It cleans
 *	up all the interpreters left behind by the "testnewapp" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	All the intepreters created by previous calls to "testnewapp"
 *	get deleted.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestdeleteappsCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    NewApp *nextPtr;

    while (newAppPtr != NULL) {
	nextPtr = newAppPtr->nextPtr;
	Tcl_DeleteInterp(newAppPtr->interp);
	ckfree((char *) newAppPtr);
	newAppPtr = nextPtr;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TesteventCmd --
 *
 *	This procedure implements the "testevent" command.  It allows
 *	events to be generated on the fly, for testing event-handling.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and handles events.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TesteventCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    Tk_Window tkwin, tkwin2;
    XEvent event;
    EventInfo *eiPtr;
    char *field, *value;
    int i, number, flags;
    KeySym keysym;

    if ((argc < 3) || !(argc & 1)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" window type ?field value field value ...?\"",
		(char *) NULL);
	return TCL_ERROR;
    }
    tkwin = Tk_NameToWindow(interp, argv[1], main);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }

    /*
     * Get the type of the event.
     */

    memset((VOID *) &event, 0, sizeof(event));
    for (eiPtr = eventArray; ; eiPtr++) {
	if (eiPtr->name == NULL) {
	    Tcl_AppendResult(interp, "bad event type \"", argv[2],
		    "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (strcmp(eiPtr->name, argv[2]) == 0) {
	    event.xany.type = eiPtr->type;
	    break;
	}
    }

    /*
     * Fill in fields that are common to all events.
     */

    event.xany.serial = NextRequest(Tk_Display(tkwin));
    event.xany.send_event = False;
    event.xany.window = Tk_WindowId(tkwin);
    event.xany.display = Tk_Display(tkwin);

    /*
     * Process the remaining arguments to fill in additional fields
     * of the event.
     */

    flags = flagArray[event.xany.type];
    for (i = 3; i < argc; i += 2) {
	field = argv[i];
	value = argv[i+1];
	if (strcmp(field, "-above") == 0) {
	    tkwin2 = Tk_NameToWindow(interp, value, main);
	    if (tkwin2 == NULL) {
		return TCL_ERROR;
	    }
	    event.xconfigure.above = Tk_WindowId(tkwin2);
	} else if (strcmp(field, "-borderwidth") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xcreatewindow.border_width = number;
	} else if (strcmp(field, "-button") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xbutton.button = number;
	} else if (strcmp(field, "-count") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (flags & EXPOSE) {
		event.xexpose.count = number;
	    } else if (flags & MAPPING) {
		event.xmapping.count = number;
	    }
	} else if (strcmp(field, "-detail") == 0) {
	    if (flags & (CROSSING|FOCUS)) {
		if (strcmp(value, "NotifyAncestor") == 0) {
		    number = NotifyAncestor;
		} else if (strcmp(value, "NotifyVirtual") == 0) {
		    number = NotifyVirtual;
		} else if (strcmp(value, "NotifyInferior") == 0) {
		    number = NotifyInferior;
		} else if (strcmp(value, "NotifyNonlinear") == 0) {
		    number = NotifyNonlinear;
		} else if (strcmp(value, "NotifyNonlinearVirtual") == 0) {
		    number = NotifyNonlinearVirtual;
		} else if (strcmp(value, "NotifyPointer") == 0) {
		    number = NotifyPointer;
		} else if (strcmp(value, "NotifyPointerRoot") == 0) {
		    number = NotifyPointerRoot;
		} else if (strcmp(value, "NotifyDetailNone") == 0) {
		    number = NotifyDetailNone;
		} else {
		    Tcl_AppendResult(interp, "bad detail \"", value, "\"",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		if (flags & FOCUS) {
		    event.xfocus.detail = number;
		} else {
		    event.xcrossing.detail = number;
		}
	    } else if (flags & CONFIG_REQ) {
		if (strcmp(value, "Above") == 0) {
		    number = Above;
		} else if (strcmp(value, "Below") == 0) {
		    number = Below;
		} else if (strcmp(value, "TopIf") == 0) {
		    number = TopIf;
		} else if (strcmp(value, "BottomIf") == 0) {
		    number = BottomIf;
		} else if (strcmp(value, "Opposite") == 0) {
		    number = Opposite;
		} else {
		    Tcl_AppendResult(interp, "bad detail \"", value, "\"",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		event.xconfigurerequest.detail = number;
	    }
	} else if (strcmp(field, "-focus") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xcrossing.focus = number;
	} else if (strcmp(field, "-height") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (flags & EXPOSE) {
		 event.xexpose.height = number;
	    } else if (flags & (CONFIG|CONFIG_REQ)) {
		event.xconfigure.height = number;
	    } else if (flags & RESIZE_REQ) {
		event.xresizerequest.height = number;
	    }
	} else if (strcmp(field, "-keycode") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xkey.keycode = number;
	} else if (strcmp(field, "-keysym") == 0) {
	    keysym = TkStringToKeysym(value);
	    if (keysym == NoSymbol) {
		Tcl_AppendResult(interp, "unknown keysym \"", value,
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    number = XKeysymToKeycode(event.xany.display, keysym);
	    if (number == 0) {
		Tcl_AppendResult(interp, "no keycode for keysym \"", value,
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    event.xkey.keycode = number;
	} else if (strcmp(field, "-mode") == 0) {
	    if (strcmp(value, "NotifyNormal") == 0) {
		number = NotifyNormal;
	    } else if (strcmp(value, "NotifyGrab") == 0) {
		number = NotifyGrab;
	    } else if (strcmp(value, "NotifyUngrab") == 0) {
		number = NotifyUngrab;
	    } else if (strcmp(value, "NotifyWhileGrabbed") == 0) {
		number = NotifyWhileGrabbed;
	    } else {
		Tcl_AppendResult(interp, "bad mode \"", value, "\"",
			(char *) NULL);
		return TCL_ERROR;
	    }
	    if (flags & CROSSING) {
		event.xcrossing.mode = number;
	    } else if (flags & FOCUS) {
		event.xfocus.mode = number;
	    }
	} else if (strcmp(field, "-override") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (flags & CREATE) {
		event.xcreatewindow.override_redirect = number;
	    } else if (flags & MAP) {
		event.xmap.override_redirect = number;
	    } else if (flags & REPARENT) {
		event.xreparent.override_redirect = number;
	    } else if (flags & CONFIG) {
		event.xconfigure.override_redirect = number;
	    }
	} else if (strcmp(field, "-place") == 0) {
	    if (strcmp(value, "PlaceOnTop") == 0) {
		event.xcirculate.place = PlaceOnTop;
	    } else if (strcmp(value, "PlaceOnBottom") == 0) {
		event.xcirculate.place = PlaceOnBottom;
	    } else if (strcmp(value, "bogus") == 0) {
		event.xcirculate.place = 147;
	    } else {
		Tcl_AppendResult(interp, "bad place \"", value, "\"",
			(char *) NULL);
		return TCL_ERROR;
	    }
	} else if (strcmp(field, "-root") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xkey.root = number;
	} else if (strcmp(field, "-rootx") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xkey.x_root = number;
	} else if (strcmp(field, "-rooty") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xkey.y_root = number;
	} else if (strcmp(field, "-sendevent") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xany.send_event = number;
	} else if (strcmp(field, "-serial") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xany.serial = number;
	} else if (strcmp(field, "-state") == 0) {
	    if (flags & KEY_BUTTON_MOTION) {
		if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		    return TCL_ERROR;
		}
		event.xkey.state = number;
	    } else if (flags & CROSSING) {
		if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		    return TCL_ERROR;
		}
		event.xcrossing.state = number;
	    } else if (flags & VISIBILITY) {
		if (strcmp(value, "VisibilityUnobscured") == 0) {
		    number = VisibilityUnobscured;
		} else if (strcmp(value, "VisibilityPartiallyObscured") == 0) {
		    number = VisibilityPartiallyObscured;
		} else if (strcmp(value, "VisibilityFullyObscured") == 0) {
		    number = VisibilityFullyObscured;
		} else {
		    Tcl_AppendResult(interp, "bad state \"", value, "\"",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		event.xvisibility.state = number;
	    }
	} else if (strcmp(field, "-subwindow") == 0) {
	    tkwin2 = Tk_NameToWindow(interp, value, main);
	    if (tkwin2 == NULL) {
		return TCL_ERROR;
	    }
	    event.xkey.subwindow = Tk_WindowId(tkwin2);
	} else if (strcmp(field, "-time") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (flags & (KEY_BUTTON_MOTION|PROP|SEL_CLEAR)) {
		event.xkey.time = (Time) number;
	    } else if (flags & SEL_REQ) {
		event.xselectionrequest.time = (Time) number;
	    } else if (flags & SEL_NOTIFY) {
		event.xselection.time = (Time) number;
	    }
	} else if (strcmp(field, "-valueMask") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    event.xconfigurerequest.value_mask = number;
	} else if (strcmp(field, "-width") == 0) {
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (flags & EXPOSE) {
		event.xexpose.width = number;
	    } else if (flags & (CONFIG|CONFIG_REQ)) {
		event.xconfigure.width = number;
	    } else if (flags & RESIZE_REQ) {
		event.xresizerequest.width = number;
	    }
	} else if (strcmp(field, "-window") == 0) {
	    tkwin2 = Tk_NameToWindow(interp, value, main);
	    if (tkwin2 == NULL) {
		return TCL_ERROR;
	    }
	    event.xmap.window = Tk_WindowId(tkwin2);
	} else if (strcmp(field, "-x") == 0) {
	    int rootX, rootY;
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tk_GetRootCoords(tkwin, &rootX, &rootY);
	    rootX += number;
	    if (flags & KEY_BUTTON_MOTION) {
		event.xkey.x = number;
		event.xkey.x_root = rootX;
	    } else if (flags & EXPOSE) {
		event.xexpose.x = number;
	    } else if (flags & (CREATE|CONFIG|GRAVITY|CONFIG_REQ)) {
		event.xcreatewindow.x = number;
	    } else if (flags & REPARENT) {
		event.xreparent.x = number;
	    } else if (flags & CROSSING) {
		event.xcrossing.x = number;
		event.xcrossing.x_root = rootY;
	    }
	} else if (strcmp(field, "-y") == 0) {
	    int rootX, rootY;
	    if (Tcl_GetInt(interp, value, &number) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tk_GetRootCoords(tkwin, &rootX, &rootY);
	    rootY += number;
	    if (flags & KEY_BUTTON_MOTION) {
		event.xkey.y = number;
		event.xkey.y_root = rootY;
	    } else if (flags & EXPOSE) {
		event.xexpose.y = number;
	    } else if (flags & (CREATE|CONFIG|GRAVITY|CONFIG_REQ)) {
		event.xcreatewindow.y = number;
	    } else if (flags & REPARENT) {
		event.xreparent.y = number;
	    } else if (flags & CROSSING) {
		event.xcrossing.y = number;
		event.xcrossing.y_root = rootY;
	    }
	} else {
	    Tcl_AppendResult(interp, "bad option \"", field, "\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    }
    Tk_HandleEvent(&event);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestmakeexistCmd --
 *
 *	This procedure implements the "testmakeexist" command.  It calls
 *	Tk_MakeWindowExist on each of its arguments to force the windows
 *	to be created.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Forces windows to be created.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestmakeexistCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    int i;
    Tk_Window tkwin;

    for (i = 1; i < argc; i++) {
	tkwin = Tk_NameToWindow(interp, argv[i], main);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	Tk_MakeWindowExist(tkwin);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageCreate --
 *
 *	This procedure is called by the Tk image code to create "test"
 *	images.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The data structure for a new image is allocated.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
ImageCreate(interp, name, argc, argv, typePtr, master, clientDataPtr)
    Tcl_Interp *interp;		/* Interpreter for application containing
				 * image. */
    char *name;			/* Name to use for image. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings for options (doesn't
				 * include image name or type). */
    Tk_ImageType *typePtr;	/* Pointer to our type record (not used). */
    Tk_ImageMaster master;	/* Token for image, to be used by us in
				 * later callbacks. */
    ClientData *clientDataPtr;	/* Store manager's token for image here;
				 * it will be returned in later callbacks. */
{
    TImageMaster *timPtr;
    char *varName;
    int i;

    varName = "log";
    for (i = 0; i < argc; i += 2) {
	if (strcmp(argv[i], "-variable") != 0) {
	    Tcl_AppendResult(interp, "bad option name \"", argv[i],
		    "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((i+1) == argc) {
	    Tcl_AppendResult(interp, "no value given for \"", argv[i],
		    "\" option", (char *) NULL);
	    return TCL_ERROR;
	}
	varName = argv[i+1];
    }
    timPtr = (TImageMaster *) ckalloc(sizeof(TImageMaster));
    timPtr->master = master;
    timPtr->interp = interp;
    timPtr->width = 30;
    timPtr->height = 15;
    timPtr->imageName = (char *) ckalloc((unsigned) (strlen(name) + 1));
    strcpy(timPtr->imageName, name);
    timPtr->varName = (char *) ckalloc((unsigned) (strlen(varName) + 1));
    strcpy(timPtr->varName, varName);
    Tcl_CreateCommand(interp, name, ImageCmd, (ClientData) timPtr,
	    (Tcl_CmdDeleteProc *) NULL);
    *clientDataPtr = (ClientData) timPtr;
    Tk_ImageChanged(master, 0, 0, 30, 15, 30, 15);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageCmd --
 *
 *	This procedure implements the commands corresponding to individual
 *	images. 
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Forces windows to be created.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
ImageCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TImageMaster *timPtr = (TImageMaster *) clientData;
    int x, y, width, height;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], "option ?arg arg ...?", (char *) NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "changed") == 0) {
	if (argc != 8) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " changed x y width height imageWidth imageHeight",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[5], &height) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[6], &timPtr->width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[7], &timPtr->height) != TCL_OK)) {
	    return TCL_ERROR;
	}
	Tk_ImageChanged(timPtr->master, x, y, width, height, timPtr->width,
		timPtr->height);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be changed", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageGet --
 *
 *	This procedure is called by Tk to set things up for using a
 *	test image in a particular widget.
 *
 * Results:
 *	The return value is a token for the image instance, which is
 *	used in future callbacks to ImageDisplay and ImageFree.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ClientData
ImageGet(tkwin, clientData)
    Tk_Window tkwin;		/* Token for window in which image will
				 * be used. */
    ClientData clientData;	/* Pointer to TImageMaster for image. */
{
    TImageMaster *timPtr = (TImageMaster *) clientData;
    TImageInstance *instPtr;
    char buffer[100];
    XGCValues gcValues;

    sprintf(buffer, "%s get", timPtr->imageName);
    Tcl_SetVar(timPtr->interp, timPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);

    instPtr = (TImageInstance *) ckalloc(sizeof(TImageInstance));
    instPtr->masterPtr = timPtr;
    instPtr->fg = Tk_GetColor(timPtr->interp, tkwin, "#ff0000");
    gcValues.foreground = instPtr->fg->pixel;
    instPtr->gc = Tk_GetGC(tkwin, GCForeground, &gcValues);
    return (ClientData) instPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageDisplay --
 *
 *	This procedure is invoked to redisplay part or all of an
 *	image in a given drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image gets partially redrawn, as an "X" that shows the
 *	exact redraw area.
 *
 *----------------------------------------------------------------------
 */

static void
ImageDisplay(clientData, display, drawable, imageX, imageY, width, height,
	drawableX, drawableY)
    ClientData clientData;	/* Pointer to TImageInstance for image. */
    Display *display;		/* Display to use for drawing. */
    Drawable drawable;		/* Where to redraw image. */
    int imageX, imageY;		/* Origin of area to redraw, relative to
				 * origin of image. */
    int width, height;		/* Dimensions of area to redraw. */
    int drawableX, drawableY;	/* Coordinates in drawable corresponding to
				 * imageX and imageY. */
{
    TImageInstance *instPtr = (TImageInstance *) clientData;
    char buffer[200];

    sprintf(buffer, "%s display %d %d %d %d %d %d",
	    instPtr->masterPtr->imageName, imageX, imageY, width, height,
	    drawableX, drawableY);
    Tcl_SetVar(instPtr->masterPtr->interp, instPtr->masterPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);
    if (width > (instPtr->masterPtr->width - imageX)) {
	width = instPtr->masterPtr->width - imageX;
    }
    if (height > (instPtr->masterPtr->height - imageY)) {
	height = instPtr->masterPtr->height - imageY;
    }
    XDrawRectangle(display, drawable, instPtr->gc, drawableX, drawableY,
	    (unsigned) (width-1), (unsigned) (height-1));
    XDrawLine(display, drawable, instPtr->gc, drawableX, drawableY,
	    (int) (drawableX + width - 1), (int) (drawableY + height - 1));
    XDrawLine(display, drawable, instPtr->gc, drawableX,
	    (int) (drawableY + height - 1),
	    (int) (drawableX + width - 1), drawableY);
}

/*
 *----------------------------------------------------------------------
 *
 * ImageFree --
 *
 *	This procedure is called when an instance of an image is
 * 	no longer used.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information related to the instance is freed.
 *
 *----------------------------------------------------------------------
 */

static void
ImageFree(clientData, display)
    ClientData clientData;	/* Pointer to TImageInstance for instance. */
    Display *display;		/* Display where image was to be drawn. */
{
    TImageInstance *instPtr = (TImageInstance *) clientData;
    char buffer[200];

    sprintf(buffer, "%s free", instPtr->masterPtr->imageName);
    Tcl_SetVar(instPtr->masterPtr->interp, instPtr->masterPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);
    Tk_FreeColor(instPtr->fg);
    Tk_FreeGC(display, instPtr->gc);
    ckfree((char *) instPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ImageDelete --
 *
 *	This procedure is called to clean up a test image when
 *	an application goes away.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information about the image is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
ImageDelete(clientData)
    ClientData clientData;	/* Pointer to TImageMaster for image.  When
				 * this procedure is called, no more
				 * instances exist. */
{
    TImageMaster *timPtr = (TImageMaster *) clientData;
    char buffer[100];

    sprintf(buffer, "%s delete", timPtr->imageName);
    Tcl_SetVar(timPtr->interp, timPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);

    Tcl_DeleteCommand(timPtr->interp, timPtr->imageName);
    ckfree(timPtr->imageName);
    ckfree(timPtr->varName);
    ckfree((char *) timPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TestsendCmd --
 *
 *	This procedure implements the "testsend" command.  It provides
 *	a set of functions for testing the "send" command and support
 *	procedure in tkSend.c.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depends on option;  see below.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestsendCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TkWindow *winPtr = (TkWindow *) clientData;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

#ifndef __WIN32__
    if (strcmp(argv[1], "bogus") == 0) {
	XChangeProperty(winPtr->dispPtr->display,
		RootWindow(winPtr->dispPtr->display, 0),
		winPtr->dispPtr->registryProperty, XA_INTEGER, 32,
		PropModeReplace,
		(unsigned char *) "This is bogus information", 6);
    } else if (strcmp(argv[1], "prop") == 0) {
	int result, actualFormat, length;
	unsigned long bytesAfter;
	Atom actualType, propName;
	char *property, *p, *end;
	Window w;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		    " prop window name ?value ?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (strcmp(argv[2], "root") == 0) {
	    w = RootWindow(winPtr->dispPtr->display, 0);
	} else if (strcmp(argv[2], "comm") == 0) {
	    w = Tk_WindowId(winPtr->dispPtr->commTkwin);
	} else {
	    w = strtoul(argv[2], &end, 0);
	}
	propName = Tk_InternAtom((Tk_Window) winPtr, argv[3]);
	if (argc == 4) {
	    property = NULL;
	    result = XGetWindowProperty(winPtr->dispPtr->display,
		    w, propName, 0, 100000, False, XA_STRING,
		    &actualType, &actualFormat, (unsigned long *) &length,
		    &bytesAfter, (unsigned char **) &property);
	    if ((result == Success) && (actualType != None)
		    && (actualFormat == 8) && (actualType == XA_STRING)) {
		for (p = property; (p-property) < length; p++) {
		    if (*p == 0) {
			*p = '\n';
		    }
		}
		Tcl_SetResult(interp, property, TCL_VOLATILE);
	    }
	    if (property != NULL) {
		XFree(property);
	    }
	} else {
	    if (argv[4][0] == 0) {
		XDeleteProperty(winPtr->dispPtr->display, w, propName);
	    } else {
		for (p = argv[4]; *p != 0; p++) {
		    if (*p == '\n') {
			*p = 0;
		    }
		}
		XChangeProperty(winPtr->dispPtr->display,
			w, propName, XA_STRING, 8, PropModeReplace,
			(unsigned char *) argv[4], p-argv[4]);
	    }
	}
    } else if (strcmp(argv[1], "serial") == 0) {
	sprintf(interp->result, "%d", tkSendSerial+1);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be bogus, prop, or serial", (char *) NULL);
	return TCL_ERROR;
    }
#endif
    return TCL_OK;
}
