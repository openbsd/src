# bitmap.tcl --
#
# This demonstration script creates a toplevel window that displays
# all of Tk's built-in bitmaps.
#
# SCCS: @(#) bitmap.tcl 1.4 96/02/16 10:49:27

# bitmapRow --
# Create a row of bitmap items in a window.
#
# Arguments:
# w -		The window that is to contain the row.
# args -	The names of one or more bitmaps, which will be displayed
#		in a new row across the bottom of w along with their
#		names.

proc bitmapRow {w args} {
    frame $w
    pack $w -side top -fill both
    set i 0
    foreach bitmap $args {
	frame $w.$i
	pack $w.$i -side left -fill both -pady .25c -padx .25c
	label $w.$i.bitmap -bitmap $bitmap
	label $w.$i.label -text $bitmap -width 9
	pack $w.$i.label $w.$i.bitmap -side bottom
	incr i
    }
}

set w .bitmap
global tk_library
catch {destroy $w}
toplevel $w
wm title $w "Bitmap Demonstration"
wm iconname $w "bitmap"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "This window displays all of Tk's built-in bitmaps, along with the names you can use for them in Tcl scripts."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

frame $w.frame
bitmapRow $w.frame.0 error gray12 gray50 hourglass
bitmapRow $w.frame.1 info question questhead warning
pack $w.frame -side top -expand yes -fill both
