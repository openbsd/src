/* 
 * tkGrid.c --
 *
 *	Grid based geometry manager.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *
 * SCCS: @(#) tkGrid.c 1.21 96/02/21 10:50:58
 */

#include "tkInt.h"

/*
 * LayoutInfo structure.  We shouldn't be using hard-wired limits!
 */

#define MAXGRIDSIZE 128
#ifndef MAXINT
#  define MAXINT 0x7fff
#endif
#define MINWEIGHT	0.0001		/* weight totals < this are considered to be zero */

/*
 * Special characters to support relative layouts
 */

#define REL_SKIP	'x'	/* skip this column */
#define REL_HORIZ	'-'	/* extend previous widget horizontally */
#define REL_VERT	'^'	/* extend previous widget verticallly */

/*
 *  structure to hold collected constraints temporarily:
 *  needs to use a "Constrain" thingy
 */

typedef struct {
    int width, height;		/* number of cells horizontally, vertically */
    int lastRow;			/* last cell with a window in it */
    int minWidth[MAXGRIDSIZE];	/* largest minWidth in each column */
    int minHeight[MAXGRIDSIZE];	/* largest minHeight in each row */
    double weightX[MAXGRIDSIZE];	/* largest weight in each column */
    double weightY[MAXGRIDSIZE];	/* largest weight in each row */
} LayoutInfo;

/* structure for holding row and column constraints */

typedef struct {
    int used;		/* maximum element used */
    int max;		/* maximum element allocated */
    int *minsize;		/* array of minimum column/row sizes */
    double *weight;	/* array of column/row weights */
} Constrain;

/* For each window that the gridbag cares about (either because
 * the window is managed by the gridbag or because the window
 * has slaves that are managed by the gridbag), there is a
 * structure of the following type:
 */

typedef struct GridBag {
    Tk_Window tkwin;		/* Tk token for window.  NULL means that
				 * the window has been deleted, but the
				 * packet hasn't had a chance to clean up
				 * yet because the structure is still in
				 * use. */
    struct GridBag *masterPtr;	/* Master window within which this window
				 * is managed (NULL means this window
				 * isn't managed by the gridbag). */
    struct GridBag *nextPtr;	/* Next window managed within same
				 * parent.  List is priority-ordered:
				 * first on list gets layed out first. */
    struct GridBag *slavePtr;	/* First in list of slaves managed
				 * inside this window (NULL means
				 * no gridbag slaves). */

    int gridColumn, gridRow;
    int gridWidth, gridHeight;

    int tempX, tempY;
    int tempWidth, tempHeight;

    double weightX, weightY;
    int minWidth, minHeight;

    int padX, padY;		/* Total additional pixels to leave around the
				 * window (half of this space is left on each
				 * side).  This is space *outside* the window:
				 * we'll allocate extra space in frame but
				 * won't enlarge window). */
    int iPadX, iPadY;		/* Total extra pixels to allocate inside the
				 * window (half this amount will appear on
				 * each side). */
    int startx, starty;		/* starting location of layout */

    int doubleBw;		/* Twice the window's last known border
				 * width.  If this changes, the window
				 * must be re-arranged within its parent. */
    int *abortPtr;		/* If non-NULL, it means that there is a nested
				 * call to ArrangeGrid already working on
				 * this window.  *abortPtr may be set to 1 to
				 * abort that nested call.  This happens, for
				 * example, if tkwin or any of its slaves
				 * is deleted. */
    int flags;			/* Miscellaneous flags;  see below
				 * for definitions. */

    Constrain row, column;		/* column and row constraints */

    int valid;
    LayoutInfo *layoutCache;
} GridBag;

/*
 * Flag values for GridBag structures:
 *
 * REQUESTED_RELAYOUT:		1 means a Tk_DoWhenIdle request
 *				has already been made to re-arrange
 *				all the slaves of this window.
 * STICK_NORTH  		1 means this window sticks to the edgth of its
 * STICK_EAST			cavity
 * STICK_SOUTH
 * STICK_WEST
 *
 * DONT_PROPAGATE:		1 means don't set this window's requested
 *				size.  0 means if this window is a master
 *				then Tk will set its requested size to fit
 *				the needs of its slaves.
 */

#define STICK_NORTH		1
#define STICK_EAST		2
#define STICK_SOUTH		4
#define STICK_WEST		8
#define STICK_ALL		(STICK_NORTH|STICK_EAST|STICK_SOUTH|STICK_WEST)

#define REQUESTED_RELAYOUT	16
#define DONT_PROPAGATE		32

/*
 * Hash table used to map from Tk_Window tokens to corresponding
 * GridBag structures:
 */

static Tcl_HashTable gridBagHashTable;

/*
 * Have statics in this module been initialized?
 */

static initialized = 0;

/*
 * Prototypes for procedures used only in this file:
 */

static void		ArrangeGrid _ANSI_ARGS_((ClientData clientData));
static int		ConfigureSlaves _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, int argc, char *argv[]));
static void		DestroyGridBag _ANSI_ARGS_((char *memPtr));
static void		GetCachedLayoutInfo _ANSI_ARGS_((GridBag *masterPtr));
static GridBag *	GetGridBag _ANSI_ARGS_((Tk_Window tkwin));
static void		GetLayoutInfo _ANSI_ARGS_((GridBag *masterPtr,
			    LayoutInfo *r));
static void		GetMinSize _ANSI_ARGS_((GridBag *masterPtr,
			    LayoutInfo *info, int *minw, int *minh));
static void		GridBagStructureProc _ANSI_ARGS_((
			    ClientData clientData, XEvent *eventPtr));
static void		GridLostSlaveProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));
static void		GridReqProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));
static void		GridBagStructureProc _ANSI_ARGS_((
			    ClientData clientData, XEvent *eventPtr));
static void		StickyToString _ANSI_ARGS_((int flags, char *result));
static int		StringToSticky _ANSI_ARGS_((char *string));
static void		Unlink _ANSI_ARGS_((GridBag *gridPtr));

static Tk_GeomMgr gridMgrType = {
    "grid",			/* name */
    GridReqProc,		/* requestProc */
    GridLostSlaveProc,		/* lostSlaveProc */
};

