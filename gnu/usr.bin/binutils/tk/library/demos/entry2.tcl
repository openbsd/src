# entry2.tcl --
#
# This demonstration script is the same as the entry1.tcl script
# except that it creates scrollbars for the entries.
#
# SCCS: @(#) entry2.tcl 1.3 96/02/16 10:49:45

set w .entry2
catch {destroy $w}
toplevel $w
wm title $w "Entry Demonstration (with scrollbars)"
wm iconname $w "entry2"
positionWindow $w

label $w.msg -font $font -wraplength 5i -justify left -text "Three different entries are displayed below, with a scrollbar for each entry.  You can add characters by pointing, clicking and typing.  The normal Motif editing characters are supported, along with many Emacs bindings.  For example, Backspace and Control-h delete the character to the left of the insertion cursor and Delete and Control-d delete the chararacter to the right of the insertion cursor.  For entries that are too large to fit in the window all at once, you can scan through the entries with the scrollbars, or by dragging with mouse button2 pressed."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

frame $w.frame -borderwidth 10
pack $w.frame -side top -fill x -expand 1

entry $w.frame.e1 -relief sunken -xscrollcommand "$w.frame.s1 set"
scrollbar $w.frame.s1 -relief sunken -orient horiz -command \
	"$w.frame.e1 xview"
frame $w.frame.spacer1 -width 20 -height 10
entry $w.frame.e2 -relief sunken -xscrollcommand "$w.frame.s2 set"
scrollbar $w.frame.s2 -relief sunken -orient horiz -command \
	"$w.frame.e2 xview"
frame $w.frame.spacer2 -width 20 -height 10
entry $w.frame.e3 -relief sunken -xscrollcommand "$w.frame.s3 set"
scrollbar $w.frame.s3 -relief sunken -orient horiz -command \
	"$w.frame.e3 xview"
pack $w.frame.e1 $w.frame.s1 $w.frame.spacer1 $w.frame.e2 $w.frame.s2 \
	$w.frame.spacer2 $w.frame.e3 $w.frame.s3 -side top -fill x

$w.frame.e1 insert 0 "Initial value"
$w.frame.e2 insert end "This entry contains a long value, much too long "
$w.frame.e2 insert end "to fit in the window at one time, so long in fact "
$w.frame.e2 insert end "that you'll have to scan or scroll to see the end."
