/* 
 * tkWinX.c --
 *
 *	This file contains Windows emulation procedures for X routines. 
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinX.c 1.27 96/04/11 17:50:13
 */

#include "tkInt.h"
#include "tkWinInt.h"

/*
 * The following declaration is a special purpose backdoor into the
 * Tcl notifier.  It is used to process events on the Tcl event queue,
 * without reentering the system event queue.
 */

extern void		TclWinFlushEvents _ANSI_ARGS_((void));

/*
 * Declarations of static variables used in this file.
 */

static HINSTANCE appInstance = (HINSTANCE) NULL;
				/* Global application instance handle. */
static Display *winDisplay;	/* Display that represents Windows screen. */
static Tcl_HashTable windowTable;
				/* Table of child windows indexed by handle. */
static char winScreenName[] = ":0";
				/* Default name of windows display. */
static ATOM topLevelAtom, childAtom;
				/* Atoms for the classes registered by Tk. */

/*
 * Forward declarations of procedures used in this file.
 */

static void		DeleteWindow _ANSI_ARGS_((HWND hwnd));
static void 		GetTranslatedKey _ANSI_ARGS_((XKeyEvent *xkey));
static void 		TranslateEvent _ANSI_ARGS_((HWND hwnd, UINT message,
			    WPARAM wParam, LPARAM lParam));