/*
 *--------------------------------------------------------------
 *
 * Tk_GridCmd --
 *
 *	This procedure is invoked to process the "grid" Tcl command.
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
Tk_GridCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    size_t length;
    char c;
  
    if ((argc >= 2) && (argv[1][0] == '.')) {
	return ConfigureSlaves(interp, tkwin, argc-1, argv+1);
    }
    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
  
    if ((c == 'b') && (strncmp(argv[1], "bbox", length) == 0)) {
	Tk_Window master;
	GridBag *masterPtr;
	int row, column;
	int i, x, y;
	int prevX, prevY;
	int width, height;
	double weight;
	int diff;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "Wrong number of arguments: ",
		    "must be \"",argv[0],
		    " bbox <master> <column> <row>\"", (char *) NULL);
	    return TCL_ERROR;
	}
        
	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[3], &column) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[4], &row) != TCL_OK) {
	    return TCL_ERROR;
	}
	masterPtr = GetGridBag(master);

	/* make sure the grid is up to snuff */

	while ((masterPtr->flags & REQUESTED_RELAYOUT)) {
	    Tk_CancelIdleCall(ArrangeGrid, (ClientData) masterPtr);
	    ArrangeGrid((ClientData) masterPtr);
	}
	GetCachedLayoutInfo(masterPtr);

	if (row < 0 || column < 0) {
	    *interp->result = '\0';
	    return TCL_OK;
	}
	if (column >= masterPtr->layoutCache->width ||
		row >= masterPtr->layoutCache->height) {
	    *interp->result = '\0';
	    return TCL_OK;
	}
	x = masterPtr->startx;
	y = masterPtr->starty;
	GetMinSize(masterPtr, masterPtr->layoutCache, &width, &height);

	diff = Tk_Width(masterPtr->tkwin) - (width + masterPtr->iPadX);
	for (weight=0.0, i=0; i<masterPtr->layoutCache->width; i++)
	    weight += masterPtr->layoutCache->weightX[i];

	prevX = 0;			/* Needed to prevent gcc warning. */
	for (i=0; i<=column; i++) {
	    int dx = 0;
	    if (weight > MINWEIGHT) {
		dx = (int)((((double)diff) * masterPtr->layoutCache->weightX[i])
			/ weight);
	    }
	    prevX = x;
	    x += masterPtr->layoutCache->minWidth[i] + dx;
	}
	diff = Tk_Height(masterPtr->tkwin) - (height + masterPtr->iPadY);
	for (weight=0.0, i=0; i<masterPtr->layoutCache->width; i++) {
	    weight += masterPtr->layoutCache->weightY[i];
	}
	prevY = 0;			/* Needed to prevent gcc warning. */
	for (i=0; i<=row; i++) {
	    int dy = 0;
	    if (weight > MINWEIGHT) {
		dy = (int)((((double)diff) * masterPtr->layoutCache->weightY[i])
			/ weight);
	    }
	    prevY = y;
	    y += masterPtr->layoutCache->minHeight[i] + dy;
	}
	sprintf(interp->result,"%d %d %d %d",prevX,prevY,x - prevX,y - prevY);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argv[2][0] != '.') {
	    Tcl_AppendResult(interp, "bad argument \"", argv[2],
		    "\": must be name of window", (char *) NULL);
	    return TCL_ERROR;
	}
	return ConfigureSlaves(interp, tkwin, argc-2, argv+2);
    } else if ((c == 'f') && (strncmp(argv[1], "forget", length) == 0)) {
	Tk_Window slave;
	GridBag *slavePtr;
	int i;
    
	for (i = 2; i < argc; i++) {
	    slave = Tk_NameToWindow(interp, argv[i], tkwin);
	    if (slave == NULL) {
		return TCL_ERROR;
	    }
	    slavePtr = GetGridBag(slave);
	    if (slavePtr->masterPtr != NULL) {
		Tk_ManageGeometry(slave, (Tk_GeomMgr *) NULL,
			(ClientData) NULL);
		Unlink(slavePtr);
		Tk_UnmapWindow(slavePtr->tkwin);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "info", length) == 0)) {
	register GridBag *slavePtr;
	Tk_Window slave;
	char buffer[64];
    
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " info window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	slave = Tk_NameToWindow(interp, argv[2], tkwin);
	if (slave == NULL) {
	    return TCL_ERROR;
	}
	slavePtr = GetGridBag(slave);
	if (slavePtr->masterPtr == NULL) {
	    interp->result[0] = '\0';
	    return TCL_OK;
	}
    
	Tcl_AppendElement(interp, "-in");
	Tcl_AppendElement(interp, Tk_PathName(slavePtr->masterPtr->tkwin));
	sprintf(buffer, " -column %d -row %d -columnspan %d -rowspan %d",
		slavePtr->gridColumn, slavePtr->gridRow,
		slavePtr->gridWidth, slavePtr->gridHeight);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	sprintf(buffer, " -ipadx %d -ipady %d -padx %d -pady %d",
		slavePtr->iPadX/2, slavePtr->iPadY/2, slavePtr->padX/2,
		slavePtr->padY/2);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	StickyToString(slavePtr->flags,buffer);
	Tcl_AppendResult(interp, " -sticky ", buffer, (char *) NULL);
/*
	sprintf(buffer, " -weightx %.2f -weighty %.2f",
		slavePtr->weightX, slavePtr->weightY);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
*/
    } else if ((c == 'p') && (strncmp(argv[1], "propagate", length) == 0)) {
	Tk_Window master;
	GridBag *masterPtr;
	int propagate;
    
	if (argc > 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " propagate window ?boolean?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetGridBag(master);
	if (argc == 3) {
	    interp->result = (masterPtr->flags & DONT_PROPAGATE) ? "0" : "1";
	    return TCL_OK;
	}
	if (Tcl_GetBoolean(interp, argv[3], &propagate) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (propagate) {
	    masterPtr->flags &= ~DONT_PROPAGATE;
      
	    /*
	     * Re-arrange the master to allow new geometry information to
	     * propagate upwards to the master\'s master.
	     */
      
	    if (masterPtr->abortPtr != NULL) {
		*masterPtr->abortPtr = 1;
	    }
	    masterPtr->valid = 0;
	    if (!(masterPtr->flags & REQUESTED_RELAYOUT)) {
		masterPtr->flags |= REQUESTED_RELAYOUT;
		Tk_DoWhenIdle(ArrangeGrid, (ClientData) masterPtr);
	    }
	} else {
	    masterPtr->flags |= DONT_PROPAGATE;
	}
    } else if ((c == 's') && (strncmp(argv[1], "size", length) == 0)) {
	Tk_Window master;
	GridBag *masterPtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " size window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL)
	    return TCL_ERROR;
	masterPtr = GetGridBag(master);
	GetCachedLayoutInfo(masterPtr);

	sprintf(interp->result, "%d %d", masterPtr->layoutCache->width,
		masterPtr->layoutCache->height);
    } else if ((c == 's') && (strncmp(argv[1], "slaves", length) == 0)) {
	Tk_Window master;
	GridBag *masterPtr, *slavePtr;
	int i, value;
	int row = -1, column = -1;
 
	if (argc < 3 || argc%2 ==0) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " slaves window ?-option value...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}

	for (i=3; i<argc; i+=2) {
	    if (*argv[i] != '-' || (length = strlen(argv[i])) < 2) {
		Tcl_AppendResult(interp, "Invalid args: should be \"",
			argv[0], " slaves window ?-option value...?\"",
			(char *) NULL);
		return TCL_ERROR;
	    }
	    if (Tcl_GetInt(interp, argv[i+1], &value) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (value < 0) {
		Tcl_AppendResult(interp, argv[i],
			" is an invalid value: should NOT be < 0",
			(char *) NULL);
		return TCL_ERROR;
	    }
	    if (strncmp(argv[i], "-column", length) == 0) {
		column = value;
	    } else if (strncmp(argv[i], "-row", length) == 0) {
		row = value;
	    } else {
		Tcl_AppendResult(interp, argv[i],
			" is an invalid option: should be \"",
			"-row, -column\"",
			(char *) NULL);
		return TCL_ERROR;
	    }
	}
	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetGridBag(master);

	for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
					     slavePtr = slavePtr->nextPtr) {
	    if (column>=0 && (slavePtr->gridColumn > column
		    || slavePtr->gridColumn+slavePtr->gridWidth-1 < column)) {
		continue;
	    }
	    if (row>=0 && (slavePtr->gridRow > row ||
		    slavePtr->gridRow+slavePtr->gridHeight-1 < row)) {
		continue;
	    }
	    Tcl_AppendElement(interp, Tk_PathName(slavePtr->tkwin));
	}

    /*
     * grid columnconfigure <master> <index> -option
     * grid columnconfigure <master> <index> -option value -option value
     * grid rowconfigure <master> <index> -option
     * grid rowconfigure <master> <index> -option value -option value
     */
   
    } else if(((c=='c') && (strncmp(argv[1], "columnconfigure", length) == 0)) ||
            ((c=='r') && (strncmp(argv[1], "rowconfigure", length) == 0))) {
	Tk_Window master;
	GridBag *masterPtr;
	Constrain *con;
	int index, i, size;
	double weight;

	if (argc != 5 && (argc < 5 || argc%2 == 1)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " ", argv[1], " master index ?-option value...?\"",
		    (char *)NULL);
	    return TCL_ERROR;
	}

	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetGridBag(master);
	con = (c=='c') ? &(masterPtr->column) : &(masterPtr->row);

	if (Tcl_GetInt(interp, argv[3], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (index < 0 || index >= MAXGRIDSIZE) {
	    Tcl_AppendResult(interp, argv[3], " is out of range",
		    (char *)NULL);
	    return TCL_ERROR;
	}

	/*
	 *  make sure the row/column constraint array is allocated.  This
	 *  Should be changed to avoid hard-wired limits.  We'll wimp out
	 *  for now.
	 */

	if (con->max == 0) {
	    unsigned int size;
	    con->max = MAXGRIDSIZE;
	    con->used = 0;

	    size = MAXGRIDSIZE * sizeof(con->minsize[0]);
	    con->minsize = (int *) ckalloc(size);
	    memset(con->minsize, 0, size);

	    size = MAXGRIDSIZE * sizeof(con->weight[0]);
	    con->weight = (double *) ckalloc(size);
	    memset(con->weight, 0, size);
	}

	for (i=4; i<argc; i+=2) {
	    if (*argv[i] != '-' || (length = strlen(argv[i])) < 2) {
		Tcl_AppendResult(interp, "Invalid arg: \"",
			argv[0], "\" expecting -minsize or -weight",
			(char *) NULL);
		return TCL_ERROR;
	    }
	    if (strncmp(argv[i], "-minsize", length) == 0) {
		if (argc == 5) {
		    size = con->used <= index ?  0 : con->minsize[index];
		    sprintf(interp->result, "%d", size);
		} else if (Tk_GetPixels(interp, master, argv[i+1], &size)
			!= TCL_OK) {
		    return TCL_ERROR;
		} else {
		    con->minsize[index] = size;
		    if (size > 0 && index >= con->used) 
			con->used = index+1;
		    else if (size == 0 && index+1 == con->used) {
			while (index >= 0  && (con->minsize[index]==0) &&
				(con->weight[index] == 0.0)) {
			    index--;
			}
			con->used = index + 1;
		    }
		}
	    } else if (strncmp(argv[i], "-weight", length) == 0) {
		if (argc == 5) {
		    weight = con->used <= index ?  0 : con->weight[index];
		    sprintf(interp->result, "%.2f", weight);
		} else if (Tcl_GetDouble(interp, argv[i+1], &weight) != TCL_OK) {
		    return TCL_ERROR;
		} else {
		    con->weight[index] = weight;
		    if (weight > MINWEIGHT && index >= con->used) 
			con->used = index+1;
		    else if (weight == 0.0 && index+1 == con->used) {
			while (index >= 0 && (con->minsize[index]==0) &&
				(con->weight[index] == 0.0)) {
			    index--;
			}
			con->used = index + 1;
		    }
		}
	    } else {
		Tcl_AppendResult(interp, argv[i],
			" is an invalid option: should be \"",
			"-minsize, -weight\"",
			(char *) NULL);
		return TCL_ERROR;
	    }
	}

	/* if we changed a property, re-arrange the table */

	if (argc != 5) {
	    if (masterPtr->abortPtr != NULL) {
		*masterPtr->abortPtr = 1;
	    }
	    masterPtr->valid = 0;
	    if (!(masterPtr->flags & REQUESTED_RELAYOUT)) {
		masterPtr->flags |= REQUESTED_RELAYOUT;
		Tk_DoWhenIdle(ArrangeGrid, (ClientData) masterPtr);
	    }
	}
    } else if((c == 'l') && (strncmp(argv[1], "location", length) == 0)) {
	Tk_Window master;
	GridBag *masterPtr;
	int x, y, i, j, w, h;
	int width, height;
	double weight;
	int diff;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " location master x y\"", (char *)NULL);
	    return TCL_ERROR;
	}

	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetGridBag(master);

	if (Tk_GetPixels(interp, master, argv[3], &x) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tk_GetPixels(interp, master, argv[4], &y) != TCL_OK) {
	    return TCL_ERROR;
	}

	/* make sure the grid is up to snuff */

	while ((masterPtr->flags & REQUESTED_RELAYOUT)) {
	    Tk_CancelIdleCall(ArrangeGrid, (ClientData) masterPtr);
	    ArrangeGrid((ClientData) masterPtr);
	}
	GetCachedLayoutInfo(masterPtr);
	GetMinSize(masterPtr, masterPtr->layoutCache, &width, &height);

	diff = Tk_Width(masterPtr->tkwin) - (width + masterPtr->iPadX);
	for (weight=0.0, i=0; i<masterPtr->layoutCache->width; i++) {
	    weight += masterPtr->layoutCache->weightX[i];
	}
	w = masterPtr->startx;
	if (w > x) {
	    i = -1;
	} else {
	    for (i=0; i<masterPtr->layoutCache->width; i++) {
		int dx = 0;
		if (weight > MINWEIGHT) {
		    dx = (int)((((double)diff) * masterPtr->layoutCache->weightX[i])
			    / weight);
		    }
		w += masterPtr->layoutCache->minWidth[i] + dx;
		if (w > x) {
		    break;
		}
	    }
	}

	diff = Tk_Height(masterPtr->tkwin) - (height + masterPtr->iPadY);
	for (weight=0.0, j = 0; j < masterPtr->layoutCache->height; j++)
	    weight += masterPtr->layoutCache->weightY[j];
	h = masterPtr->starty;
	if (h > y) {
	    j = -1;
	} else {
	    for (j=0; j<masterPtr->layoutCache->height; j++) {
		int dy = 0;
		if (weight > MINWEIGHT) {
		    dy = (int)((((double)diff) * masterPtr->layoutCache->weightY[j])
			    / weight);
		}
		h += masterPtr->layoutCache->minHeight[j] + dy;
		if (h > y) {
		    break;
		}
	    }
	}
	sprintf(interp->result, "%d %d", i, j);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be bbox, columnconfigure, configure, forget, info, ",
		"location, propagate, rowconfigure, size, or slaves",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * GridReqProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for
 *	windows managed by the gridbag.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for tkwin, and all its managed siblings, to
 *	be re-arranged at the next idle point.
 *
 *--------------------------------------------------------------
 */

/* ARGSUSED */
static void
GridReqProc(clientData, tkwin)
    ClientData clientData;	/* GridBag's information about
				 * window that got new preferred
				 * geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information
				 * about the window. */
{
    register GridBag *gridPtr = (GridBag *) clientData;

    gridPtr = gridPtr->masterPtr;
    gridPtr->valid = 0;
    if (!(gridPtr->flags & REQUESTED_RELAYOUT)) {
	gridPtr->flags |= REQUESTED_RELAYOUT;
	Tk_DoWhenIdle(ArrangeGrid, (ClientData) gridPtr);
    }
}


/*
 *--------------------------------------------------------------
 *
 * GridLostSlaveProc --
 *
 *	This procedure is invoked by Tk whenever some other geometry
 *	claims control over a slave that used to be managed by us.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Forgets all grid-related information about the slave.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
GridLostSlaveProc(clientData, tkwin)
    ClientData clientData;	/* GridBag structure for slave window that
				 * was stolen away. */
    Tk_Window tkwin;		/* Tk's handle for the slave window. */
{
    register GridBag *slavePtr = (GridBag *) clientData;

    if (slavePtr->masterPtr->tkwin != Tk_Parent(slavePtr->tkwin)) {
	Tk_UnmaintainGeometry(slavePtr->tkwin, slavePtr->masterPtr->tkwin);
    }
    Unlink(slavePtr);
    Tk_UnmapWindow(slavePtr->tkwin);
}

/*
 * Fill in an instance of the above structure for the current set
 * of managed children.  This requires two passes through the
 * set of children, first to figure out what cells they occupy
 * and how many rows and columns there are, and then to distribute
 * the weights and min sizes amoung the rows/columns.
 *
 * This also caches the minsizes for all the children when they are
 * first encountered.
 */

static void
GetLayoutInfo(masterPtr, r)
    GridBag *masterPtr;
    LayoutInfo *r;
{
    register GridBag *slavePtr;
    int i, k, px, py, pixels_diff, nextSize;
    double weight_diff, weight;
    register int curX, curY, curWidth, curHeight, curRow, curCol;
    int xMax[MAXGRIDSIZE];
    int yMax[MAXGRIDSIZE];

    /*
     * Pass #1
     *
     * Figure out the dimensions of the layout grid.
     */

    r->width = r->height = 0;
    curRow = curCol = -1;
    memset(xMax, 0, sizeof(int) * MAXGRIDSIZE);
    memset(yMax, 0, sizeof(int) * MAXGRIDSIZE);

    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
					 slavePtr = slavePtr->nextPtr) {

	curX = slavePtr->gridColumn;
	curY = slavePtr->gridRow;
	curWidth = slavePtr->gridWidth;
	curHeight = slavePtr->gridHeight;

	/* Adjust the grid width and height */
	for (px = curX + curWidth; r->width < px; r->width++) {
	    /* Null body. */
	}
	for (py = curY + curHeight; r->height < py; r->height++) {
	    /* Null body. */
	}

	/* Adjust the xMax and yMax arrays */
	for (i = curX; i < (curX + curWidth); i++) {
	    yMax[i] = py;
	}
	for (i = curY; i < (curY + curHeight); i++) {
	    xMax[i] = px;
	}

	/* Cache the current slave's size. */
	slavePtr->minWidth = Tk_ReqWidth(slavePtr->tkwin) + slavePtr->doubleBw;
	slavePtr->minHeight = Tk_ReqHeight(slavePtr->tkwin) + slavePtr->doubleBw;
    }

    /*
     * Apply minimum row/column dimensions
     */ 
    if (r->width < masterPtr->column.used) {
	r->width = masterPtr->column.used;
    }
    r->lastRow = r->height;
    if (r->height < masterPtr->row.used) {
	r->height = masterPtr->row.used;
    }

    /*
     * Pass #2
     */

    curRow = curCol = -1;
    memset(xMax, 0, sizeof(int) * MAXGRIDSIZE);
    memset(yMax, 0, sizeof(int) * MAXGRIDSIZE);

    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
					 slavePtr = slavePtr->nextPtr) {
	curX = slavePtr->gridColumn;
	curY = slavePtr->gridRow;
	curWidth = slavePtr->gridWidth;
	curHeight = slavePtr->gridHeight;

	px = curX + curWidth;
	py = curY + curHeight;

	for (i = curX; i < (curX + curWidth); i++) {
	    yMax[i] = py;
	}
	for (i = curY; i < (curY + curHeight); i++) {
	    xMax[i] = px;
	}

	/* Assign the new values to the gridbag slave */
	slavePtr->tempX = curX;
	slavePtr->tempY = curY;
	slavePtr->tempWidth = curWidth;
	slavePtr->tempHeight = curHeight;
    }

    /*
     * Pass #3
     *
     * Distribute the minimun widths and weights:
     */

    /* Initialize arrays to zero */
    memset(r->minWidth, 0, r->width * sizeof(int));
    memset(r->minHeight, 0, r->height * sizeof(int));
    memset(r->weightX, 0, r->width * sizeof(double));
    memset(r->weightY, 0, r->height * sizeof(double));
    nextSize = MAXINT;

    for (i = 1; i != MAXINT; i = nextSize, nextSize = MAXINT) {
	for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
					     slavePtr = slavePtr->nextPtr) {

	    if (slavePtr->tempWidth == i) {
		px = slavePtr->tempX + slavePtr->tempWidth; /* right column */

		/* 
		 * Figure out if we should use this slave\'s weight.  If the weight
		 * is less than the total weight spanned by the width of the cell,
		 * then discard the weight.  Otherwise split it the difference
		 * according to the existing weights.
		 */

		weight_diff = slavePtr->weightX;
		for (k = slavePtr->tempX; k < px; k++)
		    weight_diff -= r->weightX[k];
		if (weight_diff > 0.0) {
		    weight = 0.0;
		    for (k = slavePtr->tempX; k < px; k++)
			weight += r->weightX[k];
		    for (k = slavePtr->tempX; weight > MINWEIGHT; k++) {
			double wt = r->weightX[k];
			double dx = (wt * weight_diff) / weight;
			r->weightX[k] += dx;
			weight_diff -= dx;
			weight -= wt;
		    }
		    /* Assign the remainder to the rightmost cell */
		    r->weightX[px-1] += weight_diff;
		}

		/*
		 * Calculate the minWidth array values.
		 * First, figure out how wide the current slave needs to be.
		 * Then, see if it will fit within the current minWidth values.
		 * If it won\'t fit, add the difference according to the weightX array.
		 */

		pixels_diff = slavePtr->minWidth + slavePtr->padX + slavePtr->iPadX;
		for (k = slavePtr->tempX; k < px; k++)
		    pixels_diff -= r->minWidth[k];
		if (pixels_diff > 0) {
		    weight = 0.0;
		    for (k = slavePtr->tempX; k < px; k++)
			weight += r->weightX[k];
		    for (k = slavePtr->tempX; weight > MINWEIGHT; k++) {
			double wt = r->weightX[k];
			int dx = (int)((wt * ((double)pixels_diff)) / weight);
			r->minWidth[k] += dx;
			pixels_diff -= dx;
			weight -= wt;
		    }
		    /* Any leftovers go into the rightmost cell */
		    r->minWidth[px-1] += pixels_diff;
		}
	    }
	    else if (slavePtr->tempWidth > i && slavePtr->tempWidth < nextSize)
		nextSize = slavePtr->tempWidth;


	    if (slavePtr->tempHeight == i) {
		py = slavePtr->tempY + slavePtr->tempHeight; /* bottom row */

		/* 
		 * Figure out if we should use this slave\'s weight.  If the weight
		 * is less than the total weight spanned by the height of the cell,
		 * then discard the weight.  Otherwise split it the difference
		 * according to the existing weights.
		 */

		weight_diff = slavePtr->weightY;
		for (k = slavePtr->tempY; k < py; k++)
		    weight_diff -= r->weightY[k];
		if (weight_diff > 0.0) {
		    weight = 0.0;
		    for (k = slavePtr->tempY; k < py; k++)
			weight += r->weightY[k];
		    for (k = slavePtr->tempY; weight > MINWEIGHT; k++) {
			double wt = r->weightY[k];
			double dy = (wt * weight_diff) / weight;
			r->weightY[k] += dy;
			weight_diff -= dy;
			weight -= wt;
		    }
		    /* Assign the remainder to the bottom cell */
		    r->weightY[py-1] += weight_diff;
		}

		/*
		 * Calculate the minHeight array values.
		 * First, figure out how tall the current slave needs to be.
		 * Then, see if it will fit within the current minHeight values.
		 * If it won\'t fit, add the difference according to the weightY array.
		 */

		pixels_diff = slavePtr->minHeight + slavePtr->padY + slavePtr->iPadY;
		for (k = slavePtr->tempY; k < py; k++)
		    pixels_diff -= r->minHeight[k];
		if (pixels_diff > 0) {
		    weight = 0.0;
		    for (k = slavePtr->tempY; k < py; k++)
			weight += r->weightY[k];
		    for (k = slavePtr->tempY; weight > MINWEIGHT; k++) {
			double wt = r->weightY[k];
			int dy = (int)((wt * ((double)pixels_diff)) / weight);
			r->minHeight[k] += dy;
			pixels_diff -= dy;
			weight -= wt;
		    }
		    /* Any leftovers go into the bottom cell */
		    r->minHeight[py-1] += pixels_diff;
		}
	    }
	    else if (slavePtr->tempHeight > i && slavePtr->tempHeight < nextSize)
		nextSize = slavePtr->tempHeight;
	}
    }

    /*
     * Apply minimum row/column dimensions
     */
    for (i=0; i<masterPtr->column.used; i++) {
	if (r->minWidth[i] < masterPtr->column.minsize[i])
	    r->minWidth[i] = masterPtr->column.minsize[i];
	if (r->weightX[i] < masterPtr->column.weight[i])
	    r->weightX[i] = masterPtr->column.weight[i];
    }
    for (i=0; i<masterPtr->row.used; i++) {
	if (r->minHeight[i] < masterPtr->row.minsize[i])
	    r->minHeight[i] = masterPtr->row.minsize[i];
	if (r->weightY[i] < masterPtr->row.weight[i])
	    r->weightY[i] = masterPtr->row.weight[i];
    }
}

