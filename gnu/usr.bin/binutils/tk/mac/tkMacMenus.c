/* 
 * tkMacMenus.c --
 *
 *	These calls set up and manage the menubar for the
 *	Macintosh version of Tk.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacMenus.c 1.23 96/03/07 18:25:48
 */

#include <Desk.h>
#include <Menus.h>
#include <Memory.h>
#include <SegLoad.h>
#include <StandardFile.h>
#include <ToolUtils.h>

#include <FullPath.h>

#include "tcl.h"
#include "tk.h"
#include "tkInt.h"
#include "tkMacInt.h"

#define kAppleMenu		256
#define kAppleAboutItem		1
#define kFileMenu		2
#define kEditMenu		3

#define kSourceItem		1
#define kCloseItem		2
#define kQuitItem		4

#define EDIT_CUT		1
#define EDIT_COPY		2
#define EDIT_PASTE		3
#define EDIT_CLEAR		4

static Tcl_Interp *	gInterp;	/* Interpreter for this application. */
static MenuHandle	gAppleM;	/* Handles to menus */
static MenuHandle	gFileM;		/* Handles to menus */
static MenuHandle	gEditM;		/* Handles to menus */

Tk_Window Tk_TopCoordsToWindow _ANSI_ARGS_((Tk_Window tkwin, int rootX,
			int rootY, int *newX, int *newY));
static void FakeEditEvent _ANSI_ARGS_((int flag));
static void SourceDialog _ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------------
 *
 * TkMacHandleMenuSelect --
 *
 *	Handles events that occur in the Menu bar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
TkMacHandleMenuSelect(mResult, optionKeyPressed)
    long mResult;
    int optionKeyPressed;
{
    short theItem = LoWord(mResult);
    short theMenu = HiWord(mResult);
    Str255 name;
    Tk_Window tkwin;
    Window window;

    if (mResult == 0) {
	return;
    }

    switch (theMenu) {
	
	case kAppleMenu:
	    switch (theItem) {
		case kAppleAboutItem:
		    {
			Tcl_CmdInfo dummy;
			
			if (optionKeyPressed || gInterp == NULL ||
			    Tcl_GetCommandInfo(gInterp,
				    "tkAboutDialog", &dummy) == 0) {
			    TkAboutDlg();
			} else {
			    Tcl_Eval(gInterp, "tkAboutDialog");
			}
			break;
		    }
		default:
		    GetItem(gAppleM, theItem, name);
		    HiliteMenu(0);
		    OpenDeskAcc(name);
		    return;
	    }
	    break;
	case kFileMenu:
	    switch (theItem) {
		case kSourceItem:
		    /* TODO: source script */
		    SourceDialog();
		    break;
		case kCloseItem:
		    /* Send close event */
		    window = TkMacGetXWindow(FrontWindow());
		    tkwin = Tk_IdToWindow(tkDisplayList->display, window);
		    TkGenWMDestroyEvent(tkwin);
		    break;
		case kQuitItem:
		    /* Exit */
		    if (optionKeyPressed || gInterp == NULL) {
			Tcl_Exit(0);
		    } else {
			Tcl_Eval(gInterp, "exit");
		    }
		    break;
	    }
	    break;
	case kEditMenu:
	    /*
	     * This implementation just send keysyms
	     * the Tk thinks are associated with function keys that
	     * do Cut, Copy & Paste on a Sun keyboard.
	     */
	    FakeEditEvent(theItem);
	    break;
	default:
	    /*
	     * This will eventually evoke Tcl scripts
	     * TODO: come up with frame work for this...
	     */
	    break;
    }

    /*
     * Finally we unhighlight the menu.
     */
    HiliteMenu(0);
} /* TkMacHandleMenuSelect */

