/* 
 * tkMacInit.c --
 *
 *	This file contains Mac-specific interpreter initialization
 *	functions.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacInit.c 1.19 96/04/05 10:30:43
 */

#include <Files.h>
#include <TextUtils.h>
#include <Strings.h>
#include "tkInt.h"
#include <tclInt.h>
#include "tkMacInt.h"
#include "tclMacInt.h"

/*
 *----------------------------------------------------------------------
 *
 * TkPlatformInit --
 *
 *	Performs Mac-specific interpreter initialization related to the
 *      tk_library variable.
 *
 * Results:
 *	A standard Tcl completion code (TCL_OK or TCL_ERROR).  Also
 *	leaves information in interp->result.
 *
 * Side effects:
 *	Sets "tk_library" Tcl variable, runs initialization scripts
 *	for Tk.
 *
 *----------------------------------------------------------------------
 */

int
TkPlatformInit(interp)
    Tcl_Interp *interp;
{
    char *libDir, *tempPath;
    Tcl_DString path;
    FSSpec macDir;
    int result;
    static char initResCmd[] =
    	"source -rsrc tk\n\
    	source -rsrc button\n\
    	source -rsrc entry\n\
    	source -rsrc listbox\n\
    	source -rsrc menu\n\
    	source -rsrc scale\n\
    	source -rsrc scrollbar\n\
    	source -rsrc text\n\
    	source -rsrc dialog\n\
    	source -rsrc focus\n\
    	source -rsrc optionMenu\n\
    	source -rsrc palette\n\
    	source -rsrc tearoff\n\
    	source -rsrc tkerror\n\
    	";
    static char initCmd[] =
	"if [file exists $tk_library/tk.tcl] {\n\
	    source $tk_library/tk.tcl\n\
	    source $tk_library/button.tcl\n\
	    source $tk_library/entry.tcl\n\
	    source $tk_library/listbox.tcl\n\
	    source $tk_library/menu.tcl\n\
	    source $tk_library/scale.tcl\n\
	    source $tk_library/scrlbar.tcl\n\
	    source $tk_library/text.tcl\n\
	} else {\n\
	    set msg \"can't find tk resource or $tk_library/tk.tcl;\"\n\
	    append msg \" perhaps you need to\\ninstall Tk or set your \"\n\
	    append msg \"TK_LIBRARY environment variable?\"\n\
	    error $msg\n\
	}";

    Tcl_DStringInit(&path);

    /*
     * The tk_library path can be found in several places.  Here is the order
     * in which the are searched.
     *		1) the variable may already exist
     *		2) env array
     *		3) System Folder:Extensions:Tool Command Language:
     *		4) use TK_LIBRARY - which probably won't work
     */
     
    libDir = Tcl_GetVar(interp, "tk_library", TCL_GLOBAL_ONLY);
    if (libDir == NULL) {
	libDir = Tcl_GetVar2(interp, "env", "TK_LIBRARY", TCL_GLOBAL_ONLY);
    }
    if (libDir == NULL) {
	tempPath = Tcl_GetVar2(interp, "env", "EXT_FOLDER", TCL_GLOBAL_ONLY);
	if (tempPath != NULL) {
	    Tcl_DString libPath;
	    
	    Tcl_JoinPath(1, &tempPath, &path);
	    
	    Tcl_DStringInit(&libPath);
	    Tcl_DStringAppend(&libPath, ":Tool Command Language:lib:tk", -1);
	    Tcl_DStringAppend(&libPath, TK_VERSION, -1);
	    Tcl_JoinPath(1, &libPath.string, &path);
	    Tcl_DStringFree(&libPath);
	    
	    if (FSpLocationFromPath(path.length, path.string, &macDir) == noErr) {
	    	libDir = path.string;
	    }
	}
    }
    if (libDir == NULL) {
	libDir = TK_LIBRARY;
    }

    /*
     * Assign path to the global Tcl variable tcl_library.
     */
    Tcl_SetVar(interp, "tk_library", libDir, TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&path);

    result = Tcl_Eval(interp, initResCmd);
    if (result != TCL_OK) {
	result = Tcl_Eval(interp, initCmd);
    }
    return result;
}