/*
 * Cache the layout info after it is calculated.
 */
static void
GetCachedLayoutInfo(masterPtr)
    GridBag *masterPtr;
{
    if (masterPtr->valid == 0) {
	if (!masterPtr->layoutCache)
	    masterPtr->layoutCache = (LayoutInfo *)ckalloc(sizeof(LayoutInfo));

	GetLayoutInfo(masterPtr, masterPtr->layoutCache);
	masterPtr->valid = 1;
    }
}

/*
 * Adjusts the x, y, width, and height fields to the correct
 * values depending on the constraint geometry and pads.
 */

static void
AdjustForGravity(gridPtr, x, y, width, height)
    GridBag *gridPtr;
    int *x;
    int *y;
    int *width;
    int *height;
{
    int diffx=0, diffy=0;
    int sticky = gridPtr->flags&STICK_ALL;

    *x += gridPtr->padX/2;
    *width -= gridPtr->padX;
    *y += gridPtr->padY/2;
    *height -= gridPtr->padY;

    if (*width > (gridPtr->minWidth + gridPtr->iPadX)) {
	diffx = *width - (gridPtr->minWidth + gridPtr->iPadX);
	*width = gridPtr->minWidth + gridPtr->iPadX;
    }

    if (*height > (gridPtr->minHeight + gridPtr->iPadY)) {
	diffy = *height - (gridPtr->minHeight + gridPtr->iPadY);
	*height = gridPtr->minHeight + gridPtr->iPadY;
    }

    if (sticky&STICK_EAST && sticky&STICK_WEST)
	*width += diffx;
    if (sticky&STICK_NORTH && sticky&STICK_SOUTH)
	*height += diffy;
    if (!(sticky&STICK_WEST)) {
	if (sticky&STICK_EAST)
	    *x += diffx;
	else
	    *x += diffx/2;
    }
    if (!(sticky&STICK_NORTH)) {
	if (sticky&STICK_SOUTH)
	    *y += diffy;
	else
	    *y += diffy/2;
    }
}

