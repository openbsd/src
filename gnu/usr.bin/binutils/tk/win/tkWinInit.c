/* 
 * tkWinInit.c --
 *
 *	This file contains Windows-specific interpreter initialization
 *	functions.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinInit.c 1.13 96/03/18 14:22:29
 */

#include "tkWinInt.h"

/*
 * The following string is the startup script executed in new
 * interpreters.  It looks on disk in several different directories
 * for a script "tk.tcl" that is compatible with this version
 * of Tk.  The tk.tcl script does all of the real work of
 * initialization.
 */

static char *initScript =
"proc init {} {\n\
    global tk_library tk_version tk_patchLevel env\n\
    rename init {}\n\
    set dirs {}\n\
    if [info exists env(TK_LIBRARY)] {\n\
	lappend dirs $env(TK_LIBRARY)\n\
    }\n\
    lappend dirs $tk_library\n\
    lappend dirs [file dirname [info library]]/lib/tk$tk_version\n\
    lappend dirs [file dirname [file dirname [info nameofexecutable]]]/lib/tk$tk_version\n\
    if [string match {*[ab]*} $tk_patchLevel] {\n\
	set lib tk$tk_patchLevel\n\
    } else {\n\
	set lib tk$tk_version\n\
    }\n\
    lappend dirs [file dirname [file dirname [pwd]]]/$lib/library\n\
    lappend dirs [file dirname [pwd]]/library\n\
    foreach i $dirs {\n\
	set tk_library $i\n\
	if ![catch {uplevel #0 source [list $i/tk.tcl]}] {\n\
	    return\n\
	}\n\
    }\n\
    set msg \"Can't find a usable tk.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\"\n\
    append msg \"This probably means that Tk wasn't installed properly.\n\"\n\
    error $msg\n\
}\n\
init";

/*
 *----------------------------------------------------------------------
 *
 * TkPlatformInit --
 *
 *	Performs Windows-specific interpreter initialization related to the
 *      tk_library variable.
 *
 * Results:
 *	A standard Tcl completion code (TCL_OK or TCL_ERROR).  Also
 *	leaves information in interp->result.
 *
 * Side effects:
 *	Sets "tk_library" Tcl variable, runs "tk.tcl" script.
 *
 *----------------------------------------------------------------------
 */

int
TkPlatformInit(interp)
    Tcl_Interp *interp;
{
    char *libDir;

    libDir = Tcl_GetVar(interp, "tk_library", TCL_GLOBAL_ONLY);
    if (libDir == NULL) {
	Tcl_SetVar(interp, "tk_library", ".", TCL_GLOBAL_ONLY);
    }

    return Tcl_Eval(interp, initScript);
}
