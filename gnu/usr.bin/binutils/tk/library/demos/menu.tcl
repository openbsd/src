# menu.tcl --
#
# This demonstration script creates a window with a bunch of menus
# and cascaded menus.
#
# SCCS: @(#) menu.tcl 1.7 96/04/12 11:57:35

set w .menu
catch {destroy $w}
toplevel $w
wm title $w "Menu Demonstration"
wm iconname $w "menu"
positionWindow $w

frame $w.menu -relief raised -bd 2
pack $w.menu -side top -fill x

label $w.msg -font $font -wraplength 4i -justify left -text "This window contains a collection of menus and cascaded menus.  You can post a menu from the keyboard by typing Alt+x, where \"x\" is the character underlined on the menu.  You can then traverse among the menus using the arrow keys.  When a menu is posted, you can invoke the current entry by typing space, or you can invoke any entry by typing its underlined character.  If a menu entry has an accelerator, you can invoke the entry without posting the menu just by typing the accelerator."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

set m $w.menu.file.m
menubutton $w.menu.file -text "File" -menu $m -underline 0
menu $m
$m add command -label "Open ..." -command {error "this is just a demo: no action has been defined for the \"Open ...\" entry"}
$m add command -label "New" -command {error "this is just a demo: no action has been defined for the \"New\" entry"}
$m add command -label "Save" -command {error "this is just a demo: no action has been defined for the \"Save\" entry"}
$m add command -label "Save As ..." -command {error "this is just a demo: no action has been defined for the \"Save As ...\" entry"}
$m add separator
$m add command -label "Print Setup ..." -command {error "this is just a demo: no action has been defined for the \"Print Setup ...\" entry"}
$m add command -label "Print ..." -command {error "this is just a demo: no action has been defined for the \"Print ...\" entry"}
$m add separator
$m add command -label "Quit" -command "destroy $w"

set m $w.menu.basic.m
menubutton $w.menu.basic -text "Basic" -menu $m -underline 0
menu $m
$m add command -label "Long entry that does nothing"
foreach i {a b c d e f g} {
    $m add command -label "Print letter \"$i\"" -underline 14 \
	    -accelerator Meta+$i -command "puts $i"
    bind $w <Meta-$i> "puts $i"
}

set m $w.menu.cascade.m
menubutton $w.menu.cascade -text "Cascades" -menu $m -underline 0
menu $m
$m add command -label "Print hello" \
	-command {puts stdout "Hello"} -accelerator Control+a -underline 6
bind . <Control-a> {puts stdout "Hello"}
$m add command -label "Print goodbye" -command {\
    puts stdout "Goodbye"} -accelerator Control+b -underline 6
bind . <Control-b> {puts stdout "Goodbye"}
$m add cascade -label "Check buttons" \
	-menu $w.menu.cascade.m.check -underline 0
$m add cascade -label "Radio buttons" \
	-menu $w.menu.cascade.m.radio -underline 0

set m $w.menu.cascade.m.check
menu $m
$m add check -label "Oil checked" -variable oil
$m add check -label "Transmission checked" -variable trans
$m add check -label "Brakes checked" -variable brakes
$m add check -label "Lights checked" -variable lights
$m add separator
$m add command -label "Show current values" \
    -command "showVars $w.menu.cascade.dialog oil trans brakes lights"
$m invoke 1
$m invoke 3

set m $w.menu.cascade.m.radio
menu $m
$m add radio -label "10 point" -variable pointSize -value 10
$m add radio -label "14 point" -variable pointSize -value 14
$m add radio -label "18 point" -variable pointSize -value 18
$m add radio -label "24 point" -variable pointSize -value 24
$m add radio -label "32 point" -variable pointSize -value 32
$m add sep
$m add radio -label "Roman" -variable style -value roman
$m add radio -label "Bold" -variable style -value bold
$m add radio -label "Italic" -variable style -value italic
$m add sep
$m add command -label "Show current values" \
	-command "showVars $w.menu.cascade.dialog pointSize style"
$m invoke 1
$m invoke 7

set m $w.menu.icon.m
menubutton $w.menu.icon -text "Icons" -menu $m -underline 0
menu $m
$m add command \
    -bitmap @[file join $tk_library demos images pattern.bmp] \
    -command {
	tk_dialog .pattern {Bitmap Menu Entry} {The menu entry you invoked displays a bitmap rather than a text string.  Other than this, it is just like any other menu entry.} {} 0 OK
}
foreach i {info questhead error} {
    $m add command -bitmap $i -command "puts {You invoked the $i bitmap}"
}

set m $w.menu.more.m
menubutton $w.menu.more -text "More" -menu $m -underline 0
menu $m
foreach i {{An entry} {Another entry} {Does nothing} {Does almost nothing} {Make life meaningful}} {
    $m add command -label $i -command [list puts "You invoked \"$i\""]
}

set m $w.menu.colors.m
menubutton $w.menu.colors -text "Colors" -menu $m -underline 1
menu $m
foreach i {red orange yellow green blue} {
    $m add command -label $i -background $i \
	    -command [list puts "You invoked \"$i\""]
}

pack $w.menu.file $w.menu.basic $w.menu.cascade $w.menu.icon $w.menu.more \
	$w.menu.colors -side left
