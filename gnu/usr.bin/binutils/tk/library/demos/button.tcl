# button.tcl --
#
# This demonstration script creates a toplevel window containing
# several button widgets.
#
# SCCS: @(#) button.tcl 1.3 96/02/16 10:49:33

set w .button
catch {destroy $w}
toplevel $w
wm title $w "Button Demonstration"
wm iconname $w "button"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "If you click on any of the four buttons below, the background of the button area will change to the color indicated in the button.  You can press Tab to move among the buttons, then press Space to invoke the current button."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

button $w.b1 -text "Peach Puff" -width 10 \
	-command "$w config -bg PeachPuff1"
button $w.b2 -text "Light Blue" -width 10 \
	-command "$w config -bg LightBlue1"
button $w.b3 -text "Sea Green" -width 10 \
	-command "$w config -bg SeaGreen2"
button $w.b4 -text "Yellow" -width 10 \
	-command "$w config -bg Yellow1"
pack $w.b1 $w.b2 $w.b3 $w.b4 -side top -expand yes -pady 2
