# tk.tcl --
#
# Initialization script normally executed in the interpreter for each
# Tk-based application.  Arranges class bindings for widgets.
#
# SCCS: @(#) tk.tcl 1.81 96/02/16 10:48:13
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# Insist on running with compatible versions of Tcl and Tk.

package require -exact Tk 4.1
package require -exact Tcl 7.5

# Add Tk's directory to the end of the auto-load search path, if it
# isn't already on the path:

if {[lsearch -exact $auto_path $tk_library] < 0} {
    lappend auto_path $tk_library
}

# Turn off strict Motif look and feel as a default.

set tk_strictMotif 0

# tkScreenChanged --
# This procedure is invoked by the binding mechanism whenever the
# "current" screen is changing.  The procedure does two things.
# First, it uses "upvar" to make global variable "tkPriv" point at an
# array variable that holds state for the current display.  Second,
# it initializes the array if it didn't already exist.
#
# Arguments:
# screen -		The name of the new screen.

proc tkScreenChanged screen {
    set disp [file rootname $screen]
    uplevel #0 upvar #0 tkPriv.$disp tkPriv
    global tkPriv
    if [info exists tkPriv] {
	set tkPriv(screen) $screen
	return
    }
    set tkPriv(afterId) {}
    set tkPriv(buttons) 0
    set tkPriv(buttonWindow) {}
    set tkPriv(dragging) 0
    set tkPriv(focus) {}
    set tkPriv(grab) {}
    set tkPriv(initPos) {}
    set tkPriv(inMenubutton) {}
    set tkPriv(listboxPrev) {}
    set tkPriv(mouseMoved) 0
    set tkPriv(oldGrab) {}
    set tkPriv(popup) {}
    set tkPriv(postedMb) {}
    set tkPriv(pressX) 0
    set tkPriv(pressY) 0
    set tkPriv(screen) $screen
    set tkPriv(selectMode) char
    set tkPriv(window) {}
}

# Do initial setup for tkPriv, so that it is always bound to something
# (otherwise, if someone references it, it may get set to a non-upvar-ed
# value, which will cause trouble later).

tkScreenChanged [winfo screen .]

# ----------------------------------------------------------------------
# Read in files that define all of the class bindings.
# ----------------------------------------------------------------------

if {$tcl_platform(platform) != "macintosh"} {
    source $tk_library/button.tcl
    source $tk_library/entry.tcl
    source $tk_library/listbox.tcl
    source $tk_library/menu.tcl
    source $tk_library/scale.tcl
    source $tk_library/scrlbar.tcl
    source $tk_library/text.tcl
}

# ----------------------------------------------------------------------
# Default bindings for keyboard traversal.
# ----------------------------------------------------------------------

bind all <Tab> {focus [tk_focusNext %W]}
bind all <Shift-Tab> {focus [tk_focusPrev %W]}

# tkCancelRepeat --
# This procedure is invoked to cancel an auto-repeat action described
# by tkPriv(afterId).  It's used by several widgets to auto-scroll
# the widget when the mouse is dragged out of the widget with a
# button pressed.
#
# Arguments:
# None.

proc tkCancelRepeat {} {
    global tkPriv
    after cancel $tkPriv(afterId)
    set tkPriv(afterId) {}
}
