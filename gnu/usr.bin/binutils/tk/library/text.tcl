# text.tcl --
#
# This file defines the default bindings for Tk text widgets and provides
# procedures that help in implementing the bindings.
#
# SCCS: @(#) text.tcl 1.44 96/04/16 11:42:24
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

#-------------------------------------------------------------------------
# Elements of tkPriv that are used in this file:
#
# afterId -		If non-null, it means that auto-scanning is underway
#			and it gives the "after" id for the next auto-scan
#			command to be executed.
# char -		Character position on the line;  kept in order
#			to allow moving up or down past short lines while
#			still remembering the desired position.
# mouseMoved -		Non-zero means the mouse has moved a significant
#			amount since the button went down (so, for example,
#			start dragging out a selection).
# prevPos -		Used when moving up or down lines via the keyboard.
#			Keeps track of the previous insert position, so
#			we can distinguish a series of ups and downs, all
#			in a row, from a new up or down.
# selectMode -		The style of selection currently underway:
#			char, word, or line.
# x, y -		Last known mouse coordinates for scanning
#			and auto-scanning.
#-------------------------------------------------------------------------

# tkTextClipboardKeysyms --
# This procedure is invoked to identify the keys that correspond to
# the "copy", "cut", and "paste" functions for the clipboard.
#
# Arguments:
# copy -	Name of the key (keysym name plus modifiers, if any,
#		such as "Meta-y") used for the copy operation.
# cut -		Name of the key used for the cut operation.
# paste -	Name of the key used for the paste operation.

proc tkTextClipboardKeysyms {copy cut paste} {
    bind Text <$copy> {tk_textCopy %W}
    bind Text <$cut> {tk_textCut %W}
    bind Text <$paste> {tk_textPaste %W}
}

#-------------------------------------------------------------------------
# The code below creates the default class bindings for entries.
#-------------------------------------------------------------------------

# Standard Motif bindings:

bind Text <1> {
    tkTextButton1 %W %x %y
    %W tag remove sel 0.0 end
}
bind Text <B1-Motion> {
    set tkPriv(x) %x
    set tkPriv(y) %y
    tkTextSelectTo %W %x %y
}
bind Text <Double-1> {
    set tkPriv(selectMode) word
    tkTextSelectTo %W %x %y
    catch {%W mark set insert sel.first}
}
bind Text <Triple-1> {
    set tkPriv(selectMode) line
    tkTextSelectTo %W %x %y
    catch {%W mark set insert sel.first}
}
bind Text <Shift-1> {
    tkTextResetAnchor %W @%x,%y
    set tkPriv(selectMode) char
    tkTextSelectTo %W %x %y
}
bind Text <Double-Shift-1>	{
    set tkPriv(selectMode) word
    tkTextSelectTo %W %x %y
}
bind Text <Triple-Shift-1>	{
    set tkPriv(selectMode) line
    tkTextSelectTo %W %x %y
}
bind Text <B1-Leave> {
    set tkPriv(x) %x
    set tkPriv(y) %y
    tkTextAutoScan %W
}
bind Text <B1-Enter> {
    tkCancelRepeat
}
bind Text <ButtonRelease-1> {
    tkCancelRepeat
}
bind Text <Control-1> {
    %W mark set insert @%x,%y
}
bind Text <ButtonRelease-2> {
    if {!$tkPriv(mouseMoved) || $tk_strictMotif} {
	tkTextPaste %W %x %y
    }
}
bind Text <Left> {
    tkTextSetCursor %W insert-1c
}
bind Text <Right> {
    tkTextSetCursor %W insert+1c
}
bind Text <Up> {
    tkTextSetCursor %W [tkTextUpDownLine %W -1]
}
bind Text <Down> {
    tkTextSetCursor %W [tkTextUpDownLine %W 1]
}
bind Text <Shift-Left> {
    tkTextKeySelect %W [%W index {insert - 1c}]
}
bind Text <Shift-Right> {
    tkTextKeySelect %W [%W index {insert + 1c}]
}
bind Text <Shift-Up> {
    tkTextKeySelect %W [tkTextUpDownLine %W -1]
}
bind Text <Shift-Down> {
    tkTextKeySelect %W [tkTextUpDownLine %W 1]
}
bind Text <Control-Left> {
    tkTextSetCursor %W [%W index {insert - 1c wordstart}]
}
bind Text <Control-Right> {
    tkTextSetCursor %W [%W index {insert wordend}]
}
bind Text <Control-Up> {
    tkTextSetCursor %W [tkTextPrevPara %W insert]
}
bind Text <Control-Down> {
    tkTextSetCursor %W [tkTextNextPara %W insert]
}
bind Text <Shift-Control-Left> {
    tkTextKeySelect %W [%W index {insert - 1c wordstart}]
}
bind Text <Shift-Control-Right> {
    tkTextKeySelect %W [%W index {insert wordend}]
}
bind Text <Shift-Control-Up> {
    tkTextKeySelect %W [tkTextPrevPara %W insert]
}
bind Text <Shift-Control-Down> {
    tkTextKeySelect %W [tkTextNextPara %W insert]
}
bind Text <Prior> {
    tkTextSetCursor %W [tkTextScrollPages %W -1]
}
bind Text <Shift-Prior> {
    tkTextKeySelect %W [tkTextScrollPages %W -1]
}
bind Text <Next> {
    tkTextSetCursor %W [tkTextScrollPages %W 1]
}
bind Text <Shift-Next> {
    tkTextKeySelect %W [tkTextScrollPages %W 1]
}
bind Text <Control-Prior> {
    %W xview scroll -1 page
}
bind Text <Control-Next> {
    %W xview scroll 1 page
}

