# hscale.tcl --
#
# This demonstration script shows an example with a horizontal scale.
#
# SCCS: @(#) hscale.tcl 1.3 96/02/16 10:49:47

set w .hscale
catch {destroy $w}
toplevel $w
wm title $w "Horizontal Scale Demonstration"
wm iconname $w "hscale"
positionWindow $w

label $w.msg -font $font -wraplength 3.5i -justify left -text "An arrow and a horizontal scale are displayed below.  If you click or drag mouse button 1 in the scale, you can change the length of the arrow."
pack $w.msg -side top -padx .5c

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

frame $w.frame -borderwidth 10
pack $w.frame -side top -fill x

canvas $w.frame.canvas -width 50 -height 50 -bd 0 -highlightthickness 0
$w.frame.canvas create polygon 0 0 1 1 2 2 -fill DeepSkyBlue3 -tags poly
$w.frame.canvas create line 0 0 1 1 2 2 0 0 -fill black -tags line
scale $w.frame.scale -orient horizontal -length 284 -from 0 -to 250 \
	-command "setWidth $w.frame.canvas" -tickinterval 50
pack $w.frame.canvas -side top -expand yes -anchor s -fill x  -padx 15
pack $w.frame.scale -side bottom -expand yes -anchor n
$w.frame.scale set 75

proc setWidth {w width} {
    incr width 21
    set x2 [expr $width - 30]
    if {$x2 < 21} {
	set x2 21
    }
    $w coords poly 20 15 20 35 $x2 35 $x2 45 $width 25 $x2 5 $x2 15 20 15
    $w coords line 20 15 20 35 $x2 35 $x2 45 $width 25 $x2 5 $x2 15 20 15
}