/*
 * Figure out the minimum size (not counting the X border) of the
 * master based on the information from GetLayoutInfo()
 */

static void
GetMinSize(masterPtr, info, minw, minh)
    GridBag *masterPtr;
    LayoutInfo *info;
    int *minw;
    int *minh;
{
    int i, t;
    int intBWidth;	/* Width of internal border in parent window,
			 * if any. */

    intBWidth = Tk_InternalBorderWidth(masterPtr->tkwin);

    t = 0;
    for(i = 0; i < info->width; i++)
	t += info->minWidth[i];
    *minw = t + 2*intBWidth;

    t = 0;
    for(i = 0; i < info->height; i++)
	t += info->minHeight[i];
    *minh = t + 2*intBWidth;
}

/*
 *--------------------------------------------------------------
 *
 * ArrangeGrid --
 *
 *	This procedure is invoked (using the Tk_DoWhenIdle
 *	mechanism) to re-layout a set of windows managed by
 *	the gridbag.  It is invoked at idle time so that a
 *	series of gridbag requests can be merged into a single
 *	layout operation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The slaves of masterPtr may get resized or moved.
 *
 *--------------------------------------------------------------
 */

static void
ArrangeGrid(clientData)
    ClientData clientData;	/* Structure describing parent whose slaves
				 * are to be re-layed out. */
{
    register GridBag *masterPtr = (GridBag *) clientData;
    register GridBag *slavePtr;	
    int abort;
    int i, x, y, width, height;
    int diffw, diffh;
    double weight;
    Tk_Window parent, ancestor;
    LayoutInfo info;
    int intBWidth;	/* Width of internal border in parent window,
			 * if any. */
    int iPadX, iPadY;

    masterPtr->flags &= ~REQUESTED_RELAYOUT;

    /*
     * If the parent has no slaves anymore, then don't do anything
     * at all:  just leave the parent's size as-is.
     * Even if row and column constraints have been set!
     */

    if (masterPtr->slavePtr == NULL) {
	return;
    }

    /*
     * Abort any nested call to ArrangeGrid for this window, since
     * we'll do everything necessary here, and set up so this call
     * can be aborted if necessary.  
     */

    if (masterPtr->abortPtr != NULL) {
	*masterPtr->abortPtr = 1;
    }
    masterPtr->abortPtr = &abort;
    abort = 0;
    Tk_Preserve((ClientData) masterPtr);

    /*
     * Pass #1: scan all the slaves to figure out the total amount
     * of space needed.
     */

    GetLayoutInfo(masterPtr, &info);
    GetMinSize(masterPtr, &info, &width, &height);

    if (((width != Tk_ReqWidth(masterPtr->tkwin))
	    || (height != Tk_ReqHeight(masterPtr->tkwin)))
	    && !(masterPtr->flags & DONT_PROPAGATE)) {
	Tk_GeometryRequest(masterPtr->tkwin, width, height);
	masterPtr->flags |= REQUESTED_RELAYOUT;
	masterPtr->valid = 0;
	Tk_DoWhenIdle(ArrangeGrid, (ClientData) masterPtr);
	goto done;
    }

    /*
     * If the parent isn't mapped then don't do anything more:  wait
     * until it gets mapped again.  Need to get at least to here to
     * reflect size needs up the window hierarchy, but there's no
     * point in actually mapping the slaves.
     */

    if (!Tk_IsMapped(masterPtr->tkwin)) {
	goto done;
    }


    /*
     * If the current dimensions of the window don't match the desired
     * dimensions, then adjust the minWidth and minHeight arrays
     * according to the weights.
     */

    diffw = Tk_Width(masterPtr->tkwin) - (width + masterPtr->iPadX);
    if (diffw != 0) {
	weight = 0.0;
	for (i = 0; i < info.width; i++)
	    weight += info.weightX[i];
	if (weight > MINWEIGHT) {
	    for (i = 0; i < info.width; i++) {
		int dx = (int)(( ((double)diffw) * info.weightX[i]) / weight);
		info.minWidth[i] += dx;
		width += dx;
		if (info.minWidth[i] < 0) {
		    width -= info.minWidth[i];
		    info.minWidth[i] = 0;
		}
	    }
	}
	diffw = Tk_Width(masterPtr->tkwin) - (width + masterPtr->iPadX);
    }
    else {
	diffw = 0;
    }

    diffh = Tk_Height(masterPtr->tkwin) - (height + masterPtr->iPadY);
    if (diffh != 0) {
	weight = 0.0;
	for (i = 0; i < info.height; i++)
	    weight += info.weightY[i];
	if (weight > MINWEIGHT) {
	    for (i = 0; i < info.height; i++) {
		int dy = (int)(( ((double)diffh) * info.weightY[i]) / weight);
		info.minHeight[i] += dy;
		height += dy;
		if (info.minHeight[i] < 0) {
		    height -= info.minHeight[i];
		    info.minHeight[i] = 0;
		}
	    }
	}
	diffh = Tk_Height(masterPtr->tkwin) - (height + masterPtr->iPadY);
    }
    else {
	diffh = 0;
    }

    /*
     * Now do the actual layout of the slaves using the layout information
     * that has been collected.
     */

    iPadX = masterPtr->iPadX/2;
    iPadY = masterPtr->iPadY/2;
    intBWidth = Tk_InternalBorderWidth(masterPtr->tkwin);

    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
					 slavePtr = slavePtr->nextPtr) {

	masterPtr->startx = x = diffw/2 + intBWidth + iPadX;
	for(i = 0; i < slavePtr->tempX; i++)
	    x += info.minWidth[i];

	masterPtr->starty = y = diffh/2 + intBWidth + iPadY;
	for(i = 0; i < slavePtr->tempY; i++)
	    y += info.minHeight[i];

	width = 0;
	for(i = slavePtr->tempX; i < (slavePtr->tempX + slavePtr->tempWidth); i++)
	    width += info.minWidth[i];

	height = 0;
	for(i = slavePtr->tempY; i < (slavePtr->tempY + slavePtr->tempHeight); i++)
	    height += info.minHeight[i];

	AdjustForGravity(slavePtr, &x, &y, &width, &height);

	/*
	 * If the window in which slavePtr is managed is not its
	 * parent in the window hierarchy, translate the coordinates
	 * to the coordinate system of the real X parent.
	 */

	parent = Tk_Parent(slavePtr->tkwin);
	for (ancestor = masterPtr->tkwin; ancestor != parent;
					  ancestor = Tk_Parent(ancestor)) {
	    x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
	    y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
	}

	/*
	 * If the window is too small to be interesting then
	 * unmap it.  Otherwise configure it and then make sure
	 * it's mapped.
	 */

	if ((width <= 0) || (height <= 0)) {
	    Tk_UnmapWindow(slavePtr->tkwin);
	}
	else {
	    if ((x != Tk_X(slavePtr->tkwin))
		    || (y != Tk_Y(slavePtr->tkwin))
		    || (width != Tk_Width(slavePtr->tkwin))
		    || (height != Tk_Height(slavePtr->tkwin))) {
		Tk_MoveResizeWindow(slavePtr->tkwin, x, y, width, height);
	    }
	    if (abort) {
		goto done;
	    }
	    Tk_MapWindow(slavePtr->tkwin);
	}

	/*
	 * Changes to the window's structure could cause almost anything
	 * to happen, including deleting the parent or child.  If this
	 * happens, we'll be told to abort.
	 */

	if (abort) {
	    goto done;
	}
    }

    done:
    masterPtr->abortPtr = NULL;
    Tk_Release((ClientData) masterPtr);
}



