# twind.tcl --
#
# This demonstration script creates a text widget with a bunch of
# embedded windows.
#
# SCCS: @(#) twind.tcl 1.4 96/02/16 10:49:35

set w .twind
catch {destroy $w}
toplevel $w
wm title $w "Text Demonstration - Embedded Windows"
wm iconname $w "Embedded Windows"
positionWindow $w

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

frame $w.f -highlightthickness 2 -borderwidth 2 -relief sunken
set t $w.f.text
text $t -yscrollcommand "$w.scroll set" -setgrid true -font $font -width 70 \
	-height 35 -wrap word -highlightthickness 0 -borderwidth 0
pack $t -expand  yes -fill both
scrollbar $w.scroll -command "$t yview"
pack $w.scroll -side right -fill y
pack $w.f -expand yes -fill both
$t tag configure center -justify center -spacing1 5m -spacing3 5m
$t tag configure buttons -lmargin1 1c -lmargin2 1c -rmargin 1c \
	-spacing1 3m -spacing2 0 -spacing3 0

button $t.on -text "Turn On" -command "textWindOn $w" \
	-cursor top_left_arrow
button $t.off -text "Turn Off" -command "textWindOff $w" \
	-cursor top_left_arrow
button $t.click -text "Click Here" -command "textWindPlot $t" \
	-cursor top_left_arrow
button $t.delete -text "Delete" -command "textWindDel $w" \
	-cursor top_left_arrow

$t insert end "A text widget can contain other widgets embedded "
$t insert end "it.  These are called \"embedded windows\", "
$t insert end "and they can consist of arbitrary widgets.  "
$t insert end "For example, here are two embedded button "
$t insert end "widgets.  You can click on the first button to "
$t window create end -window $t.on
$t insert end " horizontal scrolling, which also turns off "
$t insert end "word wrapping.  Or, you can click on the second "
$t insert end "button to\n"
$t window create end -window $t.off
$t insert end " horizontal scrolling and turn back on word wrapping.\n\n"

$t insert end "Or, here is another example.  If you "
$t window create end -window $t.click
$t insert end " a canvas displaying an x-y plot will appear right here."
$t mark set plot insert
$t mark gravity plot left
$t insert end "  You can drag the data points around with the mouse, "
$t insert end "or you can click here to "
$t window create end -window $t.delete
$t insert end " the plot again.\n\n"

$t insert end "You may also find it useful to put embedded windows in "
$t insert end "a text without any actual text.  In this case the "
$t insert end "text widget acts like a geometry manager.  For "
$t insert end "example, here is a collection of buttons laid out "
$t insert end "neatly into rows by the text widget.  These buttons "
$t insert end "can be used to change the background color of the "
$t insert end "text widget (\"Default\" restores the color to "
$t insert end "its default).  If you click on the button labeled "
$t insert end "\"Short\", it changes to a longer string so that "
$t insert end "you can see how the text widget automatically "
$t insert end "changes the layout.  Click on the button again "
$t insert end "to restore the short string.\n"

button $t.default -text Default -command "embDefBg $t" \
	-cursor top_left_arrow
$t window create end -window $t.default -padx 3
global embToggle
set embToggle Short
checkbutton $t.toggle -textvariable embToggle -indicatoron 0 \
	-variable embToggle -onvalue "A much longer string" \
	-offvalue "Short" -cursor top_left_arrow
$t window create end -window $t.toggle -padx 3 -pady 2
set i 1
foreach color {AntiqueWhite3 Bisque1 Bisque2 Bisque3 Bisque4
	SlateBlue3 RoyalBlue1 SteelBlue2 DeepSkyBlue3 LightBlue1
	DarkSlateGray1 Aquamarine2 DarkSeaGreen2 SeaGreen1
	Yellow1 IndianRed1 IndianRed2 Tan1 Tan4} {
    button $t.color$i -text $color -cursor top_left_arrow -command \
	    "$t configure -bg $color"
    $t window create end -window $t.color$i -padx 3 -pady 2
    incr i
}
$t tag add buttons $t.default end

proc textWindOn w {
    catch {destroy $w.scroll2}
    set t $w.f.text
    scrollbar $w.scroll2 -orient horizontal -command "$t xview"
    pack $w.scroll2 -after $w.buttons -side bottom -fill x
    $t configure -xscrollcommand "$w.scroll2 set" -wrap none
}

proc textWindOff w {
    catch {destroy $w.scroll2}
    set t $w.f.text
    $t configure -xscrollcommand {} -wrap word
}

proc textWindPlot t {
    set c $t.c
    if [winfo exists $c] {
	return
    }
    canvas $c -relief sunken -width 450 -height 300 -cursor top_left_arrow

    set font -Adobe-Helvetica-Medium-R-Normal--*-180-*-*-*-*-*-*

    $c create line 100 250 400 250 -width 2
    $c create line 100 250 100 50 -width 2
    $c create text 225 20 -text "A Simple Plot" -font $font -fill brown
    
    for {set i 0} {$i <= 10} {incr i} {
	set x [expr {100 + ($i*30)}]
	$c create line $x 250 $x 245 -width 2
	$c create text $x 254 -text [expr 10*$i] -anchor n -font $font
    }
    for {set i 0} {$i <= 5} {incr i} {
	set y [expr {250 - ($i*40)}]
	$c create line 100 $y 105 $y -width 2
	$c create text 96 $y -text [expr $i*50].0 -anchor e -font $font
    }
    
    foreach point {{12 56} {20 94} {33 98} {32 120} {61 180}
	    {75 160} {98 223}} {
	set x [expr {100 + (3*[lindex $point 0])}]
	set y [expr {250 - (4*[lindex $point 1])/5}]
	set item [$c create oval [expr $x-6] [expr $y-6] \
		[expr $x+6] [expr $y+6] -width 1 -outline black \
		-fill SkyBlue2]
	$c addtag point withtag $item
    }

    $c bind point <Any-Enter> "$c itemconfig current -fill red"
    $c bind point <Any-Leave> "$c itemconfig current -fill SkyBlue2"
    $c bind point <1> "embPlotDown $c %x %y"
    $c bind point <ButtonRelease-1> "$c dtag selected"
    bind $c <B1-Motion> "embPlotMove $c %x %y"
    while {[string first [$t get plot] " \t\n"] >= 0} {
	$t delete plot
    }
    $t insert plot "\n"
    $t window create plot -window $c
    $t tag add center plot
    $t insert plot "\n"
}

set embPlot(lastX) 0
set embPlot(lastY) 0

proc embPlotDown {w x y} {
    global embPlot
    $w dtag selected
    $w addtag selected withtag current
    $w raise current
    set embPlot(lastX) $x
    set embPlot(lastY) $y
}

proc embPlotMove {w x y} {
    global embPlot
    $w move selected [expr $x-$embPlot(lastX)] [expr $y-$embPlot(lastY)]
    set embPlot(lastX) $x
    set embPlot(lastY) $y
}

proc textWindDel w {
    set t $w.f.text
    if [winfo exists $t.c] {
	$t delete $t.c
	while {[string first [$t get plot] " \t\n"] >= 0} {
	    $t delete plot
	}
	$t insert plot "  "
    }
}

proc embDefBg t {
    $t configure -background [lindex [$t configure -background] 3]
}
