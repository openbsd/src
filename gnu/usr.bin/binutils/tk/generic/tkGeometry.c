/* 
 * tkGeometry.c --
 *
 *	This file contains generic Tk code for geometry management
 *	(stuff that's used by all geometry managers).
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkGeometry.c 1.31 96/02/15 18:53:32
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * Data structures of the following type are used by Tk_MaintainGeometry.
 * For each slave managed by Tk_MaintainGeometry, there is one of these
 * structures associated with its master.
 */

typedef struct MaintainSlave {
    Tk_Window slave;		/* The slave window being positioned. */
    Tk_Window master;		/* The master that determines slave's
				 * position; it must be a descendant of
				 * slave's parent. */
    int x, y;			/* Desired position of slave relative to
				 * master. */
    int width, height;		/* Desired dimensions of slave. */
    struct MaintainSlave *nextPtr;
				/* Next in list of Maintains associated
				 * with master. */
} MaintainSlave;

/*
 * For each window that has been specified as a master to
 * Tk_MaintainGeometry, there is a structure of the following type:
 */

typedef struct MaintainMaster {
    Tk_Window ancestor;		/* The lowest ancestor of this window
				 * for which we have *not* created a
				 * StructureNotify handler.  May be the
				 * same as the window itself. */
    int checkScheduled;		/* Non-zero means that there is already a
				 * call to MaintainCheckProc scheduled as
				 * an idle handler. */
    MaintainSlave *slavePtr;	/* First in list of all slaves associated
				 * with this master. */
} MaintainMaster;

/*
 * Hash table that maps from a master's Tk_Window token to a list of
 * Maintains for that master:
 */

static Tcl_HashTable maintainHashTable;

/*
 * Has maintainHashTable been initialized yet?
 */

static int initialized = 0;

/*
 * Prototypes for static procedures in this file:
 */

static void		MaintainCheckProc _ANSI_ARGS_((ClientData clientData));
static void		MaintainMasterProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		MaintainSlaveProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));

/*
 *--------------------------------------------------------------
 *
 * Tk_ManageGeometry --
 *
 *	Arrange for a particular procedure to manage the geometry
 *	of a given slave window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc becomes the new geometry manager for tkwin, replacing
 *	any previous geometry manager.  The geometry manager will
 *	be notified (by calling procedures in *mgrPtr) when interesting
 *	things happen in the future.  If there was an existing geometry
 *	manager for tkwin different from the new one, it is notified
 *	by calling its lostSlaveProc.
 *
 *--------------------------------------------------------------
 */

void
Tk_ManageGeometry(tkwin, mgrPtr, clientData)
    Tk_Window tkwin;		/* Window whose geometry is to
				 * be managed by proc.  */
    Tk_GeomMgr *mgrPtr;		/* Static structure describing the
				 * geometry manager.  This structure
				 * must never go away. */
    ClientData clientData;	/* Arbitrary one-word argument to
				 * pass to geometry manager procedures. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    if ((winPtr->geomMgrPtr != NULL) && (mgrPtr != NULL)
	    && ((winPtr->geomMgrPtr != mgrPtr)
		|| (winPtr->geomData != clientData))
	    && (winPtr->geomMgrPtr->lostSlaveProc != NULL)) {
	(*winPtr->geomMgrPtr->lostSlaveProc)(winPtr->geomData, tkwin);
    }

    winPtr->geomMgrPtr = mgrPtr;
    winPtr->geomData = clientData;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GeometryRequest --
 *
 *	This procedure is invoked by widget code to indicate
 *	its preferences about the size of a window it manages.
 *	In general, widget code should call this procedure
 *	rather than Tk_ResizeWindow.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The geometry manager for tkwin (if any) is invoked to
 *	handle the request.  If possible, it will reconfigure
 *	tkwin and/or other windows to satisfy the request.  The
 *	caller gets no indication of success or failure, but it
 *	will get X events if the window size was actually
 *	changed.
 *
 *--------------------------------------------------------------
 */

