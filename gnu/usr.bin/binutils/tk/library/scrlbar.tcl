# scrlbar.tcl --
#
# This file defines the default bindings for Tk scrollbar widgets.
# It also provides procedures that help in implementing the bindings.
#
# SCCS: @(#) scrlbar.tcl 1.23 96/04/16 11:42:23
#
# Copyright (c) 1994 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# The code below creates the default class bindings for scrollbars.
#-------------------------------------------------------------------------

# Standard Motif bindings:

bind Scrollbar <Enter> {
    if $tk_strictMotif {
	set tkPriv(activeBg) [%W cget -activebackground]
	%W config -activebackground [%W cget -background]
    }
    %W activate [%W identify %x %y]
}
bind Scrollbar <Motion> {
    %W activate [%W identify %x %y]
}
bind Scrollbar <Leave> {
    if $tk_strictMotif {
	%W config -activebackground $tkPriv(activeBg)
    }
    %W activate {}
}
bind Scrollbar <1> {
    tkScrollButtonDown %W %x %y
}
bind Scrollbar <B1-Motion> {
    tkScrollDrag %W %x %y
}
bind Scrollbar <B1-B2-Motion> {
    tkScrollDrag %W %x %y
}
bind Scrollbar <ButtonRelease-1> {
    tkScrollButtonUp %W %x %y
}
bind Scrollbar <B1-Leave> {
    # Prevents <Leave> binding from being invoked.
}
bind Scrollbar <B1-Enter> {
    # Prevents <Enter> binding from being invoked.
}
bind Scrollbar <2> {
    tkScrollButton2Down %W %x %y
}
bind Scrollbar <B1-2> {
    # Do nothing, since button 1 is already down.
}
bind Scrollbar <B2-1> {
    # Do nothing, since button 2 is already down.
}
bind Scrollbar <B2-Motion> {
    tkScrollDrag %W %x %y
}
bind Scrollbar <ButtonRelease-2> {
    tkScrollButtonUp %W %x %y
}
bind Scrollbar <B1-ButtonRelease-2> {
    # Do nothing:  B1 release will handle it.
}
bind Scrollbar <B2-ButtonRelease-1> {
    # Do nothing:  B2 release will handle it.
}
bind Scrollbar <B2-Leave> {
    # Prevents <Leave> binding from being invoked.
}
bind Scrollbar <B2-Enter> {
    # Prevents <Enter> binding from being invoked.
}
bind Scrollbar <Control-1> {
    tkScrollTopBottom %W %x %y
}
bind Scrollbar <Control-2> {
    tkScrollTopBottom %W %x %y
}

bind Scrollbar <Up> {
    tkScrollByUnits %W v -1
}
bind Scrollbar <Down> {
    tkScrollByUnits %W v 1
}
bind Scrollbar <Control-Up> {
    tkScrollByPages %W v -1
}
bind Scrollbar <Control-Down> {
    tkScrollByPages %W v 1
}
bind Scrollbar <Left> {
    tkScrollByUnits %W h -1
}
bind Scrollbar <Right> {
    tkScrollByUnits %W h 1
}
bind Scrollbar <Control-Left> {
    tkScrollByPages %W h -1
}
bind Scrollbar <Control-Right> {
    tkScrollByPages %W h 1
}
bind Scrollbar <Prior> {
    tkScrollByPages %W hv -1
}
bind Scrollbar <Next> {
    tkScrollByPages %W hv 1
}
bind Scrollbar <Home> {
    tkScrollToPos %W 0
}
bind Scrollbar <End> {
    tkScrollToPos %W 1
}

# tkScrollButtonDown --
# This procedure is invoked when a button is pressed in a scrollbar.
# It changes the way the scrollbar is displayed and takes actions
# depending on where the mouse is.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	Mouse coordinates.

proc tkScrollButtonDown {w x y} {
    global tkPriv
    set tkPriv(relief) [$w cget -activerelief]
    $w configure -activerelief sunken
    set element [$w identify $x $y]
    if {$element == "slider"} {
	tkScrollStartDrag $w $x $y
    } else {
	tkScrollSelect $w $element initial
    }
}

