# palette.tcl --
#
# This file contains procedures that change the color palette used
# by Tk.
#
# SCCS: @(#) palette.tcl 1.2 96/02/16 10:48:25
#
# Copyright (c) 1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

# tk_setPalette --
# Changes the default color scheme for a Tk application by setting
# default colors in the option database and by modifying all of the
# color options for existing widgets that have the default value.
#
# Arguments:
# The arguments consist of either a single color name, which
# will be used as the new background color (all other colors will
# be computed from this) or an even number of values consisting of
# option names and values.  The name for an option is the one used
# for the option database, such as activeForeground, not -activeforeground.

proc tk_setPalette args {
    global tkPalette

    # Create an array that has the complete new palette.  If some colors
    # aren't specified, compute them from other colors that are specified.

    if {[llength $args] == 1} {
	set new(background) [lindex $args 0]
    } else {
	array set new $args
    }
    if ![info exists new(background)] {
	error "must specify a background color"
    }
    if ![info exists new(foreground)] {
	set new(foreground) black
    }
    set bg [winfo rgb . $new(background)]
    set fg [winfo rgb . $new(foreground)]
    set darkerBg [format #%02x%02x%02x [expr (9*[lindex $bg 0])/2560] \
	    [expr (9*[lindex $bg 1])/2560] [expr (9*[lindex $bg 2])/2560]]
    foreach i {activeForeground insertBackground selectForeground \
	    highlightColor} {
	if ![info exists new($i)] {
	    set new($i) $new(foreground)
	}
    }
    if ![info exists new(disabledForeground)] {
	set new(disabledForeground) [format #%02x%02x%02x \
		[expr (3*[lindex $bg 0] + [lindex $fg 0])/1024] \
		[expr (3*[lindex $bg 1] + [lindex $fg 1])/1024] \
		[expr (3*[lindex $bg 2] + [lindex $fg 2])/1024]]
    }
    if ![info exists new(highlightBackground)] {
	set new(highlightBackground) $new(background)
    }
    if ![info exists new(activeBackground)] {
	# Pick a default active background that islighter than the
	# normal background.  To do this, round each color component
	# up by 15% or 1/3 of the way to full white, whichever is
	# greater.

	foreach i {0 1 2} {
	    set light($i) [expr [lindex $bg $i]/256]
	    set inc1 [expr ($light($i)*15)/100]
	    set inc2 [expr (255-$light($i))/3]
	    if {$inc1 > $inc2} {
		incr light($i) $inc1
	    } else {
		incr light($i) $inc2
	    }
	    if {$light($i) > 255} {
		set light($i) 255
	    }
	}
	set new(activeBackground) [format #%02x%02x%02x $light(0) \
		$light(1) $light(2)]
    }
    if ![info exists new(selectBackground)] {
	set new(selectBackground) $darkerBg
    }
    if ![info exists new(troughColor)] {
	set new(troughColor) $darkerBg
    }
    if ![info exists new(selectColor)] {
	set new(selectColor) #b03060
    }

    # Walk the widget hierarchy, recoloring all existing windows.
    # Before doing this, make sure that the tkPalette variable holds
    # the default values of all options, so that tkRecolorTree can
    # be sure to only change options that have their default values.
    # If the variable exists, then it is already correct (it was created
    # the last time this procedure was invoked).  If the variable
    # doesn't exist, fill it in using the defaults from a few widgets.

    if ![info exists tkPalette] {
	checkbutton .c14732
	entry .e14732
	scrollbar .s14732
	set tkPalette(activeBackground) \
		[lindex [.c14732 configure -activebackground] 3]
	set tkPalette(activeForeground) \
		[lindex [.c14732 configure -activeforeground] 3]
	set tkPalette(background) \
		[lindex [.c14732 configure -background] 3]
	set tkPalette(disabledForeground) \
		[lindex [.c14732 configure -disabledforeground] 3]
	set tkPalette(foreground) \
		[lindex [.c14732 configure -foreground] 3]
	set tkPalette(highlightBackground) \
		[lindex [.c14732 configure -highlightbackground] 3]
	set tkPalette(highlightColor) \
		[lindex [.c14732 configure -highlightcolor] 3]
	set tkPalette(insertBackground) \
		[lindex [.e14732 configure -insertbackground] 3]
	set tkPalette(selectColor) \
		[lindex [.c14732 configure -selectcolor] 3]
	set tkPalette(selectBackground) \
		[lindex [.e14732 configure -selectbackground] 3]
	set tkPalette(selectForeground) \
		[lindex [.e14732 configure -selectforeground] 3]
	set tkPalette(troughColor) \
		[lindex [.s14732 configure -troughcolor] 3]
	destroy .c14732 .e14732 .s14732
    }
    tkRecolorTree . new

    # Change the option database so that future windows will get the
    # same colors.

    foreach option [array names new] {
	option add *$option $new($option) widgetDefault
    }

    # Save the options in the global variable tkPalette, for use the
    # next time we change the options.

    array set tkPalette [array get new]
}

# tkRecolorTree --
# This procedure changes the colors in a window and all of its
# descendants, according to information provided by the colors
# argument.  It only modifies colors that have their default values
# as specified by the tkPalette variable.
#
# Arguments:
# w -			The name of a window.  This window and all its
#			descendants are recolored.
# colors -		The name of an array variable in the caller,
#			which contains color information.  Each element
#			is named after a widget configuration option, and
#			each value is the value for that option.

proc tkRecolorTree {w colors} {
    global tkPalette
    upvar $colors c
    foreach dbOption [array names c] {
	set option -[string tolower $dbOption]
	if ![catch {$w cget $option} value] {
	    if {$value == $tkPalette($dbOption)} {
		$w configure $option $c($dbOption)
	    }
	}
    }
    foreach child [winfo children $w] {
	tkRecolorTree $child c
    }
}

# tkDarken --
# Given a color name, computes a new color value that darkens (or
# brightens) the given color by a given percent.
#
# Arguments:
# color -	Name of starting color.
# perecent -	Integer telling how much to brighten or darken as a
#		percent: 50 means darken by 50%, 110 means brighten
#		by 10%.

proc tkDarken {color percent} {
    set l [winfo rgb . $color]
    set red [expr [lindex $l 0]/256]
    set green [expr [lindex $l 1]/256]
    set blue [expr [lindex $l 2]/256]
    set red [expr ($red*$percent)/100]
    if {$red > 255} {
	set red 255
    }
    set green [expr ($green*$percent)/100]
    if {$green > 255} {
	set green 255
    }
    set blue [expr ($blue*$percent)/100]
    if {$blue > 255} {
	set blue 255
    }
    format #%02x%02x%02x $red $green $blue
}

# tk_bisque --
# Reset the Tk color palette to the old "bisque" colors.
#
# Arguments:
# None.

proc tk_bisque {} {
    tk_setPalette activeBackground #e6ceb1 activeForeground black \
	    background #ffe4c4 disabledForeground #b0b0b0 foreground black \
	    highlightBackground #ffe4c4 highlightColor black \
	    insertBackground black selectColor #b03060 \
	    selectBackground #e6ceb1 selectForeground black \
	    troughColor #cdb79e
}
