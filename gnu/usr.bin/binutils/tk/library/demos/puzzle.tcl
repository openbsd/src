# puzzle.tcl --
#
# This demonstration script creates a 15-puzzle game using a collection
# of buttons.
#
# SCCS: @(#) puzzle.tcl 1.4 96/02/16 10:49:48

# puzzleSwitch --
# This procedure is invoked when the user clicks on a particular button;
# if the button is next to the empty space, it moves the button into th
# empty space.

proc puzzleSwitch {w num} {
    global xpos ypos
    if {(($ypos($num) >= ($ypos(space) - .01))
	    && ($ypos($num) <= ($ypos(space) + .01))
	    && ($xpos($num) >= ($xpos(space) - .26))
	    && ($xpos($num) <= ($xpos(space) + .26)))
	    || (($xpos($num) >= ($xpos(space) - .01))
	    && ($xpos($num) <= ($xpos(space) + .01))
	    && ($ypos($num) >= ($ypos(space) - .26))
	    && ($ypos($num) <= ($ypos(space) + .26)))} {
	set tmp $xpos(space)
	set xpos(space) $xpos($num)
	set xpos($num) $tmp
	set tmp $ypos(space)
	set ypos(space) $ypos($num)
	set ypos($num) $tmp
	place $w.frame.$num -relx $xpos($num) -rely $ypos($num)
    }
}

set w .puzzle
catch {destroy $w}
toplevel $w
wm title $w "15-Puzzle Demonstration"
wm iconname $w "15-Puzzle"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "A 15-puzzle appears below as a collection of buttons.  Click on any of the pieces next to the space, and that piece will slide over the space.  Continue this until the pieces are arranged in numerical order from upper-left to lower-right."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

# Special trick: select a darker color for the space by creating a
# scrollbar widget and using its trough color.

scrollbar $w.s
frame $w.frame -width 120 -height 120 -borderwidth 2 -relief sunken \
	-bg [$w.s cget -troughcolor]
pack $w.frame -side top -pady 1c -padx 1c
destroy $w.s

set order {3 1 6 2 5 7 15 13 4 11 8 9 14 10 12}
for {set i 0} {$i < 15} {set i [expr $i+1]} {
    set num [lindex $order $i]
    set xpos($num) [expr ($i%4)*.25]
    set ypos($num) [expr ($i/4)*.25]
    button $w.frame.$num -relief raised -text $num -highlightthickness 0 \
	    -command "puzzleSwitch $w $num"
    place $w.frame.$num -relx $xpos($num) -rely $ypos($num) \
	-relwidth .25 -relheight .25
}
set xpos(space) .75
set ypos(space) .75