# tkScrollButtonUp --
# This procedure is invoked when a button is released in a scrollbar.
# It cancels scans and auto-repeats that were in progress, and restores
# the way the active element is displayed.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	Mouse coordinates.

proc tkScrollButtonUp {w x y} {
    global tkPriv
    tkCancelRepeat
    $w configure -activerelief $tkPriv(relief)
    tkScrollEndDrag $w $x $y
    $w activate [$w identify $x $y]
}

# tkScrollSelect --
# This procedure is invoked when a button is pressed over the scrollbar.
# It invokes one of several scrolling actions depending on where in
# the scrollbar the button was pressed.
#
# Arguments:
# w -		The scrollbar widget.
# element -	The element of the scrollbar that was selected, such
#		as "arrow1" or "trough2".  Shouldn't be "slider".
# repeat -	Whether and how to auto-repeat the action:  "noRepeat"
#		means don't auto-repeat, "initial" means this is the
#		first action in an auto-repeat sequence, and "again"
#		means this is the second repetition or later.

proc tkScrollSelect {w element repeat} {
    global tkPriv
    if {![winfo exists $w]} return
    if {$element == "arrow1"} {
	tkScrollByUnits $w hv -1
    } elseif {$element == "trough1"} {
	tkScrollByPages $w hv -1
    } elseif {$element == "trough2"} {
	tkScrollByPages $w hv 1
    } elseif {$element == "arrow2"} {
	tkScrollByUnits $w hv 1
    } else {
	return
    }
    if {$repeat == "again"} {
	set tkPriv(afterId) [after [$w cget -repeatinterval] \
		tkScrollSelect $w $element again]
    } elseif {$repeat == "initial"} {
	set delay [$w cget -repeatdelay]
	if {$delay > 0} {
	    set tkPriv(afterId) [after $delay tkScrollSelect $w $element again]
	}
    }
}

# tkScrollStartDrag --
# This procedure is called to initiate a drag of the slider.  It just
# remembers the starting position of the mouse and slider.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	The mouse position at the start of the drag operation.

proc tkScrollStartDrag {w x y} {
    global tkPriv

    if {[$w cget -command] == ""} {
	return
    }
    set tkPriv(pressX) $x
    set tkPriv(pressY) $y
    set tkPriv(initValues) [$w get]
    set iv0 [lindex $tkPriv(initValues) 0]
    if {[llength $tkPriv(initValues)] == 2} {
	set tkPriv(initPos) $iv0
    } else {
	if {$iv0 == 0} {
	    set tkPriv(initPos) 0.0
	} else {
	    set tkPriv(initPos) [expr (double([lindex $tkPriv(initValues) 2])) \
		    / [lindex $tkPriv(initValues) 0]]
	}
    }
}

# tkScrollDrag --
# This procedure is called for each mouse motion even when the slider
# is being dragged.  It notifies the associated widget if we're not
# jump scrolling, and it just updates the scrollbar if we are jump
# scrolling.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	The current mouse position.

proc tkScrollDrag {w x y} {
    global tkPriv

    if {$tkPriv(initPos) == ""} {
	return
    }
    set delta [$w delta [expr $x - $tkPriv(pressX)] [expr $y - $tkPriv(pressY)]]
    if [$w cget -jump] {
	if {[llength $tkPriv(initValues)] == 2} {
	    $w set [expr [lindex $tkPriv(initValues) 0] + $delta] \
		    [expr [lindex $tkPriv(initValues) 1] + $delta]
	} else {
	    set delta [expr round($delta * [lindex $tkPriv(initValues) 0])]
	    eval $w set [lreplace $tkPriv(initValues) 2 3 \
		    [expr [lindex $tkPriv(initValues) 2] + $delta] \
		    [expr [lindex $tkPriv(initValues) 3] + $delta]]
	}
    } else {
	tkScrollToPos $w [expr $tkPriv(initPos) + $delta]
    }
}

# tkScrollEndDrag --
# This procedure is called to end an interactive drag of the slider.
# It scrolls the window if we're in jump mode, otherwise it does nothing.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	The mouse position at the end of the drag operation.

