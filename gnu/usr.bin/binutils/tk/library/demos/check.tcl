# check.tcl --
#
# This demonstration script creates a toplevel window containing
# several checkbuttons.
#
# SCCS: @(#) check.tcl 1.3 96/02/16 10:49:37

set w .check
catch {destroy $w}
toplevel $w
wm title $w "Checkbutton Demonstration"
wm iconname $w "check"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "Three checkbuttons are displayed below.  If you click on a button, it will toggle the button's selection state and set a Tcl variable to a value indicating the state of the checkbutton.  Click the \"See Variables\" button to see the current values of the variables."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
button $w.buttons.vars -text "See Variables"  \
	-command "showVars $w.dialog wipers brakes sober"
pack $w.buttons.dismiss $w.buttons.code $w.buttons.vars -side left -expand 1

checkbutton $w.b1 -text "Wipers OK" -variable wipers -relief flat
checkbutton $w.b2 -text "Brakes OK" -variable brakes -relief flat
checkbutton $w.b3 -text "Driver Sober" -variable sober -relief flat
pack $w.b1 $w.b2 $w.b3 -side top -pady 2 -anchor w