/*
 *----------------------------------------------------------------------
 *
 * TkGetServerInfo --
 *
 *	Given a window, this procedure returns information about
 *	the window server for that window.  This procedure provides
 *	the guts of the "winfo server" command.
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
TkGetServerInfo(interp, tkwin)
    Tcl_Interp *interp;		/* The server information is returned in
				 * this interpreter's result. */
    Tk_Window tkwin;		/* Token for window;  this selects a
				 * particular display and server. */
{
    char buffer[50];
    OSVERSIONINFO info;
    int index = 0;
    static char* os[] = {"Win32", "Win32s", "Win32c"};

    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&info);
    if (info.dwPlatformId == VER_PLATFORM_WIN32s) {
	index = 1;
    } else if (info.dwPlatformId == VER_PLATFORM_WIN32s) {
	index = 2;
    }
    sprintf(buffer, "Windows %d.%d %d ", info.dwMajorVersion,
	    info.dwMinorVersion, info.dwBuildNumber);
    Tcl_AppendResult(interp, buffer, os[index], (char *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinGetTkModule --
 *
 *	This function returns the module handle for the Tk DLL.
 *
 * Results:
 *	Returns the library module handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HMODULE
TkWinGetTkModule()
{
    char libName[13];
    sprintf(libName, "tk%d%d.dll", TK_MAJOR_VERSION, TK_MINOR_VERSION);
    return GetModuleHandle(libName);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinGetAppInstance --
 *
 *	Retrieves the global application instance handle.
 *
 * Results:
 *	Returns the global application instance handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HINSTANCE
TkWinGetAppInstance()
{
    return appInstance;
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinXInit --
 *
 *	Initialize Xlib emulation layer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up various data structures.
 *
 *----------------------------------------------------------------------
 */

void
TkWinXInit(hInstance)
    HINSTANCE hInstance;
{
    WNDCLASS class;
    static initialized = 0;

    if (initialized != 0) {
	return;
    }
    initialized = 1;

    appInstance = hInstance;

    class.style = CS_HREDRAW | CS_VREDRAW;
    class.cbClsExtra = 0;
    class.cbWndExtra = 0;
    class.hInstance = hInstance;
    class.hbrBackground = NULL;
    class.lpszMenuName = NULL;

    /*
     * Register the TopLevel window class.
     */

    class.lpszClassName = TK_WIN_TOPLEVEL_CLASS_NAME;
    class.lpfnWndProc = TkWinTopLevelProc;
    class.hIcon = LoadIcon(TkWinGetTkModule(), "tk");
    class.hCursor = LoadCursor(NULL, IDC_ARROW);

    topLevelAtom = RegisterClass(&class);
    if (topLevelAtom == 0) {
	panic("Unable to register TkTopLevel class");
    }
    
    /*
     * Register the Child window class.
     */

    class.lpszClassName = TK_WIN_CHILD_CLASS_NAME;
    class.lpfnWndProc = TkWinChildProc;
    class.hIcon = NULL;
    class.hCursor = NULL;

    childAtom = RegisterClass(&class);
    if (childAtom == 0) {
	UnregisterClass((LPCTSTR)topLevelAtom, hInstance);
	panic("Unable to register TkChild class");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDefaultScreenName --
 *
 *	Returns the name of the screen that Tk should use during
 *	initialization.
 *
 * Results:
 *	Returns a statically allocated string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TkGetDefaultScreenName(interp, screenName)
    Tcl_Interp *interp;		/* Not used. */
    char *screenName;		/* If NULL, use default string. */
{
    if ((screenName == NULL) || (screenName[0] == '\0')) {
	screenName = winScreenName;
    }
    return screenName;
}

/*
 *----------------------------------------------------------------------
 *
 * XOpenDisplay --
 *
 *	Create the Display structure and fill it with device
 *	specific information.
 *
 * Results:
 *	Returns a Display structure on success or NULL on failure.
 *
 * Side effects:
 *	Allocates a new Display structure.
 *
 *----------------------------------------------------------------------
 */

Display *
XOpenDisplay(display_name)
    _Xconst char *display_name;
{
    Screen *screen;
    HDC dc;
    TkWinDrawable *twdPtr;

    TkWinPointerInit();

    Tcl_InitHashTable(&windowTable, TCL_ONE_WORD_KEYS);

    if (winDisplay != NULL) {
	if (strcmp(winDisplay->display_name, display_name) == 0) {
	    return winDisplay;
	} else {
	    panic("XOpenDisplay: tried to open multiple displays");
	    return NULL;
	}
    }

    winDisplay = (Display *) ckalloc(sizeof(Display));
    winDisplay->display_name = (char *) ckalloc(strlen(display_name)+1);
    strcpy(winDisplay->display_name, display_name);

    winDisplay->cursor_font = 1;
    winDisplay->nscreens = 1;
    winDisplay->request = 1;
    winDisplay->qlen = 0;

    screen = (Screen *) ckalloc(sizeof(Screen));
    screen->display = winDisplay;

    dc = GetDC(NULL);
    screen->width = GetDeviceCaps(dc, HORZRES);
    screen->height = GetDeviceCaps(dc, VERTRES);
    screen->mwidth = GetDeviceCaps(dc, HORZSIZE);
    screen->mheight = GetDeviceCaps(dc, VERTSIZE);

    /*
     * Set up the root window.
     */

    twdPtr = (TkWinDrawable*) ckalloc(sizeof(TkWinDrawable));
    if (twdPtr == NULL) {
	return None;
    }
    twdPtr->type = TWD_WINDOW;
    twdPtr->window.winPtr = NULL;
    twdPtr->window.handle = NULL;
    screen->root = (Window)twdPtr;

    screen->root_depth = GetDeviceCaps(dc, BITSPIXEL);
    screen->root_visual = (Visual *) ckalloc(sizeof(Visual));
    screen->root_visual->visualid = 0;
    if (GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) {
	screen->root_visual->map_entries = GetDeviceCaps(dc, SIZEPALETTE);
	screen->root_visual->class = PseudoColor;
    } else {
	if (screen->root_depth == 4) {
	    screen->root_visual->class = StaticColor;
	    screen->root_visual->map_entries = 16;
	} else if (screen->root_depth == 16) {
	    screen->root_visual->class = TrueColor;
	    screen->root_visual->map_entries = 64;
	    screen->root_visual->red_mask = 0xf8;
	    screen->root_visual->green_mask = 0xfc00;
	    screen->root_visual->blue_mask = 0xf80000;
	} else if (screen->root_depth >= 24) {
	    screen->root_visual->class = TrueColor;
	    screen->root_visual->map_entries = 256;
	    screen->root_visual->red_mask = 0xff;
	    screen->root_visual->green_mask = 0xff00;
	    screen->root_visual->blue_mask = 0xff0000;
	}
    }
    screen->root_visual->bits_per_rgb = screen->root_depth;
    ReleaseDC(NULL, dc);

    /*
     * Note that these pixel values are not palette relative.
     */

    screen->white_pixel = RGB(255, 255, 255);
    screen->black_pixel = RGB(0, 0, 0);

    winDisplay->screens = screen;
    winDisplay->nscreens = 1;
    winDisplay->default_screen = 0;
    screen->cmap = XCreateColormap(winDisplay, None, screen->root_visual,
	    AllocNone);
    return winDisplay;
}

/*
 *----------------------------------------------------------------------
 *
 * XBell --
 *
 *	Generate a beep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Plays a sounds out the system speakers.
 *
 *----------------------------------------------------------------------
 */

void
XBell(display, percent)
    Display* display;
    int percent;
{
    MessageBeep(MB_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinTopLevelProc --
 *
 *	Callback from Windows whenever an event occurs on a top level
 *	window.
 *
 * Results:
 *	Standard Windows return value.
 *
 * Side effects:
 *	Default window behavior.
 *
 *----------------------------------------------------------------------
 */

LRESULT CALLBACK
TkWinTopLevelProc(hwnd, message, wParam, lParam)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
{
    static inMoveSize = 0;
	
    if (inMoveSize) {
	TclWinFlushEvents();
    }

    switch (message) {
	case WM_ENTERSIZEMOVE:
	    inMoveSize = 1;
	    break;

	case WM_EXITSIZEMOVE:
	    inMoveSize = 0;
	    break;

	case WM_CREATE: {
	    CREATESTRUCT *info = (CREATESTRUCT *) lParam;
	    TkWinDrawable *twdPtr = (TkWinDrawable *)info->lpCreateParams;
	    Tcl_HashEntry *hPtr;
	    int new;

	    /*
	     * Add the window and handle to the window table.
	     */

	    twdPtr->window.handle = hwnd;
	    hPtr = Tcl_CreateHashEntry(&windowTable, (char *)hwnd, &new);
	    if (!new) {
		panic("Duplicate window handle: %p", hwnd);
	    }
	    Tcl_SetHashValue(hPtr, twdPtr);

	    /*
	     * Store the pointer to the drawable structure passed into
	     * CreateWindow in the user data slot of the window.
	     */

	    SetWindowLong(hwnd, GWL_USERDATA, (DWORD)twdPtr);
	    return 0;
	}

	case WM_DESTROY:
	    DeleteWindow(hwnd);
	    return 0;
	    
	case WM_GETMINMAXINFO: 
	    TkWinWmSetLimits(hwnd, (MINMAXINFO *) lParam);
	    return 0;

	case WM_PALETTECHANGED:
	    return TkWinWmInstallColormaps(hwnd, WM_PALETTECHANGED,
		    hwnd == (HWND)wParam);

	case WM_QUERYNEWPALETTE:
	    return TkWinWmInstallColormaps(hwnd, WM_QUERYNEWPALETTE, TRUE);

	case WM_WINDOWPOSCHANGED: {
	    WINDOWPOS *pos = (WINDOWPOS *) lParam;
	    TkWinDrawable *twdPtr =
		(TkWinDrawable *) GetWindowLong(hwnd, GWL_USERDATA);

	    TkWinWmConfigure(TkWinGetWinPtr(twdPtr), pos);
	    return 0;
	}

	case WM_CLOSE:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
	    TranslateEvent(hwnd, message, wParam, lParam);
	    return 0;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_DESTROYCLIPBOARD:
	    TranslateEvent(hwnd, message, wParam, lParam);

	    /*
	     * We need to pass these messages to the default window
	     * procedure in order to get the system menu to work.
	     */

	    break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinChildProc --
 *
 *	Callback from Windows whenever an event occurs on a child
 *	window.
 *
 * Results:
 *	Standard Windows return value.
 *
 * Side effects:
 *	Default window behavior.
 *
 *----------------------------------------------------------------------
 */

LRESULT CALLBACK
TkWinChildProc(hwnd, message, wParam, lParam)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
{
    switch (message) {
	case WM_CREATE: {
	    CREATESTRUCT *info = (CREATESTRUCT *) lParam;
	    Tcl_HashEntry *hPtr;
	    int new;

	    /*
	     * Add the window and handle to the window table.
	     */

	    hPtr = Tcl_CreateHashEntry(&windowTable, (char *)hwnd, &new);
	    if (!new) {
		panic("Duplicate window handle: %p", hwnd);
	    }
	    Tcl_SetHashValue(hPtr, info->lpCreateParams);

	    /*
	     * Store the pointer to the drawable structure passed into
	     * CreateWindow in the user data slot of the window.  Then set
	     * the Z stacking order so the window appears on top.
	     */
	    
	    SetWindowLong(hwnd, GWL_USERDATA, (DWORD)info->lpCreateParams);
	    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
		    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	    return 0;
	}

	case WM_DESTROY:
	    DeleteWindow(hwnd);
	    return 0;
	    
	case WM_ERASEBKGND:
	case WM_WINDOWPOSCHANGED:
	    return 0;

	case WM_RENDERFORMAT: {
	    TkWinDrawable *twdPtr;
	    twdPtr = (TkWinDrawable *) GetWindowLong(hwnd, GWL_USERDATA);
	    TkWinClipboardRender(TkWinGetWinPtr(twdPtr), wParam);
	    return 0;
	}

	case WM_DESTROYCLIPBOARD:
	case WM_PAINT:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
	    TranslateEvent(hwnd, message, wParam, lParam);
	    return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

/*
 *----------------------------------------------------------------------
 *
 * TranslateEvent --
 *
 *	This function is called by the window procedures to handle
 *	the translation from Win32 events to Tk events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Queues a new Tk event.
 *
 *----------------------------------------------------------------------
 */

static void
TranslateEvent(hwnd, message, wParam, lParam)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
{
    TkWindow *winPtr;
    XEvent event;
    TkWinDrawable *twdPtr;

    /*
     * Retrieve the window information, and reset the hwnd pointer in
     * case the original window was a toplevel decorative frame.
     */

    twdPtr = (TkWinDrawable *) GetWindowLong(hwnd, GWL_USERDATA);
    if (twdPtr == NULL) {
	return;
    }
    winPtr = TkWinGetWinPtr(twdPtr);

    /*
     * TranslateEvent may get called even after Tk has deleted the window.
     * So we must check for a dead window before proceeding.
     */

    if (winPtr == NULL || winPtr->window == None) {
	return;
    }

    hwnd = TkWinGetHWND(winPtr->window);

    event.xany.serial = winPtr->display->request++;
    event.xany.send_event = False;
    event.xany.display = winPtr->display;
    event.xany.window = (Window) winPtr->window;

    switch (message) {
	case WM_PAINT: {
	    PAINTSTRUCT ps;

	    event.type = Expose;
	    BeginPaint(hwnd, &ps);
	    event.xexpose.x = ps.rcPaint.left;
	    event.xexpose.y = ps.rcPaint.top;
	    event.xexpose.width = ps.rcPaint.right - ps.rcPaint.left;
	    event.xexpose.height = ps.rcPaint.bottom - ps.rcPaint.top;
	    EndPaint(hwnd, &ps);
	    event.xexpose.count = 0;
	    break;
	}

	case WM_CLOSE:
	    event.type = ClientMessage;
	    event.xclient.message_type =
		Tk_InternAtom((Tk_Window) winPtr, "WM_PROTOCOLS");
	    event.xclient.format = 32;
	    event.xclient.data.l[0] =
		Tk_InternAtom((Tk_Window) winPtr, "WM_DELETE_WINDOW");
	    break;

	case WM_SETFOCUS:
	    event.type = FocusIn;
	    event.xfocus.mode = NotifyNormal;
	    event.xfocus.detail = NotifyAncestor;
	    break;

	case WM_KILLFOCUS:
	    event.type = FocusOut;
	    event.xfocus.mode = NotifyNormal;
	    event.xfocus.detail = NotifyAncestor;
	    break;
	    
	case WM_DESTROYCLIPBOARD:
	    event.type = SelectionClear;
	    event.xselectionclear.selection =
		Tk_InternAtom((Tk_Window)winPtr, "CLIPBOARD");
	    event.xselectionclear.time = GetCurrentTime();
	    break;
	    
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
	    unsigned int state = TkWinGetModifierState(message,
		    wParam, lParam);
	    Time time = GetCurrentTime();
	    POINT clientPoint;
	    POINTS rootPoint;	/* Note: POINT and POINTS are different */
	    DWORD msgPos;

	    /*
	     * Compute the screen and window coordinates of the event.
	     */
	    
	    msgPos = GetMessagePos();
	    rootPoint = MAKEPOINTS(msgPos);
	    clientPoint.x = rootPoint.x;
	    clientPoint.y = rootPoint.y;
	    ScreenToClient(hwnd, &clientPoint);

	    /*
	     * Set up the common event fields.
	     */

	    event.xbutton.root = RootWindow(winPtr->display,
		    winPtr->screenNum);
	    event.xbutton.subwindow = None;
	    event.xbutton.x = clientPoint.x;
	    event.xbutton.y = clientPoint.y;
	    event.xbutton.x_root = rootPoint.x;
	    event.xbutton.y_root = rootPoint.y;
	    event.xbutton.state = state;
	    event.xbutton.time = time;
	    event.xbutton.same_screen = True;

	    /*
	     * Now set up event specific fields.
	     */

	    switch (message) {
		case WM_LBUTTONDOWN:
		    event.type = ButtonPress;
		    event.xbutton.button = Button1;
		    break;

		case WM_MBUTTONDOWN:
		    event.type = ButtonPress;
		    event.xbutton.button = Button2;
		    break;

		case WM_RBUTTONDOWN:
		    event.type = ButtonPress;
		    event.xbutton.button = Button3;
		    break;
	
		case WM_LBUTTONUP:
		    event.type = ButtonRelease;
		    event.xbutton.button = Button1;
		    break;
	
		case WM_MBUTTONUP:
		    event.type = ButtonRelease;
		    event.xbutton.button = Button2;
		    break;

		case WM_RBUTTONUP:
		    event.type = ButtonRelease;
		    event.xbutton.button = Button3;
		    break;
	
		case WM_MOUSEMOVE:
		    event.type = MotionNotify;
		    event.xmotion.is_hint = NotifyNormal;
		    break;

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		    /*
		     * Check for translated characters in the event queue.
		     */

		    event.type = KeyPress;
		    event.xkey.keycode = wParam;
		    GetTranslatedKey(&event.xkey);
		    break;

		case WM_SYSKEYUP:
		case WM_KEYUP:
		    /*
		     * We don't check for translated characters on keyup
		     * because Tk won't know what to do with them.  Instead, we
		     * wait for the WM_CHAR messages which will follow.
		     */
		    event.type = KeyRelease;
		    event.xkey.keycode = wParam;
		    event.xkey.nchars = 0;
		    break;

		case WM_CHAR:
		case WM_SYSCHAR:
		    /*
		     * Synthesize both a KeyPress and a KeyRelease.
		     */

		    event.type = KeyPress;
		    event.xkey.keycode = 0;
		    event.xkey.nchars = 1;
		    event.xkey.trans_chars[0] = (char) wParam;
		    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
		    event.type = KeyRelease;
		    break;
	    }

	    if ((event.type == MotionNotify)
		    || (event.type == ButtonPress)
		    || (event.type == ButtonRelease)) {
		TkWinPointerEvent(&event, winPtr);
		return;
	    }
	    break;
	}

	default:
	    return;
    }
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinGetModifierState --
 *
 *	This function constructs a state mask for the mouse buttons 
 *	and modifier keys.
 *
 * Results:
 *	Returns a composite value of all the modifier and button state
 *	flags that were set at the time the event occurred.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned int
TkWinGetModifierState(message, wParam, lParam)
    UINT message;		/* Win32 message type */
    WPARAM wParam;		/* wParam of message, used if key message */
    LPARAM lParam;		/* lParam of message, used if key message */
{
    unsigned int state = 0;	/* accumulated state flags */
    int isKeyEvent;		/* 1 if message is a key press or release */
    int prevState;		/* 1 if key was previously down */

    /*
     * If the event is a key press or release, we check for autorepeat.
     */

    if (message == WM_SYSKEYDOWN || message == WM_KEYDOWN
	    || message == WM_SYSKEYUP || message == WM_KEYUP) {
	isKeyEvent = TRUE;
	prevState = HIWORD(lParam) & KF_REPEAT;
    }

    /*
     * If the key being pressed or released is a modifier key, then
     * we use its previous state, otherwise we look at the current state.
     */

    if (isKeyEvent && (wParam == VK_SHIFT)) {
	state |= prevState ? ShiftMask : 0;
    } else {
	state |= (GetKeyState(VK_SHIFT) & 0x8000) ? ShiftMask : 0;
    }
    if (isKeyEvent && (wParam == VK_CONTROL)) {
	state |= prevState ? ControlMask : 0;
    } else {
	state |= (GetKeyState(VK_CONTROL) & 0x8000) ? ControlMask : 0;
    }
    if (isKeyEvent && (wParam == VK_MENU)) {
	state |= prevState ? Mod2Mask : 0;
    } else {
	state |= (GetKeyState(VK_MENU) & 0x8000) ? Mod2Mask : 0;
    }

    /*
     * For toggle keys, we have to check both the previous key state
     * and the current toggle state.  The result is the state of the
     * toggle before the event.
     */

    if ((wParam == VK_CAPITAL)
	    && (message == WM_SYSKEYDOWN || message == WM_KEYDOWN)) {
	state = (prevState ^ (GetKeyState(VK_CAPITAL) & 0x0001))
	    ? 0 : LockMask;
    } else {
	state |= (GetKeyState(VK_CAPITAL) & 0x0001) ? LockMask : 0;
    }
    if ((wParam == VK_NUMLOCK)
	    && (message == WM_SYSKEYDOWN || message == WM_KEYDOWN)) {
	state = (prevState ^ (GetKeyState(VK_NUMLOCK) & 0x0001))
	    ? 0 : Mod1Mask;
    } else {
	state |= (GetKeyState(VK_NUMLOCK) & 0x0001) ? Mod1Mask : 0;
    }
    if ((wParam == VK_SCROLL)
	    && (message == WM_SYSKEYDOWN || message == WM_KEYDOWN)) {
	state = (prevState ^ (GetKeyState(VK_SCROLL) & 0x0001))
	    ? 0 : Mod3Mask;
    } else {
	state |= (GetKeyState(VK_SCROLL) & 0x0001) ? Mod3Mask : 0;
    }

    /*
     * If a mouse button is being pressed or released, we use the previous
     * state of the button.
     */

    if (message == WM_LBUTTONUP || (message != WM_LBUTTONDOWN
	    && GetKeyState(VK_LBUTTON) & 0x8000)) {
	state |= Button1Mask;
    }
    if (message == WM_MBUTTONUP || (message != WM_MBUTTONDOWN
	    && GetKeyState(VK_MBUTTON) & 0x8000)) {
	state |= Button2Mask;
    }
    if (message == WM_RBUTTONUP || (message != WM_RBUTTONDOWN
	    && GetKeyState(VK_RBUTTON) & 0x8000)) {
	state |= Button3Mask;
    }
    return state;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTranslatedKey --
 *
 *	Retrieves WM_CHAR messages that are placed on the system queue
 *	by the TranslateMessage system call and places them in the
 *	given KeyPress event.
 *
 * Results:
 *	Sets the trans_chars and nchars member of the key event.
 *
 * Side effects:
 *	Removes any WM_CHAR messages waiting on the top of the system
 *	event queue.
 *
 *----------------------------------------------------------------------
 */

static void
GetTranslatedKey(xkey)
    XKeyEvent *xkey;
{
    MSG msg;
    
    xkey->nchars = 0;

    while (xkey->nchars < XMaxTransChars
	    && PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
	if ((msg.message == WM_CHAR) || (msg.message == WM_SYSCHAR)) {
	    xkey->trans_chars[xkey->nchars] = (char) msg.wParam;
	    xkey->nchars++;
	    GetMessage(&msg, NULL, 0, 0);
	    if ((msg.message == WM_CHAR) && (msg.lParam & 0x20000000)) {
		xkey->state = 0;
	    }
	} else {
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinGetDrawableFromHandle --
 *
 *	Find the drawable associated with the given window handle.
 *
 * Results:
 *	Returns a drawable pointer if the window is managed by Tk.
 *	Otherwise it returns NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWinDrawable *
TkWinGetDrawableFromHandle(hwnd)
    HWND hwnd;			/* Win32 window handle */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&windowTable, (char *)hwnd);
    if (hPtr) {
	return (TkWinDrawable *)Tcl_GetHashValue(hPtr);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteWindow --
 *
 *	Remove a window from the window table, and free the resources
 *	associated with the drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the resources associated with a window handle.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteWindow(hwnd)
    HWND hwnd;
{
    TkWinDrawable *twdPtr;
    Tcl_HashEntry *hPtr;

    /*
     * Remove the window from the window table.
     */

    hPtr = Tcl_FindHashEntry(&windowTable, (char *)hwnd);
    if (hPtr) {
	Tcl_DeleteHashEntry(hPtr);
    }

    /*
     * Free the drawable associated with this window, unless the drawable
     * is still in use by a TkWindow.  This only happens in the case of
     * a top level window, since the window gets destroyed when the
     * decorative frame is destroyed.
     */

    twdPtr = (TkWinDrawable *) GetWindowLong(hwnd, GWL_USERDATA);
    if (twdPtr) {
	if (twdPtr->window.winPtr == NULL) {
	    ckfree((char *) twdPtr);
	} else if (!(twdPtr->window.winPtr->flags & TK_TOP_LEVEL)) {
	    panic("Non-toplevel window destroyed before its drawable");
	} else {
	    twdPtr->window.handle = NULL;
	}
    }
}
