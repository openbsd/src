# bgerror.tcl --
#
# This file contains a default version of the bgerror procedure.  It
# posts a dialog box with the error message and gives the user a chance
# to see a more detailed stack trace.
#
# SCCS: @(#) bgerror.tcl 1.8 96/02/16 10:48:14
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# bgerror --
# This is the default version of bgerror.  It posts a dialog box containing
# the error message and gives the user a chance to ask to see a stack
# trace.
# Arguments:
# err -			The error message.

proc bgerror err {
    global errorInfo
    set info $errorInfo
    set button [tk_dialog .bgerrorDialog "Error in Tcl Script" \
	    "Error: $err" error 0 OK "Skip Messages" "Stack Trace"]
    if {$button == 0} {
	return
    } elseif {$button == 1} {
	return -code break
    }

    set w .bgerrorTrace
    catch {destroy $w}
    toplevel $w -class ErrorTrace
    wm minsize $w 1 1
    wm title $w "Stack Trace for Error"
    wm iconname $w "Stack Trace"
    button $w.ok -text OK -command "destroy $w"
    text $w.text -relief sunken -bd 2 -yscrollcommand "$w.scroll set" \
	    -setgrid true -width 60 -height 20
    scrollbar $w.scroll -relief sunken -command "$w.text yview"
    pack $w.ok -side bottom -padx 3m -pady 2m
    pack $w.scroll -side right -fill y
    pack $w.text -side left -expand yes -fill both
    $w.text insert 0.0 $info
    $w.text mark set insert 0.0

    # Center the window on the screen.

    wm withdraw $w
    update idletasks
    set x [expr [winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
	    - [winfo vrootx [winfo parent $w]]]
    set y [expr [winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
	    - [winfo vrooty [winfo parent $w]]]
    wm geom $w +$x+$y
    wm deiconify $w

    # Be sure to release any grabs that might be present on the
    # screen, since they could make it impossible for the user
    # to interact with the stack trace.

    if {[grab current .] != ""} {
	grab release [grab current .]
    }
}