bind Text <Home> {
    tkTextSetCursor %W {insert linestart}
}
bind Text <Shift-Home> {
    tkTextKeySelect %W {insert linestart}
}
bind Text <End> {
    tkTextSetCursor %W {insert lineend}
}
bind Text <Shift-End> {
    tkTextKeySelect %W {insert lineend}
}
bind Text <Control-Home> {
    tkTextSetCursor %W 1.0
}
bind Text <Control-Shift-Home> {
    tkTextKeySelect %W 1.0
}
bind Text <Control-End> {
    tkTextSetCursor %W {end - 1 char}
}
bind Text <Control-Shift-End> {
    tkTextKeySelect %W {end - 1 char}
}

bind Text <Tab> {
    tkTextInsert %W \t
    focus %W
    break
}
bind Text <Shift-Tab> {
    # Needed only to keep <Tab> binding from triggering;  doesn't
    # have to actually do anything.
}
bind Text <Control-Tab> {
    focus [tk_focusNext %W]
}
bind Text <Control-Shift-Tab> {
    focus [tk_focusPrev %W]
}
bind Text <Control-i> {
    tkTextInsert %W \t
}
bind Text <Return> {
    tkTextInsert %W \n
}
bind Text <Delete> {
    if {[%W tag nextrange sel 1.0 end] != ""} {
	%W delete sel.first sel.last
    } else {
	%W delete insert
	%W see insert
    }
}
bind Text <BackSpace> {
    if {[%W tag nextrange sel 1.0 end] != ""} {
	%W delete sel.first sel.last
    } elseif [%W compare insert != 1.0] {
	%W delete insert-1c
	%W see insert
    }
}

bind Text <Control-space> {
    %W mark set anchor insert
}
bind Text <Select> {
    %W mark set anchor insert
}
bind Text <Control-Shift-space> {
    set tkPriv(selectMode) char
    tkTextKeyExtend %W insert
}
bind Text <Shift-Select> {
    set tkPriv(selectMode) char
    tkTextKeyExtend %W insert
}
bind Text <Control-slash> {
    %W tag add sel 1.0 end
}
bind Text <Control-backslash> {
    %W tag remove sel 1.0 end
}
tkTextClipboardKeysyms F16 F20 F18
bind Text <Insert> {
    catch {tkTextInsert %W [selection get -displayof %W]}
}
bind Text <KeyPress> {
    tkTextInsert %W %A
}

# Ignore all Alt, Meta, and Control keypresses unless explicitly bound.
# Otherwise, if a widget binding for one of these is defined, the
# <KeyPress> class binding will also fire and insert the character,
# which is wrong.  Ditto for <Escape>.

