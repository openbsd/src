/* 
 * tclMacResource.r --
 *
 *	This file creates resources for use in a simple shell.
 *	This is designed to be an example of using the Tcl libraries
 *	in a Macintosh Application.
 *
 * Copyright (c) 1993-94 Lockheed Missle & Space Company
 * Copyright (c) 1994-96 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacResource.r 1.10 96/04/10 14:25:48
 */

/* 
 * Resources definitions for a Simple Tcl Shell 
 */

#include <Types.r>
#include <SysTypes.r>

#ifdef applec
#	define	__kPrefSize 384
#	define	__kMinSize 256	
#	include "siow.r"
#endif

#define RELEASE_CODE 0x00

resource 'vers' (1) {
	0x07, 0x50, final,
	RELEASE_CODE, 0,
	"7.5.0",
	"7.5.0" ", by Ray Johnson ©Sun Microsystems"
};

resource 'vers' (2) {
	0x07, 0x50, final,
	RELEASE_CODE, 0,
	"7.5.0",
	"Simple Tcl Shell 7.5 © 1996"
};


/* 
 * The mechanisim below loads Tcl source into the resource fork of the
 * application.  The example below creates a TEXT resource named
 * "Init" from the file "init.tcl".  This allows applications to use
 * Tcl to define the behavior of the application without having to
 * require some predetermined file structure - all needed Tcl "files"
 * are located within the application.  To source a file for the
 * resource fork the source command has been modified to support
 * sourcing from resources.  In the below case "source -rsrc {Init}"
 * will load the TEXT resource named "Init".
 */
read 'TEXT' (0, "Init", purgeable, preload) "::library:init.tcl";

/*
 * The following resource is used when creating the 'env' variable in
 * the Macintosh environment.  The creation mechanisim looks for the
 * 'STR#' resource named "Tcl Environment Variables" rather than a
 * specific resource number.  (In other words, feel free to change the
 * resource id if it conflicts with your application.)  Each string in
 * the resource must be of the form "KEYWORD=SOME STRING".  See Tcl
 * documentation for futher information about the env variable.
 *
 * A good example of something you may want to set is: "TCL_LIBRARY=My
 * disk:etc."
 */
 
resource 'STR#' (128, "Tcl Environment Variables") {
	{	"SCHEDULE_NAME=Agent Controller Schedule",
		"SCHEDULE_PATH=Lozoya:System Folder:Tcl Lib:Tcl-Scheduler"
	};
};

