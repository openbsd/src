# form.tcl --
#
# This demonstration script creates a simple form with a bunch
# of entry widgets.
#
# SCCS: @(#) form.tcl 1.4 96/02/16 10:49:30

set w .form
catch {destroy $w}
toplevel $w
wm title $w "Form Demonstration"
wm iconname $w "form"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "This window contains a simple form where you can type in the various entries and use tabs to move circularly between the entries."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

foreach i {f1 f2 f3 f4 f5} {
    frame $w.$i -bd 2
    entry $w.$i.entry -relief sunken -width 40
    label $w.$i.label
    pack $w.$i.entry -side right
    pack $w.$i.label -side left
}
$w.f1.label config -text Name:
$w.f2.label config -text Address:
$w.f5.label config -text Phone:
pack $w.msg $w.f1 $w.f2 $w.f3 $w.f4 $w.f5 -side top -fill x
bind $w <Return> "destroy $w"
focus $w.f1.entry