bind Text <Alt-KeyPress> {# nothing }
bind Text <Meta-KeyPress> {# nothing}
bind Text <Control-KeyPress> {# nothing}
bind Text <Escape> {# nothing}
bind Text <KP_Enter> {# nothing}

# Additional emacs-like bindings:

bind Text <Control-a> {
    if !$tk_strictMotif {
	tkTextSetCursor %W {insert linestart}
    }
}
bind Text <Control-b> {
    if !$tk_strictMotif {
	tkTextSetCursor %W insert-1c
    }
}
bind Text <Control-d> {
    if !$tk_strictMotif {
	%W delete insert
    }
}
bind Text <Control-e> {
    if !$tk_strictMotif {
	tkTextSetCursor %W {insert lineend}
    }
}
bind Text <Control-f> {
    if !$tk_strictMotif {
	tkTextSetCursor %W insert+1c
    }
}
bind Text <Control-k> {
    if !$tk_strictMotif {
	if [%W compare insert == {insert lineend}] {
	    %W delete insert
	} else {
	    %W delete insert {insert lineend}
	}
    }
}
bind Text <Control-n> {
    if !$tk_strictMotif {
	tkTextSetCursor %W [tkTextUpDownLine %W 1]
    }
}
bind Text <Control-o> {
    if !$tk_strictMotif {
	%W insert insert \n
	%W mark set insert insert-1c
    }
}
bind Text <Control-p> {
    if !$tk_strictMotif {
	tkTextSetCursor %W [tkTextUpDownLine %W -1]
    }
}
bind Text <Control-t> {
    if !$tk_strictMotif {
	tkTextTranspose %W
    }
}
bind Text <Control-v> {
    if !$tk_strictMotif {
	tkTextScrollPages %W 1
    }
}
bind Text <Meta-b> {
    if !$tk_strictMotif {
	tkTextSetCursor %W {insert - 1c wordstart}
    }
}
bind Text <Meta-d> {
    if !$tk_strictMotif {
	%W delete insert {insert wordend}
    }
}
bind Text <Meta-f> {
    if !$tk_strictMotif {
	tkTextSetCursor %W {insert wordend}
    }
}
bind Text <Meta-less> {
    if !$tk_strictMotif {
	tkTextSetCursor %W 1.0
    }
}
bind Text <Meta-greater> {
    if !$tk_strictMotif {
	tkTextSetCursor %W end-1c
    }
}
bind Text <Meta-BackSpace> {
    if !$tk_strictMotif {
	%W delete {insert -1c wordstart} insert
    }
}
bind Text <Meta-Delete> {
    if !$tk_strictMotif {
	%W delete {insert -1c wordstart} insert
    }
}
if !$tk_strictMotif {
    tkTextClipboardKeysyms Meta-w Control-w Control-y
}

# A few additional bindings of my own.

bind Text <Control-h> {
    if !$tk_strictMotif {
	if [%W compare insert != 1.0] {
	    %W delete insert-1c
	    %W see insert
	}
    }
}
bind Text <2> {
    if !$tk_strictMotif {
	%W scan mark %x %y
	set tkPriv(x) %x
	set tkPriv(y) %y
	set tkPriv(mouseMoved) 0
    }
}
bind Text <B2-Motion> {
    if !$tk_strictMotif {
	if {(%x != $tkPriv(x)) || (%y != $tkPriv(y))} {
	    set tkPriv(mouseMoved) 1
	}
	if $tkPriv(mouseMoved) {
	    %W scan dragto %x %y
	}
    }
}
set tkPriv(prevPos) {}

# tkTextClosestGap --
# Given x and y coordinates, this procedure finds the closest boundary
# between characters to the given coordinates and returns the index
# of the character just after the boundary.
#
# Arguments:
# w -		The text window.
# x -		X-coordinate within the window.
# y -		Y-coordinate within the window.

proc tkTextClosestGap {w x y} {
    set pos [$w index @$x,$y]
    set bbox [$w bbox $pos]
    if ![string compare $bbox ""] {
	return $pos
    }
    if {($x - [lindex $bbox 0]) < ([lindex $bbox 2]/2)} {
	return $pos
    }
    $w index "$pos + 1 char"
}

# tkTextButton1 --
# This procedure is invoked to handle button-1 presses in text
# widgets.  It moves the insertion cursor, sets the selection anchor,
# and claims the input focus.
#
# Arguments:
# w -		The text window in which the button was pressed.
# x -		The x-coordinate of the button press.
# y -		The x-coordinate of the button press.

proc tkTextButton1 {w x y} {
    global tkPriv

    set tkPriv(selectMode) char
    set tkPriv(mouseMoved) 0
    set tkPriv(pressX) $x
    $w mark set insert [tkTextClosestGap $w $x $y]
    $w mark set anchor insert
    if {[$w cget -state] == "normal"} {focus $w}
}

# tkTextSelectTo --
# This procedure is invoked to extend the selection, typically when
# dragging it with the mouse.  Depending on the selection mode (character,
# word, line) it selects in different-sized units.  This procedure
# ignores mouse motions initially until the mouse has moved from
# one character to another or until there have been multiple clicks.
#
# Arguments:
# w -		The text window in which the button was pressed.
# x -		Mouse x position.
# y - 		Mouse y position.

proc tkTextSelectTo {w x y} {
    global tkPriv

    set cur [tkTextClosestGap $w $x $y]
    if [catch {$w index anchor}] {
	$w mark set anchor $cur
    }
    set anchor [$w index anchor]
    if {[$w compare $cur != $anchor] || (abs($tkPriv(pressX) - $x) >= 3)} {
	set tkPriv(mouseMoved) 1
    }
    switch $tkPriv(selectMode) {
	char {
	    if [$w compare $cur < anchor] {
		set first $cur
		set last anchor
	    } else {
		set first anchor
		set last $cur
	    }
	}
	word {
	    if [$w compare $cur < anchor] {
		set first [$w index "$cur wordstart"]
		set last [$w index "anchor - 1c wordend"]
	    } else {
		set first [$w index "anchor wordstart"]
		set last [$w index "$cur -1c wordend"]
	    }
	}
	line {
	    if [$w compare $cur < anchor] {
		set first [$w index "$cur linestart"]
		set last [$w index "anchor - 1c lineend + 1c"]
	    } else {
		set first [$w index "anchor linestart"]
		set last [$w index "$cur lineend + 1c"]
	    }
	}
    }
    if {$tkPriv(mouseMoved) || ($tkPriv(selectMode) != "char")} {
	$w tag remove sel 0.0 $first
	$w tag add sel $first $last
	$w tag remove sel $last end
	update idletasks
    }
}

# tkTextKeyExtend --
# This procedure handles extending the selection from the keyboard,
# where the point to extend to is really the boundary between two
# characters rather than a particular character.
#
# Arguments:
# w -		The text window.
# index -	The point to which the selection is to be extended.

proc tkTextKeyExtend {w index} {
    global tkPriv

    set cur [$w index $index]
    if [catch {$w index anchor}] {
	$w mark set anchor $cur
    }
    set anchor [$w index anchor]
    if [$w compare $cur < anchor] {
	set first $cur
	set last anchor
    } else {
	set first anchor
	set last $cur
    }
    $w tag remove sel 0.0 $first
    $w tag add sel $first $last
    $w tag remove sel $last end
}

# tkTextPaste --
# This procedure sets the insertion cursor to the mouse position,
# inserts the selection, and sets the focus to the window.
#
# Arguments:
# w -		The text window.
# x, y - 	Position of the mouse.

proc tkTextPaste {w x y} {
    $w mark set insert [tkTextClosestGap $w $x $y]
    catch {$w insert insert [selection get -displayof $w]}
    if {[$w cget -state] == "normal"} {focus $w}
}

# tkTextAutoScan --
# This procedure is invoked when the mouse leaves a text window
# with button 1 down.  It scrolls the window up, down, left, or right,
# depending on where the mouse is (this information was saved in
# tkPriv(x) and tkPriv(y)), and reschedules itself as an "after"
# command so that the window continues to scroll until the mouse
# moves back into the window or the mouse button is released.
#
# Arguments:
# w -		The text window.

proc tkTextAutoScan {w} {
    global tkPriv
    if {![winfo exists $w]} return
    if {$tkPriv(y) >= [winfo height $w]} {
	$w yview scroll 2 units
    } elseif {$tkPriv(y) < 0} {
	$w yview scroll -2 units
    } elseif {$tkPriv(x) >= [winfo width $w]} {
	$w xview scroll 2 units
    } elseif {$tkPriv(x) < 0} {
	$w xview scroll -2 units
    } else {
	return
    }
    tkTextSelectTo $w $tkPriv(x) $tkPriv(y)
    set tkPriv(afterId) [after 50 tkTextAutoScan $w]
}

# tkTextSetCursor
# Move the insertion cursor to a given position in a text.  Also
# clears the selection, if there is one in the text, and makes sure
# that the insertion cursor is visible.  Also, don't let the insertion
# cursor appear on the dummy last line of the text.
#
# Arguments:
# w -		The text window.
# pos -		The desired new position for the cursor in the window.

proc tkTextSetCursor {w pos} {
    global tkPriv

    if [$w compare $pos == end] {
	set pos {end - 1 chars}
    }
    $w mark set insert $pos
    $w tag remove sel 1.0 end
    $w see insert
}

# tkTextKeySelect
# This procedure is invoked when stroking out selections using the
# keyboard.  It moves the cursor to a new position, then extends
# the selection to that position.
#
# Arguments:
# w -		The text window.
# new -		A new position for the insertion cursor (the cursor hasn't
#		actually been moved to this position yet).

proc tkTextKeySelect {w new} {
    global tkPriv

    if {[$w tag nextrange sel 1.0 end] == ""} {
	if [$w compare $new < insert] {
	    $w tag add sel $new insert
	} else {
	    $w tag add sel insert $new
	}
	$w mark set anchor insert
    } else {
	if [$w compare $new < anchor] {
	    set first $new
	    set last anchor
	} else {
	    set first anchor
	    set last $new
	}
	$w tag remove sel 1.0 $first
	$w tag add sel $first $last
	$w tag remove sel $last end
    }
    $w mark set insert $new
    $w see insert
    update idletasks
}

# tkTextResetAnchor --
# Set the selection anchor to whichever end is farthest from the
# index argument.  One special trick: if the selection has two or
# fewer characters, just leave the anchor where it is.  In this
# case it doesn't matter which point gets chosen for the anchor,
# and for the things like Shift-Left and Shift-Right this produces
# better behavior when the cursor moves back and forth across the
# anchor.
#
# Arguments:
# w -		The text widget.
# index -	Position at which mouse button was pressed, which determines
#		which end of selection should be used as anchor point.

proc tkTextResetAnchor {w index} {
    global tkPriv

    if {[$w tag ranges sel] == ""} {
	$w mark set anchor $index
	return
    }
    set a [$w index $index]
    set b [$w index sel.first]
    set c [$w index sel.last]
    if [$w compare $a < $b] {
	$w mark set anchor sel.last
	return
    }
    if [$w compare $a > $c] {
	$w mark set anchor sel.first
	return
    }
    scan $a "%d.%d" lineA chA
    scan $b "%d.%d" lineB chB
    scan $c "%d.%d" lineC chC
    if {$lineB < $lineC+2} {
	set total [string length [$w get $b $c]]
	if {$total <= 2} {
	    return
	}
	if {[string length [$w get $b $a]] < ($total/2)} {
	    $w mark set anchor sel.last
	} else {
	    $w mark set anchor sel.first
	}
	return
    }
    if {($lineA-$lineB) < ($lineC-$lineA)} {
	$w mark set anchor sel.last
    } else {
	$w mark set anchor sel.first
    }
}

# tkTextInsert --
# Insert a string into a text at the point of the insertion cursor.
# If there is a selection in the text, and it covers the point of the
# insertion cursor, then delete the selection before inserting.
#
# Arguments:
# w -		The text window in which to insert the string
# s -		The string to insert (usually just a single character)

proc tkTextInsert {w s} {
    if {($s == "") || ([$w cget -state] == "disabled")} {
	return
    }
    catch {
	if {[$w compare sel.first <= insert]
		&& [$w compare sel.last >= insert]} {
	    $w delete sel.first sel.last
	}
    }
    $w insert insert $s
    $w see insert
}

# tkTextUpDownLine --
# Returns the index of the character one line above or below the
# insertion cursor.  There are two tricky things here.  First,
# we want to maintain the original column across repeated operations,
# even though some lines that will get passed through don't have
# enough characters to cover the original column.  Second, don't
# try to scroll past the beginning or end of the text.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# n -		The number of lines to move: -1 for up one line,
#		+1 for down one line.

proc tkTextUpDownLine {w n} {
    global tkPriv

    set i [$w index insert]
    scan $i "%d.%d" line char
    if {[string compare $tkPriv(prevPos) $i] != 0} {
	set tkPriv(char) $char
    }
    set new [$w index [expr $line + $n].$tkPriv(char)]
    if {[$w compare $new == end] || [$w compare $new == "insert linestart"]} {
	set new $i
    }
    set tkPriv(prevPos) $new
    return $new
}

# tkTextPrevPara --
# Returns the index of the beginning of the paragraph just before a given
# position in the text (the beginning of a paragraph is the first non-blank
# character after a blank line).
#
# Arguments:
# w -		The text window in which the cursor is to move.
# pos -		Position at which to start search.

proc tkTextPrevPara {w pos} {
    set pos [$w index "$pos linestart"]
    while 1 {
	if {(([$w get "$pos - 1 line"] == "\n") && ([$w get $pos] != "\n"))
		|| ($pos == "1.0")} {
	    if [regexp -indices {^[ 	]+(.)} [$w get $pos "$pos lineend"] \
		    dummy index] {
		set pos [$w index "$pos + [lindex $index 0] chars"]
	    }
	    if {[$w compare $pos != insert] || ($pos == "1.0")} {
		return $pos
	    }
	}
	set pos [$w index "$pos - 1 line"]
    }
}

# tkTextNextPara --
# Returns the index of the beginning of the paragraph just after a given
# position in the text (the beginning of a paragraph is the first non-blank
# character after a blank line).
#
# Arguments:
# w -		The text window in which the cursor is to move.
# start -	Position at which to start search.

proc tkTextNextPara {w start} {
    set pos [$w index "$start linestart + 1 line"]
    while {[$w get $pos] != "\n"} {
	if [$w compare $pos == end] {
	    return [$w index "end - 1c"]
	}
	set pos [$w index "$pos + 1 line"]
    }
    while {[$w get $pos] == "\n"} {
	set pos [$w index "$pos + 1 line"]
	if [$w compare $pos == end] {
	    return [$w index "end - 1c"]
	}
    }
    if [regexp -indices {^[ 	]+(.)} [$w get $pos "$pos lineend"] \
	    dummy index] {
	return [$w index "$pos + [lindex $index 0] chars"]
    }
    return $pos
}

# tkTextScrollPages --
# This is a utility procedure used in bindings for moving up and down
# pages and possibly extending the selection along the way.  It scrolls
# the view in the widget by the number of pages, and it returns the
# index of the character that is at the same position in the new view
# as the insertion cursor used to be in the old view.
#
# Arguments:
# w -		The text window in which the cursor is to move.
# count -	Number of pages forward to scroll;  may be negative
#		to scroll backwards.

proc tkTextScrollPages {w count} {
    set bbox [$w bbox insert]
    $w yview scroll $count pages
    if {$bbox == ""} {
	return [$w index @[expr [winfo height $w]/2],0]
    }
    return [$w index @[lindex $bbox 0],[lindex $bbox 1]]
}

# tkTextTranspose --
# This procedure implements the "transpose" function for text widgets.
# It tranposes the characters on either side of the insertion cursor,
# unless the cursor is at the end of the line.  In this case it
# transposes the two characters to the left of the cursor.  In either
# case, the cursor ends up to the right of the transposed characters.
#
# Arguments:
# w -		Text window in which to transpose.

proc tkTextTranspose w {
    set pos insert
    if [$w compare $pos != "$pos lineend"] {
	set pos [$w index "$pos + 1 char"]
    }
    set new [$w get "$pos - 1 char"][$w get  "$pos - 2 char"]
    if [$w compare "$pos - 1 char" == 1.0] {
	return
    }
    $w delete "$pos - 2 char" $pos
    $w insert insert $new
    $w see insert
}

# tk_textCopy --
# This procedure copies the selection from a text widget into the
# clipboard.
#
# Arguments:
# w -		Name of a text widget.

proc tk_textCopy w {
    if {[selection own -displayof $w] == "$w"} {
	clipboard clear -displayof $w
	catch {
	    clipboard append -displayof $w [selection get -displayof $w]
	}
    }
}

# tk_textCut --
# This procedure copies the selection from a text widget into the
# clipboard, then deletes the selection (if it exists in the given
# widget).
#
# Arguments:
# w -		Name of a text widget.

proc tk_textCut w {
    if {[selection own -displayof $w] == "$w"} {
	clipboard clear -displayof $w
	catch {
	    clipboard append -displayof $w [selection get -displayof $w]
	    $w delete sel.first sel.last
	}
    }
}

# tk_textPaste --
# This procedure pastes the contents of the clipboard to the insertion
# point in a text widget.
#
# Arguments:
# w -		Name of a text widget.

proc tk_textPaste w {
    catch {
	$w insert insert [selection get -displayof $w \
		-selection CLIPBOARD]
    }
}
