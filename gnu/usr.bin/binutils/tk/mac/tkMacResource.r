/*
 * tkMacResources.r --
 *
 *	This file creates resources for use in a simple shell.
 *	This is designed to be an example of using the Tcl/Tk 
 *	libraries in a Macintosh Application.
 *
 * Copyright (c) 1993-1994 Lockheed Missle & Space Company, AI Center
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacResource.r 1.16 96/04/10 14:28:11
 */

/* 
 * Resources for a Simple Tcl/Tk Shell 
 */

#include <Types.r>
#include <SysTypes.r>
#include <AEUserTermTypes.r>

#ifdef applec
#	define	__kPrefSize 384
#	define	__kMinSize 256	
#	include "siow.r"
#endif

#define RELEASE_CODE 0x00

resource 'vers' (1) {
	0x04, 0x01, final,
	RELEASE_CODE, 0,
	"4.1.0",
	"4.1.0" ", by Ray Johnson © 1993-1996" "\n" "Sun Microsystems Labratories"
};

resource 'vers' (2) {
	0x04, 0x01, final,
	RELEASE_CODE, 0,
	"4.1.0",
	"Wish 4.1 © 1993-1996"
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

read 'TEXT' (0, "Init", purgeable, preload) ":::tcl7.5:library:init.tcl";
read 'TEXT' (1, "tk", purgeable, preload) "::library:tk.tcl";
read 'TEXT' (2, "button", purgeable, preload) "::library:button.tcl";
read 'TEXT' (3, "dialog", purgeable, preload) "::library:dialog.tcl";
read 'TEXT' (4, "entry", purgeable, preload) "::library:entry.tcl";
read 'TEXT' (5, "focus", purgeable, preload) "::library:focus.tcl";
read 'TEXT' (6, "listbox", purgeable, preload) "::library:listbox.tcl";
read 'TEXT' (7, "menu", purgeable, preload) "::library:menu.tcl";
read 'TEXT' (8, "optionMenu", purgeable, preload) "::library:optMenu.tcl";
read 'TEXT' (9, "palette", purgeable, preload) "::library:palette.tcl";
read 'TEXT' (10, "scale", purgeable, preload) "::library:scale.tcl";
read 'TEXT' (11, "scrollbar", purgeable, preload) "::library:scrlbar.tcl";
read 'TEXT' (12, "tearoff", purgeable, preload) "::library:tearoff.tcl";
read 'TEXT' (13, "text", purgeable, preload) "::library:text.tcl";
read 'TEXT' (14, "tkerror", purgeable, preload) "::library:bgerror.tcl";
read 'TEXT' (15, "Console", purgeable, preload) "::library:console.tcl";

/*
 * The following resource is used when creating the 'env' variable in
 * the Macintosh environment.  The creation mechanisim looks for the
 * 'STR#' resource named "Tcl Environment Variables" rather than a
 * specific resource number.  (In other words, feel free to change the
 * resource id if it conflicts with your application.)  Each string in
 * the resource must be of the form "KEYWORD=SOME STRING".  See Tcl
 * documentation for futher information about the env variable.
 */
 
/* A good example of something you may want to set is:
 * "TCL_LIBRARY=My disk:etc." 
 */
		
resource 'STR#' (128, "Tcl Environment Variables") {
	{	"SCHEDULE_NAME=Agent Controller Schedule",
		"SCHEDULE_PATH=Lozoya:System Folder:Tcl Lib:Tcl-Scheduler"
	};
};

/*
 * The following two resources define the default "About Box" for Mac Tk.
 * This dialog appears if the "About Tk..." menu item is selected from
 * the Apple menu.  This dialog may be overridden by defining a Tcl procedure
 * with the name of "tkAboutDialog".  If this procedure is defined the
 * default dialog will not be shown and the Tcl procedure is expected to
 * create and manage an About Dialog box.
 */
 
data 'DLOG' (128, "Default About Box", purgeable) {
	$"0028 0028 00C4 0113 0001 0100 0100 0000"
	$"0000 0081 0000 280A"
};

data 'DITL' (129, "About Box") {
	$"0001 0000 0000 0080 005C 0094 0096 0402"
	$"4F4B 0000 0000 000C 000E 0073 00E9 886B"
	$"5769 7368 202D 2057 696E 646F 7769 6E67"
	$"2053 6865 6C6C 0D62 6173 6564 206F 6E20"
	$"5463 6C20 372E 3520 2620 546B 2034 2E31"
	$"0D0D 5261 7920 4A6F 686E 736F 6E0D 5375"
	$"6E20 4D69 6372 6F73 7973 7465 6D73 204C"
	$"6162 730D 7261 792E 6A6F 686E 736F 6E40"
	$"656E 672E 7375 6E2E 636F 6D00"
};


/*
 * The following resources defines the Apple Events that Tk can be
 * sent from Apple Script.
 */

resource 'aete' (0, "Wish Suite") {
    0x01, 0x00, english, roman,
    {
	"Required Suite", 
	"Events that every application should support", 
	'reqd', 1, 1,
	{},
	{},
	{},
	{},

	"Wish Suite", "Events for the Wish application", 'WIsH', 1, 1,
	{
	    "do script", "Execute a Tcl script", 'misc', 'dosc',
	    'TEXT', "Result", replyOptional, singleItem,
	    notEnumerated, reserved, reserved, reserved, reserved,
	    reserved, reserved, reserved, reserved, reserved,
	    reserved, reserved, reserved, reserved, 
	    'TEXT', "Script to execute", directParamRequired,
	    singleItem, notEnumerated, changesState, reserved,
	    reserved, reserved, reserved, reserved, reserved,
	    reserved, reserved, reserved, reserved, reserved,
	    reserved, 
	    {},
	},
	{},
	{},
	{},
    }
};