/*
 *--------------------------------------------------------------
 *
 * GetGridBag --
 *
 *	This internal procedure is used to locate a GridBag
 *	structure for a given window, creating one if one
 *	doesn't exist already.
 *
 * Results:
 *	The return value is a pointer to the GridBag structure
 *	corresponding to tkwin.
 *
 * Side effects:
 *	A new gridbag structure may be created.  If so, then
 *	a callback is set up to clean things up when the
 *	window is deleted.
 *
 *--------------------------------------------------------------
 */

static GridBag *
GetGridBag(tkwin)
    Tk_Window tkwin;		/* Token for window for which
				 * gridbag structure is desired. */
{
    register GridBag *gridPtr;
    Tcl_HashEntry *hPtr;
    int new;

    if (!initialized) {
	initialized = 1;
	Tcl_InitHashTable(&gridBagHashTable, TCL_ONE_WORD_KEYS);
    }

    /*
     * See if there's already gridbag for this window.  If not,
     * then create a new one.
     */

    hPtr = Tcl_CreateHashEntry(&gridBagHashTable, (char *) tkwin, &new);
    if (!new) {
	return (GridBag *) Tcl_GetHashValue(hPtr);
    }
    gridPtr = (GridBag *) ckalloc(sizeof(GridBag));
    gridPtr->tkwin = tkwin;
    gridPtr->masterPtr = NULL;
    gridPtr->nextPtr = NULL;
    gridPtr->slavePtr = NULL;

    gridPtr->gridColumn = gridPtr->gridRow = -1;
    gridPtr->gridWidth = gridPtr->gridHeight = 1;
    gridPtr->weightX = gridPtr->weightY = 0.0;
    gridPtr->minWidth = gridPtr->minHeight = 0;

    gridPtr->padX = gridPtr->padY = 0;
    gridPtr->iPadX = gridPtr->iPadY = 0;
    gridPtr->startx = gridPtr->starty = 0;
    gridPtr->doubleBw = 2*Tk_Changes(tkwin)->border_width;
    gridPtr->abortPtr = NULL;
    gridPtr->flags = 0;

    gridPtr->column.max = 0;
    gridPtr->row.max = 0;
    gridPtr->column.used = 0;
    gridPtr->row.used = 0;

    gridPtr->valid = 0;
    gridPtr->layoutCache = NULL;

    Tcl_SetHashValue(hPtr, gridPtr);
    Tk_CreateEventHandler(tkwin, StructureNotifyMask,
	    GridBagStructureProc, (ClientData) gridPtr);
    return gridPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Unlink --
 *
 *	Remove a gridbag from its parent's list of slaves.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The parent will be scheduled for re-arranging.
 *
 *----------------------------------------------------------------------
 */

static void
Unlink(gridPtr)
    register GridBag *gridPtr;		/* Window to unlink. */
{
    register GridBag *masterPtr, *gridPtr2;

    masterPtr = gridPtr->masterPtr;
    if (masterPtr == NULL) {
	return;
    }
    if (masterPtr->slavePtr == gridPtr) {
	masterPtr->slavePtr = gridPtr->nextPtr;
    }
    else {
	for (gridPtr2 = masterPtr->slavePtr; ; gridPtr2 = gridPtr2->nextPtr) {
	    if (gridPtr2 == NULL) {
		panic("Unlink couldn't find previous window");
	    }
	    if (gridPtr2->nextPtr == gridPtr) {
		gridPtr2->nextPtr = gridPtr->nextPtr;
		break;
	    }
	}
    }
    masterPtr->valid = 0;
    if (!(masterPtr->flags & REQUESTED_RELAYOUT)) {
	masterPtr->flags |= REQUESTED_RELAYOUT;
	Tk_DoWhenIdle(ArrangeGrid, (ClientData) masterPtr);
    }
    if (masterPtr->abortPtr != NULL) {
	*masterPtr->abortPtr = 1;
    }

    gridPtr->masterPtr = NULL;
}



/*
 *----------------------------------------------------------------------
 *
 * DestroyGridBag --
 *
 *	This procedure is invoked by Tk_EventuallyFree or Tk_Release
 *	to clean up the internal structure of a gridbag at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the gridbag is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyGridBag(memPtr)
    char *memPtr;		/* Info about window that is now dead. */
{
    register GridBag *gridPtr = (GridBag *) memPtr;

    if (gridPtr->column.max) {
	ckfree((char *) gridPtr->column.minsize);
	ckfree((char *) gridPtr->column.weight);
    }
    if (gridPtr->row.max) {
	ckfree((char *) gridPtr->row.minsize);
	ckfree((char *) gridPtr->row.weight);
    }
    if (gridPtr->layoutCache)
	ckfree((char *) gridPtr->layoutCache);

    ckfree((char *) gridPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * GridBagStructureProc --
 *
 *	This procedure is invoked by the Tk event dispatcher in response
 *	to StructureNotify events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a window was just deleted, clean up all its gridbag-related
 *	information.  If it was just resized, re-configure its slaves, if
 *	any.
 *
 *----------------------------------------------------------------------
 */

static void
GridBagStructureProc(clientData, eventPtr)
    ClientData clientData;		/* Our information about window
					 * referred to by eventPtr. */
    XEvent *eventPtr;			/* Describes what just happened. */
{
    register GridBag *gridPtr = (GridBag *) clientData;

    if (eventPtr->type == ConfigureNotify) {
	gridPtr->valid = 0;
	if (!(gridPtr->flags & REQUESTED_RELAYOUT)) {
	    gridPtr->flags |= REQUESTED_RELAYOUT;
	    Tk_DoWhenIdle(ArrangeGrid, (ClientData) gridPtr);
	}
	if (gridPtr->doubleBw != 2*Tk_Changes(gridPtr->tkwin)->border_width) {
	    if ((gridPtr->masterPtr != NULL) &&
		    !(gridPtr->masterPtr->flags & REQUESTED_RELAYOUT)) {
		gridPtr->doubleBw = 2*Tk_Changes(gridPtr->tkwin)->border_width;
		gridPtr->masterPtr->flags |= REQUESTED_RELAYOUT;
		Tk_DoWhenIdle(ArrangeGrid, (ClientData) gridPtr->masterPtr);
	    }
	}
    }
    else if (eventPtr->type == DestroyNotify) {
	register GridBag *gridPtr2, *nextPtr;

	if (gridPtr->masterPtr != NULL) {
	    Unlink(gridPtr);
	}
	for (gridPtr2 = gridPtr->slavePtr; gridPtr2 != NULL;
					   gridPtr2 = nextPtr) {
	    Tk_UnmapWindow(gridPtr2->tkwin);
	    gridPtr2->masterPtr = NULL;
	    nextPtr = gridPtr2->nextPtr;
	    gridPtr2->nextPtr = NULL;
	}
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&gridBagHashTable,
		(char *) gridPtr->tkwin));
	if (gridPtr->flags & REQUESTED_RELAYOUT) {
	    Tk_CancelIdleCall(ArrangeGrid, (ClientData) gridPtr);
	}
	gridPtr->tkwin = NULL;
	Tk_EventuallyFree((ClientData) gridPtr, DestroyGridBag);
    }
    else if (eventPtr->type == MapNotify) {
	gridPtr->valid = 0;
	if (!(gridPtr->flags & REQUESTED_RELAYOUT)) {
	    gridPtr->flags |= REQUESTED_RELAYOUT;
	    Tk_DoWhenIdle(ArrangeGrid, (ClientData) gridPtr);
	}
    }
    else if (eventPtr->type == UnmapNotify) {
	register GridBag *gridPtr2;

	for (gridPtr2 = gridPtr->slavePtr; gridPtr2 != NULL;
					   gridPtr2 = gridPtr2->nextPtr) {
	    Tk_UnmapWindow(gridPtr2->tkwin);
	}
    }
}



/*
 *----------------------------------------------------------------------
 *
 * ConfigureSlaves --
 *
 *	This implements the guts of the "grid configure" command.  Given
 *	a list of slaves and configuration options, it arranges for the
 *	gridbag to manage the slaves and sets the specified options.
 *
 * Results:
 *	TCL_OK is returned if all went well.  Otherwise, TCL_ERROR is
 *	returned and interp->result is set to contain an error message.
 *
 * Side effects:
 *	Slave windows get taken over by the gridbag.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureSlaves(interp, tkwin, argc, argv)
    Tcl_Interp *interp;	/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Any window in application containing
				 * slaves.  Used to look up slave names. */
    int argc;			/* Numb = 0er of elements in argv. */
    char *argv[];		/* Argument strings:  contains one or more
				 * window names followed by any number
				 * of "option value" pairs.  Caller must
				 * make sure that there is at least one
				 * window name. */
{
    GridBag *masterPtr, *slavePtr, *prevPtr;
    Tk_Window other, slave, parent, ancestor;
    int i, j, numWindows, c, length, tmp, positionGiven;
    int currentColumn=0, numColumns=1;
    int gotLayout = 0;
    int gotWidth = 0;
    int width;

    /*
     * Find out how many windows are specified. (shouldn't use harwired symbols)
     */

    for (numWindows = 0; numWindows < argc; numWindows++) {
	if (argv[numWindows][0] != '.'
		 && strcmp(argv[numWindows],"-")!=0
		 && strcmp(argv[numWindows],"^")!=0
		 && strcmp(argv[numWindows],"x")!=0) {
	    break;
	}
    }
    slave = NULL;

    /*
     * Iterate over all of the slave windows, parsing the configuration
     * options for each slave.  It's a bit wasteful to re-parse the
     * options for each slave, but things get too messy if we try to
     * parse the arguments just once at the beginning.  For example,
     * if a slave already is managed we want to just change a few
     * existing values without resetting everything.  If there are
     * multiple windows, the -in option only gets processed for the
     * first window.
     */

    masterPtr = NULL;
    prevPtr = NULL;
    positionGiven = 0;
    for (j = 0; j < numWindows; j++) {

	/* adjust default widget location for non-widgets */
	if (*argv[j] != '.') {
	    switch (*argv[j]) {
		case '^':	/* extend the widget in the previous row 
				 * Since we don't know who the master is yet,
				 * handle these in a separate pass at the end
				 */
		    /* no break */
		case REL_SKIP:	/* skip over the next column */
		    currentColumn++;
		    break;
		case REL_HORIZ:	/* increase the span, already dealt with */
		    /* not quite right */
		    if (j>0 && (*argv[j-1] == REL_SKIP || *argv[j-1] == '^')) {
			Tcl_AppendResult(interp, "Invalid grid combination:",
				" \"-\" can't follow \"", argv[j-1], "\"",NULL);
			return TCL_ERROR;
		    }
		    break;
		default:
		    panic("Invalid grid position indicator");
	    }
	    continue;
	}

	for (numColumns=1; j+numColumns < numWindows && *argv[j+numColumns] == REL_HORIZ;
		numColumns++) {
	    /* null body */
	}
	slave = Tk_NameToWindow(interp, argv[j], tkwin);
	if (slave == NULL) {
	    return TCL_ERROR;
	}
	if (Tk_IsTopLevel(slave)) {
	    Tcl_AppendResult(interp, "can't manage \"", argv[j],
		    "\": it's a top-level window", (char *) NULL);
	    return TCL_ERROR;
	}
	slavePtr = GetGridBag(slave);

	/*
	 * The following statement is taken from tkPack.c:
	 *
	 * "If the slave isn't currently managed, reset all of its
	 * configuration information to default values (there could
	 * be old values left from a previous packer)."
	 *
	 * I disagree with this statement.  If a slave is disabled (using
	 * "forget") and then re-enabled, I submit that 90% of the time the
	 * programmer will want it to retain its old configuration information.
	 * If the programmer doesn't want this behavior, then she can reset the
	 * defaults for herself, but she will never have to worry about keeping
	 * track of the old state. 
	 */

	for (i = numWindows; i < argc; i+=2) {
	    if ((i+2) > argc) {
		Tcl_AppendResult(interp, "extra option \"", argv[i],
			"\" (option with no value?)", (char *) NULL);
		return TCL_ERROR;
	    }
	    length = strlen(argv[i]);
	    if (length < 2) {
		goto badOption;
	    }
	    c = argv[i][1];
	    if ((c == 'i') && (strcmp(argv[i], "-in") == 0)) {
		if (j == 0) {
		    other = Tk_NameToWindow(interp, argv[i+1], tkwin);
		    if (other == NULL) {
			return TCL_ERROR;
		    }
		    if (other == slave) {
			sprintf(interp->result,"Window can't be managed in itself");
			return TCL_ERROR;
		    }
		    masterPtr = GetGridBag(other);
		    prevPtr = masterPtr->slavePtr;
		    if (prevPtr != NULL) {
			while (prevPtr->nextPtr != NULL) {
			    prevPtr = prevPtr->nextPtr;
			}
		    }
		    positionGiven = 1;
		}
	    } else if ((c == 'i') && (strcmp(argv[i], "-ipadx") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp < 0)) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad ipadx value \"", argv[i+1],
			    "\": must be positive screen distance",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		slavePtr->iPadX = tmp*2;
	    } else if ((c == 'i') && (strcmp(argv[i], "-ipady") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad ipady value \"", argv[i+1],
			    "\": must be positive screen distance",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		slavePtr->iPadY = tmp*2;
	    } else if ((c == 'p') && (strcmp(argv[i], "-padx") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad padx value \"", argv[i+1],
			    "\": must be positive screen distance",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		slavePtr->padX = tmp*2;
	    } else if ((c == 'p') && (strcmp(argv[i], "-pady") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad pady value \"", argv[i+1],
			    "\": must be positive screen distance",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		slavePtr->padY = tmp*2;
	    } else if ((c == 'c') && (strcmp(argv[i], "-column") == 0)) {
		if (Tcl_GetInt(interp, argv[i+1], &tmp) != TCL_OK || tmp<0) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad column value \"", argv[i+1],
			    "\": must be a non-negative integer", (char *)NULL);
		    return TCL_ERROR;
		}
		slavePtr->gridColumn = tmp;
	    } else if ((c == 'r') && (strcmp(argv[i], "-row") == 0)) {
		if (Tcl_GetInt(interp, argv[i+1], &tmp) != TCL_OK || tmp<0) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad grid value \"", argv[i+1],
			    "\": must be a non-negative integer", (char *)NULL);
		    return TCL_ERROR;
		}
		slavePtr->gridRow = tmp;
	    } else if ((c == 'c') && (strcmp(argv[i], "-columnspan") == 0)) {
		if (Tcl_GetInt(interp, argv[i+1], &tmp) != TCL_OK || tmp <= 0) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad columnspan value \"", argv[i+1],
			    "\": must be a positive integer", (char *)NULL);
		    return TCL_ERROR;
		}
		slavePtr->gridWidth = tmp;
		gotWidth++;
	    } else if ((c == 'r') && (strcmp(argv[i], "-rowspan") == 0)) {
		if (Tcl_GetInt(interp, argv[i+1], &tmp) != TCL_OK) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad rowspan value \"", argv[i+1],
			    "\": must be a positive integer", (char *)NULL);
		    return TCL_ERROR;
		}
		slavePtr->gridHeight = tmp;
