# image2.tcl --
#
# This demonstration script creates a simple collection of widgets
# that allow you to select and view images in a Tk label.
#
# SCCS: @(#) image2.tcl 1.6 96/04/12 13:22:54

# loadDir --
# This procedure reloads the directory listbox from the directory
# named in the demo's entry.
#
# Arguments:
# w -			Name of the toplevel window of the demo.

proc loadDir w {
    global dirName

    $w.f.list delete 0 end
    foreach i [lsort [glob [file join $dirName *]]] {
	$w.f.list insert end [file tail $i]
    }
}

# loadImage --
# Given the name of the toplevel window of the demo and the mouse
# position, extracts the directory entry under the mouse and loads
# that file into a photo image for display.
#
# Arguments:
# w -			Name of the toplevel window of the demo.
# x, y-			Mouse position within the listbox.

proc loadImage {w x y} {
    global dirName

    set file [file join $dirName [$w.f.list get @$x,$y]]
    image2a configure -file $file
}

set w .image2
catch {destroy $w}
toplevel $w
wm title $w "Image Demonstration #2"
wm iconname $w "Image2"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "This demonstration allows you to view images using a Tk \"photo\" image.  First type a directory name in the listbox, then type Return to load the directory into the listbox.  Then double-click on a file name in the listbox to see that image."
pack $w.msg -side top

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

label $w.dirLabel -text "Directory:"
set dirName [file join $tk_library demos images]
entry $w.dirName -width 30 -textvariable dirName
bind $w.dirName <Return> "loadDir $w"
frame $w.spacer1 -height 3m -width 20
label $w.fileLabel -text "File:"
frame $w.f
pack $w.dirLabel $w.dirName $w.spacer1 $w.fileLabel $w.f -side top -anchor w

listbox $w.f.list -width 20 -height 10 -yscrollcommand "$w.f.scroll set"
scrollbar $w.f.scroll -command "$w.f.list yview"
pack $w.f.list $w.f.scroll -side left -fill y -expand 1
$w.f.list insert 0 earth.gif earthris.gif mickey.gif teapot.ppm
bind $w.f.list <Double-1> "loadImage $w %x %y"

catch {image delete image2a}
image create photo image2a
frame $w.spacer2 -height 3m -width 20
label $w.imageLabel -text "Image:"
label $w.image -image image2a
pack $w.spacer2 $w.imageLabel $w.image -side top -anchor w
