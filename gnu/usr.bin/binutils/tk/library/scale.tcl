# scale.tcl --
#
# This file defines the default bindings for Tk scale widgets and provides
# procedures that help in implementing the bindings.
#
# SCCS: @(#) scale.tcl 1.12 96/04/16 11:42:25
#
# Copyright (c) 1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# The code below creates the default class bindings for entries.
#-------------------------------------------------------------------------

# Standard Motif bindings:

bind Scale <Enter> {
    if $tk_strictMotif {
	set tkPriv(activeBg) [%W cget -activebackground]
	%W config -activebackground [%W cget -background]
    }
    tkScaleActivate %W %x %y
}
bind Scale <Motion> {
    tkScaleActivate %W %x %y
}
bind Scale <Leave> {
    if $tk_strictMotif {
	%W config -activebackground $tkPriv(activeBg)
    }
    if {[%W cget -state] == "active"} {
	%W configure -state normal
    }
}
bind Scale <1> {
    tkScaleButtonDown %W %x %y
}
bind Scale <B1-Motion> {
    tkScaleDrag %W %x %y
}
bind Scale <B1-Leave> { }
bind Scale <B1-Enter> { }
bind Scale <ButtonRelease-1> {
    tkCancelRepeat
    tkScaleEndDrag %W
    tkScaleActivate %W %x %y
}
bind Scale <2> {
    tkScaleButton2Down %W %x %y
}
bind Scale <B2-Motion> {
    tkScaleDrag %W %x %y
}
bind Scale <B2-Leave> { }
bind Scale <B2-Enter> { }
bind Scale <ButtonRelease-2> {
    tkCancelRepeat
    tkScaleEndDrag %W
    tkScaleActivate %W %x %y
}
bind Scale <Control-1> {
    tkScaleControlPress %W %x %y
}
bind Scale <Up> {
    tkScaleIncrement %W up little noRepeat
}
bind Scale <Down> {
    tkScaleIncrement %W down little noRepeat
}
bind Scale <Left> {
    tkScaleIncrement %W up little noRepeat
}
bind Scale <Right> {
    tkScaleIncrement %W down little noRepeat
}
bind Scale <Control-Up> {
    tkScaleIncrement %W up big noRepeat
}
bind Scale <Control-Down> {
    tkScaleIncrement %W down big noRepeat
}
bind Scale <Control-Left> {
    tkScaleIncrement %W up big noRepeat
}
bind Scale <Control-Right> {
    tkScaleIncrement %W down big noRepeat
}
bind Scale <Home> {
    %W set [%W cget -from]
}
bind Scale <End> {
    %W set [%W cget -to]
}

# tkScaleActivate --
# This procedure is invoked to check a given x-y position in the
# scale and activate the slider if the x-y position falls within
# the slider.
#
# Arguments:
# w -		The scale widget.
# x, y -	Mouse coordinates.

proc tkScaleActivate {w x y} {
    global tkPriv
    if {[$w cget -state] == "disabled"} {
	return;
    }
    if {[$w identify $x $y] == "slider"} {
	$w configure -state active
    } else {
	$w configure -state normal
    }
}

# tkScaleButtonDown --
# This procedure is invoked when a button is pressed in a scale.  It
# takes different actions depending on where the button was pressed.
#
# Arguments:
# w -		The scale widget.
# x, y -	Mouse coordinates of button press.

proc tkScaleButtonDown {w x y} {
    global tkPriv
    set tkPriv(dragging) 0
    set el [$w identify $x $y]
    if {$el == "trough1"} {
	tkScaleIncrement $w up little initial
    } elseif {$el == "trough2"} {
	tkScaleIncrement $w down little initial
    } elseif {$el == "slider"} {
	set tkPriv(dragging) 1
	set tkPriv(initValue) [$w get]
	set coords [$w coords]
	set tkPriv(deltaX) [expr $x - [lindex $coords 0]]
	set tkPriv(deltaY) [expr $y - [lindex $coords 1]]
	$w configure -sliderrelief sunken
    }
}

# tkScaleDrag --
# This procedure is called when the mouse is dragged with
# mouse button 1 down.  If the drag started inside the slider
# (i.e. the scale is active) then the scale's value is adjusted
# to reflect the mouse's position.
#
# Arguments:
# w -		The scale widget.
# x, y -	Mouse coordinates.

proc tkScaleDrag {w x y} {
    global tkPriv
    if !$tkPriv(dragging) {
	return
    }
    $w set [$w get [expr $x - $tkPriv(deltaX)] \
	    [expr $y - $tkPriv(deltaY)]]
}

# tkScaleEndDrag --
# This procedure is called to end an interactive drag of the
# slider.  It just marks the drag as over.
#
# Arguments:
# w -		The scale widget.

proc tkScaleEndDrag {w} {
    global tkPriv
    set tkPriv(dragging) 0
    $w configure -sliderrelief raised
}

# tkScaleIncrement --
# This procedure is invoked to increment the value of a scale and
# to set up auto-repeating of the action if that is desired.  The
# way the value is incremented depends on the "dir" and "big"
# arguments.
#
# Arguments:
# w -		The scale widget.
# dir -		"up" means move value towards -from, "down" means
#		move towards -to.
# big -		Size of increments: "big" or "little".
# repeat -	Whether and how to auto-repeat the action:  "noRepeat"
#		means don't auto-repeat, "initial" means this is the
#		first action in an auto-repeat sequence, and "again"
#		means this is the second repetition or later.

proc tkScaleIncrement {w dir big repeat} {
    global tkPriv
    if {![winfo exists $w]} return
    if {$big == "big"} {
	set inc [$w cget -bigincrement]
	if {$inc == 0} {
	    set inc [expr abs([$w cget -to] - [$w cget -from])/10.0]
	}
	if {$inc < [$w cget -resolution]} {
	    set inc [$w cget -resolution]
	}
    } else {
	set inc [$w cget -resolution]
    }
    if {([$w cget -from] > [$w cget -to]) ^ ($dir == "up")} {
	set inc [expr -$inc]
    }
    $w set [expr [$w get] + $inc]

    if {$repeat == "again"} {
	set tkPriv(afterId) [after [$w cget -repeatinterval] \
		tkScaleIncrement $w $dir $big again]
    } elseif {$repeat == "initial"} {
	set delay [$w cget -repeatdelay]
	if {$delay > 0} {
	    set tkPriv(afterId) [after $delay \
		    tkScaleIncrement $w $dir $big again]
	}
    }
}

# tkScaleControlPress --
# This procedure handles button presses that are made with the Control
# key down.  Depending on the mouse position, it adjusts the scale
# value to one end of the range or the other.
#
# Arguments:
# w -		The scale widget.
# x, y -	Mouse coordinates where the button was pressed.

proc tkScaleControlPress {w x y} {
    set el [$w identify $x $y]
    if {$el == "trough1"} {
	$w set [$w cget -from]
    } elseif {$el == "trough2"} {
	$w set [$w cget -to]
    }
}

# tkScaleButton2Down
# This procedure is invoked when button 2 is pressed over a scale.
# It sets the value to correspond to the mouse position and starts
# a slider drag.
#
# Arguments:
# w -		The scrollbar widget.
# x, y -	Mouse coordinates within the widget.

proc tkScaleButton2Down {w x y} {
    global tkPriv

    if {[$w cget -state] == "disabled"} {
	return;
    }
    $w configure -state active
    $w set [$w get $x $y]
    set tkPriv(dragging) 1
    set tkPriv(initValue) [$w get]
    set coords "$x $y"
    set tkPriv(deltaX) 0
    set tkPriv(deltaY) 0
}