/*
	    } else if ((c == 'w') &&
		    (!strcmp(argv[i], "-weightx") || !strcmp(argv[i], "-wx"))) {
		if (Tcl_GetDouble(interp, argv[i+1], &tmp_dbl) != TCL_OK) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad weight value \"", argv[i+1],
			    "\": must be a double", (char *)NULL);
		    return TCL_ERROR;
		}
		slavePtr->weightX = tmp_dbl;
	    }
	    else if ((c == 'w') &&
		    (!strcmp(argv[i], "-weighty") || !strcmp(argv[i], "-wy"))) {
		if (Tcl_GetDouble(interp, argv[i+1], &tmp_dbl) != TCL_OK) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad weight value \"", argv[i+1],
			    "\": must be a double", (char *)NULL);
		    return TCL_ERROR;
		}
		slavePtr->weightY = tmp_dbl;
*/
	    } else if ((c == 's') && strcmp(argv[i], "-sticky") == 0) {
		int sticky = StringToSticky(argv[i+1]);
		if (sticky == -1) {
		    Tcl_AppendResult(interp, "bad stickyness value \"", argv[i+1],
			    "\": must be a string containing n, e, s, and/or w", (char *)NULL);
		    return TCL_ERROR;
		}
		slavePtr->flags = sticky | (slavePtr->flags & ~STICK_ALL);
	    } else {
		badOption:
		Tcl_AppendResult(interp, "unknown or ambiguous option \"",
			argv[i], "\": must be -in, -sticky, ",
			"-row, -column, -rowspan, -columnspan, ",
			"-ipadx, -ipady, -padx or -pady.",
			(char *) NULL);
		return TCL_ERROR;
	    }
	}

	/*
	 * If no position in a gridbag list was specified and the slave
	 * is already managed, then leave it in its current location in
	 * its current gridbag list.
	 */

	if (!positionGiven && (slavePtr->masterPtr != NULL)) {
	    masterPtr = slavePtr->masterPtr;
	    goto scheduleLayout;
	}

	/*
	 * If the slave is going to be put back after itself then
	 * skip the whole operation, since it won't work anyway.
	 */

	if (prevPtr == slavePtr) {
	    masterPtr = slavePtr->masterPtr;
	    goto scheduleLayout;
	}
    
	/*
	 * If the "-in" option has not been specified, arrange for the
	 * slave to go at the end of the order for its parent.
	 */
    
	if (!positionGiven) {
	    masterPtr = GetGridBag(Tk_Parent(slave));
	    prevPtr = masterPtr->slavePtr;
	    if (prevPtr != NULL) {
		while (prevPtr->nextPtr != NULL) {
		    prevPtr = prevPtr->nextPtr;
		}
	    }
	}

	/*
	 * Make sure that the slave's parent is either the master or
	 * an ancestor of the master.
	 */
    
	parent = Tk_Parent(slave);
	for (ancestor = masterPtr->tkwin; ; ancestor = Tk_Parent(ancestor)) {
	    if (ancestor == parent) {
		break;
	    }
	    if (Tk_IsTopLevel(ancestor)) {
		Tcl_AppendResult(interp, "can't put ", argv[j],
			" inside ", Tk_PathName(masterPtr->tkwin),
			(char *) NULL);
		return TCL_ERROR;
	    }
	}

	/*
	 * Unlink the slave if it's currently managed, then position it
	 * after prevPtr.
	 */

	if (slavePtr->masterPtr != NULL) {
	    Unlink(slavePtr);
	}
	slavePtr->masterPtr = masterPtr;
	if (prevPtr == NULL) {
	    slavePtr->nextPtr = masterPtr->slavePtr;
	    masterPtr->slavePtr = slavePtr;
	} else {
	    slavePtr->nextPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = slavePtr;
	}
	Tk_ManageGeometry(slave, &gridMgrType, (ClientData) slavePtr);
	prevPtr = slavePtr;

	/* assign default row and column */

	if (slavePtr->gridColumn == -1) {
	    slavePtr->gridColumn = currentColumn;
	}
	slavePtr->gridWidth += numColumns - 1;
	if (slavePtr->gridRow == -1) {
	    if (!gotLayout++) GetCachedLayoutInfo(masterPtr);
	    slavePtr->gridRow = masterPtr->layoutCache->lastRow;
	}

	/*
	 * Arrange for the parent to be re-arranged at the first
	 * idle moment.
	 */

	scheduleLayout:
	if (masterPtr->abortPtr != NULL) {
	    *masterPtr->abortPtr = 1;
	}
	masterPtr->valid = 0;
	if (!(masterPtr->flags & REQUESTED_RELAYOUT)) {
	    masterPtr->flags |= REQUESTED_RELAYOUT;
	    Tk_DoWhenIdle(ArrangeGrid, (ClientData) masterPtr);
	}
	currentColumn += slavePtr->gridWidth;
	numColumns = 1;
    }

    /* now look for all the "^"'s */

    for (j = 0; j < numWindows; j++) {
	struct GridBag *otherPtr;
    	char *lastWindow;	/* use this window to base current row/col on */
	int match;		/* found a match for the ^ */

    	if (*argv[j] == '.') {
	    lastWindow = argv[j];
	}
	if (*argv[j] != '^') {
	    continue;
	}
	for (width=1; width+j < numWindows && *argv[j+width] == '^'; width++) {
	    /* Null Body */
	}
	other = Tk_NameToWindow(interp, lastWindow, tkwin);
	otherPtr = GetGridBag(other);
	if (!gotLayout++) GetCachedLayoutInfo(masterPtr);

	for (match=0, slavePtr = masterPtr->slavePtr; slavePtr != NULL;
					 slavePtr = slavePtr->nextPtr) {

	    if (slavePtr->gridWidth == width
		    && slavePtr->gridColumn == otherPtr->gridColumn + otherPtr->gridWidth
		    && slavePtr->gridRow + slavePtr->gridHeight == otherPtr->gridRow) {
		slavePtr->gridHeight++;
		match++;
	    }
	    lastWindow = Tk_PathName(slavePtr->tkwin);
	}
	if (!match) {
	    Tcl_AppendResult(interp, "can't find slave to extend with \"^\"",
		    " after ",lastWindow,
		    (char *) NULL);
	    return TCL_ERROR;
	}
	j += width - 1;
    }
    return TCL_OK;
}

/* convert "Sticky" bits into a string */

static void
StickyToString(flags, result)
    int flags;		/* the sticky flags */
    char *result;	/* where to put the result */
{
    int count = 0;
    if (flags&STICK_NORTH) result[count++] = 'n';
    if (flags&STICK_EAST) result[count++] = 'e';
    if (flags&STICK_SOUTH) result[count++] = 's';
    if (flags&STICK_WEST) result[count++] = 'w';
    if (count) {
	result[count] = '\0';
    } else {
	sprintf(result,"{}");
    }
}

/* convert sticky string to flags */

static int
StringToSticky(string)
    char *string;
{
    int sticky = 0;
    char c;

    while ((c = *string++) != '\0') {
	switch (c) {
	    case 'n': case 'N': sticky |= STICK_NORTH; break;
	    case 'e': case 'E': sticky |= STICK_EAST;  break;
	    case 's': case 'S': sticky |= STICK_SOUTH; break;
	    case 'w': case 'W': sticky |= STICK_WEST;  break;
	    case ' ': case ',': case '\t': case '\r': case '\n': break;
	    default: return -1;
	}
    }
    return sticky;
}		
