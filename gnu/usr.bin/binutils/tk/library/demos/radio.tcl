# radio.tcl --
#
# This demonstration script creates a toplevel window containing
# several radiobutton widgets.
#
# SCCS: @(#) radio.tcl 1.4 96/02/16 10:49:34

set w .radio
catch {destroy $w}
toplevel $w
wm title $w "Radiobutton Demonstration"
wm iconname $w "radio"
positionWindow $w
label $w.msg -font $font -wraplength 5i -justify left -text "Two groups of radiobuttons are displayed below.  If you click on a button then the button will become selected exclusively among all the buttons in its group.  A Tcl variable is associated with each group to indicate which of the group's buttons is selected.  Click the \"See Variables\" button to see the current values of the variables."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
button $w.buttons.vars -text "See Variables"  \
	-command "showVars $w.dialog size color"
pack $w.buttons.dismiss $w.buttons.code $w.buttons.vars -side left -expand 1

frame $w.left
frame $w.right
pack $w.left $w.right -side left -expand yes  -pady .5c -padx .5c

foreach i {10 12 18 24} {
    radiobutton $w.left.b$i -text "Point Size $i" -variable size \
	    -relief flat -value $i
    pack $w.left.b$i  -side top -pady 2 -anchor w
}

foreach color {Red Green Blue Yellow Orange Purple} {
    set lower [string tolower $color]
    radiobutton $w.right.$lower -text $color -variable color \
	    -relief flat -value $lower
    pack $w.right.$lower -side top -pady 2 -anchor w
}
