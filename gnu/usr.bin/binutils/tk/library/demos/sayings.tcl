# sayings.tcl --
#
# This demonstration script creates a listbox that can be scrolled
# both horizontally and vertically.  It displays a collection of
# well-known sayings.
#
# SCCS: @(#) sayings.tcl 1.3 96/02/16 10:49:49

set w .sayings
catch {destroy $w}
toplevel $w
wm title $w "Listbox Demonstration (well-known sayings)"
wm iconname $w "sayings"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "The listbox below contains a collection of well-known sayings.  You can scan the list using either of the scrollbars or by dragging in the listbox window with button 2 pressed."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

frame $w.frame -borderwidth 10
pack $w.frame -side top -expand yes -fill y

scrollbar $w.frame.yscroll -command "$w.frame.list yview"
scrollbar $w.frame.xscroll -orient horizontal \
	-command "$w.frame.list xview"
listbox $w.frame.list -width 20 -height 10 -setgrid 1 \
	-yscroll "$w.frame.yscroll set" -xscroll "$w.frame.xscroll set"
pack $w.frame.yscroll -side right -fill y
pack $w.frame.xscroll -side bottom -fill x
pack $w.frame.list -expand yes -fill y

$w.frame.list insert 0 "Waste not, want not" "Early to bed and early to rise makes a man healthy, wealthy, and wise" "Ask not what your country can do for you, ask what you can do for your country" "I shall return" "NOT" "A picture is worth a thousand words" "User interfaces are hard to build" "Thou shalt not steal" "A penny for your thoughts" "Fool me once, shame on you;  fool me twice, shame on me" "Every cloud has a silver lining" "Where there's smoke there's fire" "It takes one to know one" "Curiosity killed the cat" "Take this job and shove it" "Up a creek without a paddle" "I'm mad as hell and I'm not going to take it any more" "An apple a day keeps the doctor away" "Don't look a gift horse in the mouth"