proc tkScrollEndDrag {w x y} {
    global tkPriv

    if {$tkPriv(initPos) == ""} {
	return
    }
    if [$w cget -jump] {
	set delta [$w delta [expr $x - $tkPriv(pressX)] \
		[expr $y - $tkPriv(pressY)]]
	tkScrollToPos $w [expr $tkPriv(initPos) + $delta]
    }
    set tkPriv(initPos) ""
}

# tkScrollByUnits --
# This procedure tells the scrollbar's associated widget to scroll up
# or down by a given number of units.  It notifies the associated widget
# in different ways for old and new command syntaxes.
#
# Arguments:
# w -		The scrollbar widget.
# orient -	Which kinds of scrollbars this applies to:  "h" for
#		horizontal, "v" for vertical, "hv" for both.
# amount -	How many units to scroll:  typically 1 or -1.

proc tkScrollByUnits {w orient amount} {
    set cmd [$w cget -command]
    if {($cmd == "") || ([string first \
	    [string index [$w cget -orient] 0] $orient] < 0)} {
	return
    }
    set info [$w get]
    if {[llength $info] == 2} {
	uplevel #0 $cmd scroll $amount units
    } else {
	uplevel #0 $cmd [expr [lindex $info 2] + $amount]
    }
}

# tkScrollByPages --
# This procedure tells the scrollbar's associated widget to scroll up
# or down by a given number of screenfuls.  It notifies the associated
# widget in different ways for old and new command syntaxes.
#
# Arguments:
# w -		The scrollbar widget.
# orient -	Which kinds of scrollbars this applies to:  "h" for
#		horizontal, "v" for vertical, "hv" for both.
# amount -	How many screens to scroll:  typically 1 or -1.

proc tkScrollByPages {w orient amount} {
    set cmd [$w cget -command]
    if {($cmd == "") || ([string first \
	    [string index [$w cget -orient] 0] $orient] < 0)} {
	return
    }
    set info [$w get]
    if {[llength $info] == 2} {
	uplevel #0 $cmd scroll $amount pages
    } else {
	uplevel #0 $cmd [expr [lindex $info 2] + $amount*([lindex $info 1] - 1)]
    }
}

# tkScrollToPos --
# This procedure tells the scrollbar's associated widget to scroll to
# a particular location, given by a fraction between 0 and 1.  It notifies
# the associated widget in different ways for old and new command syntaxes.
#
# Arguments:
# w -		The scrollbar widget.
# pos -		A fraction between 0 and 1 indicating a desired position
#		in the document.

proc tkScrollToPos {w pos} {
    set cmd [$w cget -command]
    if {($cmd == "")} {
	return
    }
    set info [$w get]
    if {[llength $info] == 2} {
	uplevel #0 $cmd moveto $pos
    } else {
	uplevel #0 $cmd [expr round([lindex $info 0]*$pos)]
    }
}

# tkScrollTopBottom
# Scroll to the top or bottom of the document, depending on the mouse
# position.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	Mouse coordinates within the widget.

proc tkScrollTopBottom {w x y} {
    global tkPriv
    set element [$w identify $x $y]
    if [string match *1 $element] {
	tkScrollToPos $w 0
    } elseif [string match *2 $element] {
	tkScrollToPos $w 1
    }

    # Set tkPriv(relief), since it's needed by tkScrollButtonUp.

    set tkPriv(relief) [$w cget -activerelief]
}

# tkScrollButton2Down
# This procedure is invoked when button 2 is pressed over a scrollbar.
# If the button is over the trough or slider, it sets the scrollbar to
# the mouse position and starts a slider drag.  Otherwise it just
# behaves the same as button 1.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	Mouse coordinates within the widget.

proc tkScrollButton2Down {w x y} {
    global tkPriv
    set element [$w identify $x $y]
    if {($element == "arrow1") || ($element == "arrow2")} {
	tkScrollButtonDown $w $x $y
	return
    }
    tkScrollToPos $w [$w fraction $x $y]
    set tkPriv(relief) [$w cget -activerelief]

    # Need the "update idletasks" below so that the widget calls us
    # back to reset the actual scrollbar position before we start the
    # slider drag.

    update idletasks
    $w configure -activerelief sunken
    $w activate slider
    tkScrollStartDrag $w $x $y
}