void
Tk_GeometryRequest(tkwin, reqWidth, reqHeight)
    Tk_Window tkwin;		/* Window that geometry information
				 * pertains to. */
    int reqWidth, reqHeight;	/* Minimum desired dimensions for
				 * window, in pixels. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    /*
     * X gets very upset if a window requests a width or height of
     * zero, so rounds requested sizes up to at least 1.
     */

    if (reqWidth <= 0) {
	reqWidth = 1;
    }
    if (reqHeight <= 0) {
	reqHeight = 1;
    }
    if ((reqWidth == winPtr->reqWidth) && (reqHeight == winPtr->reqHeight)) {
	return;
    }
    winPtr->reqWidth = reqWidth;
    winPtr->reqHeight = reqHeight;
    if ((winPtr->geomMgrPtr != NULL)
	    && (winPtr->geomMgrPtr->requestProc != NULL)) {
	(*winPtr->geomMgrPtr->requestProc)(winPtr->geomData, tkwin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetInternalBorder --
 *
 *	Notify relevant geometry managers that a window has an internal
 *	border of a given width and that child windows should not be
 *	placed on that border.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The border width is recorded for the window, and all geometry
 *	managers of all children are notified so that can re-layout, if
 *	necessary.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetInternalBorder(tkwin, width)
    Tk_Window tkwin;		/* Window that will have internal border. */
    int width;			/* Width of internal border, in pixels. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    if (width == winPtr->internalBorderWidth) {
	return;
    }
    if (width < 0) {
	width = 0;
    }
    winPtr->internalBorderWidth = width;

    /*
     * All the slaves for which this is the master window must now be
     * repositioned to take account of the new internal border width.
     * To signal all the geometry managers to do this, just resize the
     * window to its current size.  The ConfigureNotify event will
     * cause geometry managers to recompute everything.
     */

    Tk_ResizeWindow(tkwin, Tk_Width(tkwin), Tk_Height(tkwin));
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MaintainGeometry --
 *
 *	This procedure is invoked by geometry managers to handle slaves
 *	whose master's are not their parents.  It translates the desired
 *	geometry for the slave into the coordinate system of the parent
 *	and respositions the slave if it isn't already at the right place.
 *	Furthermore, it sets up event handlers so that if the master (or
 *	any of its ancestors up to the slave's parent) is mapped, unmapped,
 *	or moved, then the slave will be adjusted to match.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Event handlers are created and state is allocated to keep track
 *	of slave.  Note:  if slave was already managed for master by
 *	Tk_MaintainGeometry, then the previous information is replaced
 *	with the new information.  The caller must eventually call
 *	Tk_UnmaintainGeometry to eliminate the correspondence (or, the
 *	state is automatically freed when either window is destroyed).
 *
 *----------------------------------------------------------------------
 */

void
Tk_MaintainGeometry(slave, master, x, y, width, height)
    Tk_Window slave;		/* Slave for geometry management. */
    Tk_Window master;		/* Master for slave; must be a descendant
				 * of slave's parent. */
    int x, y;			/* Desired position of slave within master. */
    int width, height;		/* Desired dimensions for slave. */
{
    Tcl_HashEntry *hPtr;
    MaintainMaster *masterPtr;
    register MaintainSlave *slavePtr;
    int new, map;
    Tk_Window ancestor, parent;

    if (!initialized) {
	initialized = 1;
	Tcl_InitHashTable(&maintainHashTable, TCL_ONE_WORD_KEYS);
    }

    /*
     * See if there is already a MaintainMaster structure for the master;
     * if not, then create one.
     */

    parent = Tk_Parent(slave);
    hPtr = Tcl_CreateHashEntry(&maintainHashTable, (char *) master, &new);
    if (!new) {
	masterPtr = (MaintainMaster *) Tcl_GetHashValue(hPtr);
    } else {
	masterPtr = (MaintainMaster *) ckalloc(sizeof(MaintainMaster));
	masterPtr->ancestor = master;
	masterPtr->checkScheduled = 0;
	masterPtr->slavePtr = NULL;
	Tcl_SetHashValue(hPtr, masterPtr);
    }

    /*
     * Create a MaintainSlave structure for the slave if there isn't
     * already one.
     */

    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	if (slavePtr->slave == slave) {
	    goto gotSlave;
	}
    }
    slavePtr = (MaintainSlave *) ckalloc(sizeof(MaintainSlave));
    slavePtr->slave = slave;
    slavePtr->master = master;
    slavePtr->nextPtr = masterPtr->slavePtr;
    masterPtr->slavePtr = slavePtr;
    Tk_CreateEventHandler(slave, StructureNotifyMask, MaintainSlaveProc,
	    (ClientData) slavePtr);

    /*
     * Make sure that there are event handlers registered for all
     * the windows between master and slave's parent (including master
     * but not slave's parent).  There may already be handlers for master
     * and some of its ancestors (masterPtr->ancestor tells how many).
     */

    for (ancestor = master; ancestor != parent;
	    ancestor = Tk_Parent(ancestor)) {
	if (ancestor == masterPtr->ancestor) {
	    Tk_CreateEventHandler(ancestor, StructureNotifyMask,
		    MaintainMasterProc, (ClientData) masterPtr);
	    masterPtr->ancestor = Tk_Parent(ancestor);
	}
    }

    /*
     * Fill in up-to-date information in the structure, then update the
     * window if it's not currently in the right place or state.
     */

    gotSlave:
    slavePtr->x = x;
    slavePtr->y = y;
    slavePtr->width = width;
    slavePtr->height = height;
    map = 1;
    for (ancestor = slavePtr->master; ; ancestor = Tk_Parent(ancestor)) {
	if (!Tk_IsMapped(ancestor) && (ancestor != parent)) {
	    map = 0;
	}
	if (ancestor == parent) {
	    if ((x != Tk_X(slavePtr->slave))
		    || (y != Tk_Y(slavePtr->slave))
		    || (width != Tk_Width(slavePtr->slave))
		    || (height != Tk_Height(slavePtr->slave))) {
		Tk_MoveResizeWindow(slavePtr->slave, x, y, width, height);
	    }
	    if (map) {
		Tk_MapWindow(slavePtr->slave);
	    } else {
		Tk_UnmapWindow(slavePtr->slave);
	    }
	    break;
	}
	x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
	y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_UnmaintainGeometry --
 *
 *	This procedure cancels a previous Tk_MaintainGeometry call,
 *	so that the relationship between slave and master is no longer
 *	maintained.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The slave is unmapped and state is released, so that slave won't
 *	track master any more.  If we weren't previously managing slave
 *	relative to master, then this procedure has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_UnmaintainGeometry(slave, master)
    Tk_Window slave;		/* Slave for geometry management. */
    Tk_Window master;		/* Master for slave; must be a descendant
				 * of slave's parent. */
{
    Tcl_HashEntry *hPtr;
    MaintainMaster *masterPtr;
    register MaintainSlave *slavePtr, *prevPtr;
    Tk_Window ancestor;

    if (!initialized) {
	initialized = 1;
	Tcl_InitHashTable(&maintainHashTable, TCL_ONE_WORD_KEYS);
    }

    if (!(((TkWindow *) slave)->flags & TK_ALREADY_DEAD)) {
	Tk_UnmapWindow(slave);
    }
    hPtr = Tcl_FindHashEntry(&maintainHashTable, (char *) master);
    if (hPtr == NULL) {
	return;
    }
    masterPtr = (MaintainMaster *) Tcl_GetHashValue(hPtr);
    slavePtr = masterPtr->slavePtr;
    if (slavePtr->slave == slave) {
	masterPtr->slavePtr = slavePtr->nextPtr;
    } else {
	for (prevPtr = slavePtr, slavePtr = slavePtr->nextPtr; ;
		prevPtr = slavePtr, slavePtr = slavePtr->nextPtr) {
	    if (slavePtr == NULL) {
		return;
	    }
	    if (slavePtr->slave == slave) {
		prevPtr->nextPtr = slavePtr->nextPtr;
		break;
	    }
	}
    }
    Tk_DeleteEventHandler(slavePtr->slave, StructureNotifyMask,
	    MaintainSlaveProc, (ClientData) slavePtr);
    ckfree((char *) slavePtr);
    if (masterPtr->slavePtr == NULL) {
	if (masterPtr->ancestor != NULL) {
	    for (ancestor = master; ; ancestor = Tk_Parent(ancestor)) {
		Tk_DeleteEventHandler(ancestor, StructureNotifyMask,
			MaintainMasterProc, (ClientData) masterPtr);
		if (ancestor == masterPtr->ancestor) {
		    break;
		}
	    }
	}
	if (masterPtr->checkScheduled) {
	    Tcl_CancelIdleCall(MaintainCheckProc, (ClientData) masterPtr);
	}
	Tcl_DeleteHashEntry(hPtr);
	ckfree((char *) masterPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainMasterProc --
 *
 *	This procedure is invoked by the Tk event dispatcher in
 *	response to StructureNotify events on the master or one
 *	of its ancestors, on behalf of Tk_MaintainGeometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	It schedules a call to MaintainCheckProc, which will eventually
 *	caused the postions and mapped states to be recalculated for all
 *	the maintained slaves of the master.  Or, if the master window is
 *	being deleted then state is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainMasterProc(clientData, eventPtr)
    ClientData clientData;		/* Pointer to MaintainMaster structure
					 * for the master window. */
    XEvent *eventPtr;			/* Describes what just happened. */
{
    MaintainMaster *masterPtr = (MaintainMaster *) clientData;
    MaintainSlave *slavePtr;
    int done;

    if ((eventPtr->type == ConfigureNotify)
	    || (eventPtr->type == MapNotify)
	    || (eventPtr->type == UnmapNotify)) {
	if (!masterPtr->checkScheduled) {
	    masterPtr->checkScheduled = 1;
	    Tcl_DoWhenIdle(MaintainCheckProc, (ClientData) masterPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	/*
	 * Delete all of the state associated with this master, but
	 * be careful not to use masterPtr after the last slave is
	 * deleted, since its memory will have been freed.
	 */

	done = 0;
	do {
	    slavePtr = masterPtr->slavePtr;
	    if (slavePtr->nextPtr == NULL) {
		done = 1;
	    }
	    Tk_UnmaintainGeometry(slavePtr->slave, slavePtr->master);
	} while (!done);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainSlaveProc --
 *
 *	This procedure is invoked by the Tk event dispatcher in
 *	response to StructureNotify events on a slave being managed
 *	by Tk_MaintainGeometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the event is a DestroyNotify event then the Maintain state
 *	and event handlers for this slave are deleted.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainSlaveProc(clientData, eventPtr)
    ClientData clientData;		/* Pointer to MaintainSlave structure
					 * for master-slave pair. */
    XEvent *eventPtr;			/* Describes what just happened. */
{
    MaintainSlave *slavePtr = (MaintainSlave *) clientData;

    if (eventPtr->type == DestroyNotify) {
	Tk_UnmaintainGeometry(slavePtr->slave, slavePtr->master);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainCheckProc --
 *
 *	This procedure is invoked by the Tk event dispatcher as an
 *	idle handler, when a master or one of its ancestors has been
 *	reconfigured, mapped, or unmapped.  Its job is to scan all of
 *	the slaves for the master and reposition them, map them, or
 *	unmap them as needed to maintain their geometry relative to
 *	the master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Slaves can get repositioned, mapped, or unmapped.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainCheckProc(clientData)
    ClientData clientData;		/* Pointer to MaintainMaster structure
					 * for the master window. */
{
    MaintainMaster *masterPtr = (MaintainMaster *) clientData;
    MaintainSlave *slavePtr;
    Tk_Window ancestor, parent;
    int x, y, map;

    masterPtr->checkScheduled = 0;
    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	parent = Tk_Parent(slavePtr->slave);
	x = slavePtr->x;
	y = slavePtr->y;
	map = 1;
	for (ancestor = slavePtr->master; ; ancestor = Tk_Parent(ancestor)) {
	    if (!Tk_IsMapped(ancestor) && (ancestor != parent)) {
		map = 0;
	    }
	    if (ancestor == parent) {
		if ((x != Tk_X(slavePtr->slave))
			|| (y != Tk_Y(slavePtr->slave))) {
		    Tk_MoveWindow(slavePtr->slave, x, y);
		}
		if (map) {
		    Tk_MapWindow(slavePtr->slave);
		} else {
		    Tk_UnmapWindow(slavePtr->slave);
		}
		break;
	    }
	    x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
	    y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
	}
    }
}
