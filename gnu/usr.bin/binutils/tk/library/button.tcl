# button.tcl --
#
# This file defines the default bindings for Tk label, button,
# checkbutton, and radiobutton widgets and provides procedures
# that help in implementing those bindings.
#
# SCCS: @(#) button.tcl 1.19 96/02/20 13:01:32
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# The code below creates the default class bindings for buttons.
#-------------------------------------------------------------------------

bind Button <FocusIn> {}
bind Button <Enter> {
    tkButtonEnter %W
}
bind Button <Leave> {
    tkButtonLeave %W
}
bind Button <1> {
    tkButtonDown %W
}
bind Button <ButtonRelease-1> {
    tkButtonUp %W
}
bind Button <space> {
    tkButtonInvoke %W
}

bind Checkbutton <FocusIn> {}
bind Checkbutton <Enter> {
    tkButtonEnter %W
}
bind Checkbutton <Leave> {
    tkButtonLeave %W
}
bind Checkbutton <1> {
    tkCheckRadioInvoke %W
}
bind Checkbutton <space> {
    tkCheckRadioInvoke %W
}
bind Checkbutton <Return> {
    if !$tk_strictMotif {
	tkCheckRadioInvoke %W
    }
}

bind Radiobutton <FocusIn> {}
bind Radiobutton <Enter> {
    tkButtonEnter %W
}
bind Radiobutton <Leave> {
    tkButtonLeave %W
}
bind Radiobutton <1> {
    tkCheckRadioInvoke %W
}
bind Radiobutton <space> {
    tkCheckRadioInvoke %W
}
bind Radiobutton <Return> {
    if !$tk_strictMotif {
	tkCheckRadioInvoke %W
    }
}

# tkButtonEnter --
# The procedure below is invoked when the mouse pointer enters a
# button widget.  It records the button we're in and changes the
# state of the button to active unless the button is disabled.
#
# Arguments:
# w -		The name of the widget.

proc tkButtonEnter {w} {
    global tkPriv
    if {[$w cget -state] != "disabled"} {
	$w config -state active
	if {$tkPriv(buttonWindow) == $w} {
	    $w configure -state active -relief sunken
	}
    }
    set tkPriv(window) $w
}

# tkButtonLeave --
# The procedure below is invoked when the mouse pointer leaves a
# button widget.  It changes the state of the button back to
# inactive.  If we're leaving the button window with a mouse button
# pressed (tkPriv(buttonWindow) == $w), restore the relief of the
# button too.
#
# Arguments:
# w -		The name of the widget.

proc tkButtonLeave w {
    global tkPriv
    if {[$w cget -state] != "disabled"} {
	$w config -state normal
    }
    if {$w == $tkPriv(buttonWindow)} {
	$w configure -relief $tkPriv(relief)
    }
    set tkPriv(window) ""
}

# tkButtonDown --
# The procedure below is invoked when the mouse button is pressed in
# a button widget.  It records the fact that the mouse is in the button,
# saves the button's relief so it can be restored later, and changes
# the relief to sunken.
#
# Arguments:
# w -		The name of the widget.

proc tkButtonDown w {
    global tkPriv
    set tkPriv(relief) [lindex [$w config -relief] 4]
    if {[$w cget -state] != "disabled"} {
	set tkPriv(buttonWindow) $w
	$w config -relief sunken
    }
}

# tkButtonUp --
# The procedure below is invoked when the mouse button is released
# in a button widget.  It restores the button's relief and invokes
# the command as long as the mouse hasn't left the button.
#
# Arguments:
# w -		The name of the widget.

proc tkButtonUp w {
    global tkPriv
    if {$w == $tkPriv(buttonWindow)} {
	set tkPriv(buttonWindow) ""
	$w config -relief $tkPriv(relief)
	if {($w == $tkPriv(window))
		&& ([$w cget -state] != "disabled")} {
	    uplevel #0 [list $w invoke]
	}
    }
}

# tkButtonInvoke --
# The procedure below is called when a button is invoked through
# the keyboard.  It simulate a press of the button via the mouse.
#
# Arguments:
# w -		The name of the widget.

proc tkButtonInvoke w {
    if {[$w cget -state] != "disabled"} {
	set oldRelief [$w cget -relief]
	set oldState [$w cget -state]
	$w configure -state active -relief sunken
	update idletasks
	after 100
	$w configure -state $oldState -relief $oldRelief
	uplevel #0 [list $w invoke]
    }
}

# tkCheckRadioInvoke --
# The procedure below is invoked when the mouse button is pressed in
# a checkbutton or radiobutton widget, or when the widget is invoked
# through the keyboard.  It invokes the widget if it
# isn't disabled.
#
# Arguments:
# w -		The name of the widget.

proc tkCheckRadioInvoke w {
    if {[$w cget -state] != "disabled"} {
	uplevel #0 [list $w invoke]
    }
}
