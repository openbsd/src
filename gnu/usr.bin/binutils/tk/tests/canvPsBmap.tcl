# This file creates a screen to exercise Postscript generation
# for bitmaps in canvases.  It is part of the Tk visual test suite,
# which is invoked via the "visual" script.
#
# SCCS: @(#) canvPsBmap.tcl 1.3 96/02/16 10:55:44

catch {destroy .t}
toplevel .t
wm title .t "Postscript Tests for Canvases"
wm iconname .t "Postscript"
wm geom .t +0+0
wm minsize .t 1 1

set c .t.c

message .t.m -text {This screen exercises the Postscript-generation abilities of Tk canvas widgets for bitmaps.  Click on "Print" to print the canvas to your default printer.  You can click on items in the canvas to delete them.} -width 6i
pack .t.m -side top -fill both

frame .t.bot
pack .t.bot -side bottom -fill both
button .t.bot.quit -text Quit -command {destroy .t}
button .t.bot.print -text Print -command "lpr $c"
pack .t.bot.print .t.bot.quit -side left -pady 1m -expand 1

canvas $c -width 6i -height 6i -bd 2 -relief sunken
pack $c -expand yes -fill both -padx 2m -pady 2m

$c create bitmap 0.5i 0.5i -bitmap @$tk_library/demos/images/flagdown \
    -background {} -foreground black -anchor nw
$c create rect 0.47i 0.47i 0.53i 0.53i -fill {} -outline black

$c create bitmap 3.0i 0.5i -bitmap @$tk_library/demos/images/flagdown \
    -background {} -foreground black -anchor n
$c create rect 2.97i 0.47i 3.03i 0.53i -fill {} -outline black

$c create bitmap 5.5i 0.5i -bitmap @$tk_library/demos/images/flagdown \
    -background black -foreground white -anchor ne
$c create rect 5.47i 0.47i 5.53i 0.53i -fill {} -outline black

$c create bitmap 0.5i 3.0i -bitmap @$tk_library/demos/images/face \
    -background {} -foreground black -anchor w
$c create rect 0.47i 2.97i 0.53i 3.03i -fill {} -outline black

$c create bitmap 3.0i 3.0i -bitmap @$tk_library/demos/images/face \
    -background {} -foreground black -anchor center
$c create rect 2.97i 2.97i 3.03i 3.03i -fill {} -outline black

$c create bitmap 5.5i 3.0i -bitmap @$tk_library/demos/images/face \
    -background blue -foreground black -anchor e
$c create rect 5.47i 2.97i 5.53i 3.03i -fill {} -outline black

$c create bitmap 0.5i 5.5i -bitmap @$tk_library/demos/images/flagup \
    -background black -foreground white -anchor sw
$c create rect 0.47i 5.47i 0.53i 5.53i -fill {} -outline black

$c create bitmap 3.0i 5.5i -bitmap @$tk_library/demos/images/flagup \
    -background green -foreground white -anchor s
$c create rect 2.97i 5.47i 3.03i 5.53i -fill {} -outline black

$c create bitmap 5.5i 5.5i -bitmap @$tk_library/demos/images/flagup \
    -background {} -foreground black -anchor se
$c create rect 5.47i 5.47i 5.53i 5.53i -fill {} -outline black