/*
 *----------------------------------------------------------------------
 *
 * TkMacInitMenus --
 *
 *	This procedure initializes the Macintosh menu bar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
TkMacInitMenus(interp)
    Tcl_Interp 	*interp;
{
    Handle mbarH;

    gInterp = interp;

    /* 
     * At this point, InitMenus() should have already been called. 
     */

    mbarH = (Handle) GetMenuBar();
    if (mbarH == NULL) {
	panic("memory - menu bar");
    }

    gAppleM = NewMenu(256, "\p\024");
    if (gAppleM == NULL) {
	panic("memory - menus");
    }
    InsertMenu(gAppleM, 0);
    AppendMenu(gAppleM, "\pAbout Tcl & TkÉ");
    AppendMenu(gAppleM, "\p(-");
    AddResMenu(gAppleM, 'DRVR');

    gFileM = NewMenu(kFileMenu, "\pFile");
    if (gFileM == NULL) {
	panic("memory - menus");
    }
    InsertMenu(gFileM, 0);
    AppendMenu(gFileM, "\pSourceÉ");
    AppendMenu(gFileM, "\pClose/W");
    AppendMenu(gFileM, "\p(-");
    AppendMenu(gFileM, "\pQuit/Q");

    gEditM = NewMenu(kEditMenu, "\pEdit");
    if (gEditM == NULL) {
	panic("memory - menus");
    }
    InsertMenu(gEditM, 0);
    AppendMenu(gEditM, "\pCut/X");
    AppendMenu(gEditM, "\pCopy/C");
    AppendMenu(gEditM, "\pPaste/V");

    DrawMenuBar();

    DisposeHandle(mbarH);

    return;
}

/*
 *----------------------------------------------------------------------
 *
 * FakeEditEvent --
 *
 *	Takes an edit menu item and simulates thier actions by sending
 *	fake keyboard events to Tk's event queue.  Eventually this
 *	should be replaced by a better mechanism.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May place events of queue.
 *
 *----------------------------------------------------------------------
 */

static void 
FakeEditEvent(flag)
    int flag;
{
    XEvent event;
    WindowRef frontWin;
    Window window;
    Point where;
    Tk_Window tkwin;

    /* 
     * Find the window 
     */
    frontWin = FrontWindow();
    window = TkMacGetXWindow(frontWin);
    tkwin = Tk_IdToWindow(tkDisplayList->display, window);
    if (tkwin == NULL) {
        return;
    }

    event.xany.send_event = false;
    event.xkey.same_screen = true;
    event.xkey.subwindow = None;
    event.xkey.time = GenerateTime();

    GetMouse(&where);
    tkwin = Tk_TopCoordsToWindow(tkwin, where.h, where.v, 
	    &event.xkey.x, &event.xkey.y);
    LocalToGlobal(&where);
    event.xkey.x_root = where.h;
    event.xkey.y_root = where.v;

    event.xany.serial = Tk_Display(tkwin)->request;
    event.xkey.window = Tk_WindowId(tkwin);
    event.xkey.display = Tk_Display(tkwin);
    event.xkey.root = XRootWindow(Tk_Display(tkwin), 0);
    
    switch (flag) {
	case EDIT_CUT:
	    event.xkey.keycode = 0x00020D17;
	    event.xkey.state = ControlMask;
	    event.xany.type = KeyPress;
	    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    event.xany.type = KeyRelease;
	    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    break;
	case EDIT_COPY:
	    /*
	     * Meta doesn't work yet - use Control-i as short term hack
	     */
	    event.xkey.state = ControlMask;
	    event.xkey.keycode = 0x00022209;
	    event.xany.type = KeyPress;
	    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    event.xany.type = KeyRelease;
	    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    break;
	case EDIT_PASTE:
	    event.xkey.keycode = 0x00021019;
	    event.xkey.state = ControlMask;
	    event.xany.type = KeyPress;
	    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    event.xany.type = KeyRelease;
	    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    break;
	case EDIT_CLEAR:
	    /*
	     * TODO: figure out a way to do this.
	     */
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SourceDialog --
 *
 *	Presents a dialog to the user for selecting a Tcl file.  The
 *	selected file will be sourced into the main interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void 
SourceDialog()
{
    StandardFileReply reply;
    OSType fileTypes[1];
    OSErr err;
    short length;
    Handle path;
    
    if (gInterp == NULL) {
	return;
    }
    
    fileTypes[0] = 'TEXT';
    StandardGetFile(NULL, 1, fileTypes, &reply);
    if (reply.sfGood == false) {
	return;
    }
    
    err = FSpGetFullPath(&reply.sfFile, &length, &path);
    if (err == noErr) {
	SetHandleSize(path, length + 1);
	HLock(path);
	((char *) (*path))[length] = '\0';
	Tcl_EvalFile(gInterp, *path);
	HUnlock(path);
	DisposeHandle(path);
    }		   
}
