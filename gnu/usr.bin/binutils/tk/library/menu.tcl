# menu.tcl --
#
# This file defines the default bindings for Tk menus and menubuttons.
# It also implements keyboard traversal of menus and implements a few
# other utility procedures related to menus.
#
# SCCS: @(#) menu.tcl 1.65 96/04/16 09:02:01
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
# cursor -		Saves the -cursor option for the posted menubutton.
# focus -		Saves the focus during a menu selection operation.
#			Focus gets restored here when the menu is unposted.
# grabGlobal -		Used in conjunction with tkPriv(oldGrab):  if
#			tkPriv(oldGrab) is non-empty, then tkPriv(grabGlobal)
#			contains either an empty string or "-global" to
#			indicate whether the old grab was a local one or
#			a global one.
# inMenubutton -	The name of the menubutton widget containing
#			the mouse, or an empty string if the mouse is
#			not over any menubutton.
# oldGrab -		Window that had the grab before a menu was posted.
#			Used to restore the grab state after the menu
#			is unposted.  Empty string means there was no
#			grab previously set.
# popup -		If a menu has been popped up via tk_popup, this
#			gives the name of the menu.  Otherwise this
#			value is empty.
# postedMb -		Name of the menubutton whose menu is currently
#			posted, or an empty string if nothing is posted
#			A grab is set on this widget.
# relief -		Used to save the original relief of the current
#			menubutton.
# window -		When the mouse is over a menu, this holds the
#			name of the menu;  it's cleared when the mouse
#			leaves the menu.
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# Overall note:
# This file is tricky because there are four different ways that menus
# can be used:
#
# 1. As a pulldown from a menubutton.  This is the most common usage.
#    In this style, the variable tkPriv(postedMb) identifies the posted
#    menubutton.
# 2. As a torn-off menu copied from some other menu.  In this style
#    tkPriv(postedMb) is empty, and the top-level menu is no
#    override-redirect.
# 3. As an option menu, triggered from an option menubutton.  In thi
#    style tkPriv(postedMb) identifies the posted menubutton.
# 4. As a popup menu.  In this style tkPriv(postedMb) is empty and
#    the top-level menu is override-redirect.
#
# The various binding procedures use the  state described above to
# distinguish the various cases and take different actions in each
# case.
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# The code below creates the default class bindings for menus
# and menubuttons.
#-------------------------------------------------------------------------

bind Menubutton <FocusIn> {}
bind Menubutton <Enter> {
    tkMbEnter %W
}
bind Menubutton <Leave> {
    tkMbLeave %W
}
bind Menubutton <1> {
    if {$tkPriv(inMenubutton) != ""} {
	tkMbPost $tkPriv(inMenubutton) %X %Y
    }
}
bind Menubutton <Motion> {
    tkMbMotion %W up %X %Y
}
bind Menubutton <B1-Motion> {
    tkMbMotion %W down %X %Y
}
bind Menubutton <ButtonRelease-1> {
    tkMbButtonUp %W
}
bind Menubutton <space> {
    tkMbPost %W
    tkMenuFirstEntry [%W cget -menu]
}

# Must set focus when mouse enters a menu, in order to allow
# mixed-mode processing using both the mouse and the keyboard.
# Don't set the focus if the event comes from a grab release,
# though:  such an event can happen after as part of unposting
# a cascaded chain of menus, after the focus has already been
# restored to wherever it was before menu selection started.

bind Menu <FocusIn> {}
bind Menu <Enter> {
    set tkPriv(window) %W
    if {"%m" != "NotifyUngrab"} {
	focus %W
    }
}
bind Menu <Leave> {
    tkMenuLeave %W %X %Y %s
}
bind Menu <Motion> {
    tkMenuMotion %W %y %s
}
bind Menu <ButtonPress> {
    tkMenuButtonDown %W
}
bind Menu <ButtonRelease> {
    tkMenuInvoke %W 1
}
bind Menu <space> {
    tkMenuInvoke %W 0
}
bind Menu <Return> {
    tkMenuInvoke %W 0
}
bind Menu <Escape> {
    tkMenuEscape %W
}
bind Menu <Left> {
    tkMenuLeftRight %W left
}
bind Menu <Right> {
    tkMenuLeftRight %W right
}
bind Menu <Up> {
    tkMenuNextEntry %W -1
}
bind Menu <Down> {
    tkMenuNextEntry %W +1
}
bind Menu <KeyPress> {
    tkTraverseWithinMenu %W %A
}

# The following bindings apply to all windows, and are used to
# implement keyboard menu traversal.

bind all <Alt-KeyPress> {
    tkTraverseToMenu %W %A
}
bind all <F10> {
    tkFirstMenu %W
}

# tkMbEnter --
# This procedure is invoked when the mouse enters a menubutton
# widget.  It activates the widget unless it is disabled.  Note:
# this procedure is only invoked when mouse button 1 is *not* down.
# The procedure tkMbB1Enter is invoked if the button is down.
#
# Arguments:
# w -			The  name of the widget.

proc tkMbEnter w {
    global tkPriv

    if {$tkPriv(inMenubutton) != ""} {
	tkMbLeave $tkPriv(inMenubutton)
    }
    set tkPriv(inMenubutton) $w
    if {[$w cget -state] != "disabled"} {
	$w configure -state active
    }
}

# tkMbLeave --
# This procedure is invoked when the mouse leaves a menubutton widget.
# It de-activates the widget, if the widget still exists.
#
# Arguments:
# w -			The  name of the widget.

proc tkMbLeave w {
    global tkPriv

    set tkPriv(inMenubutton) {}
    if ![winfo exists $w] {
	return
    }
    if {[$w cget -state] == "active"} {
	$w configure -state normal
    }
}

# tkMbPost --
# Given a menubutton, this procedure does all the work of posting
# its associated menu and unposting any other menu that is currently
# posted.
#
# Arguments:
# w -			The name of the menubutton widget whose menu
#			is to be posted.
# x, y -		Root coordinates of cursor, used for positioning
#			option menus.  If not specified, then the center
#			of the menubutton is used for an option menu.

proc tkMbPost {w {x {}} {y {}}} {
    global tkPriv
    if {([$w cget -state] == "disabled") || ($w == $tkPriv(postedMb))} {
	return
    }
    set menu [$w cget -menu]
    if {$menu == ""} {
	return
    }
    if ![string match $w.* $menu] {
	error "can't post $menu:  it isn't a descendant of $w (this is a new requirement in Tk versions 3.0 and later)"
    }
    set cur $tkPriv(postedMb)
    if {$cur != ""} {
	tkMenuUnpost {}
    }
    set tkPriv(cursor) [$w cget -cursor]
    set tkPriv(relief) [$w cget -relief]
    $w configure -cursor arrow
    $w configure -relief raised
    set tkPriv(postedMb) $w
    set tkPriv(focus) [focus]
    $menu activate none

    # If this looks like an option menubutton then post the menu so
    # that the current entry is on top of the mouse.  Otherwise post
    # the menu just below the menubutton, as for a pull-down.

    if [$w cget -indicatoron] {
	if {$y == ""} {
	    set x [expr [winfo rootx $w] + [winfo width $w]/2]
	    set y [expr [winfo rooty $w] + [winfo height $w]/2]
	}
	tkPostOverPoint $menu $x $y [tkMenuFindName $menu [$w cget -text]]
    } else {
	$menu post [winfo rootx $w] [expr [winfo rooty $w]+[winfo height $w]]
    }
    focus $menu
    tkSaveGrabInfo $w
    grab -global $w
}

# tkMenuUnpost --
# This procedure unposts a given menu, plus all of its ancestors up
# to (and including) a menubutton, if any.  It also restores various
# values to what they were before the menu was posted, and releases
# a grab if there's a menubutton involved.  Special notes:
# 1. It's important to unpost all menus before releasing the grab, so
#    that any Enter-Leave events (e.g. from menu back to main
#    application) have mode NotifyGrab.
# 2. Be sure to enclose various groups of commands in "catch" so that
#    the procedure will complete even if the menubutton or the menu
#    or the grab window has been deleted.
#
# Arguments:
# menu -		Name of a menu to unpost.  Ignored if there
#			is a posted menubutton.

proc tkMenuUnpost menu {
    global tkPriv
    set mb $tkPriv(postedMb)

    # Restore focus right away (otherwise X will take focus away when
    # the menu is unmapped and under some window managers (e.g. olvwm)
    # we'll lose the focus completely).

    catch {focus $tkPriv(focus)}
    set tkPriv(focus) ""

    # Unpost menu(s) and restore some stuff that's dependent on
    # what was posted.

    catch {
	if {$mb != ""} {
	    set menu [$mb cget -menu]
	    $menu unpost
	    set tkPriv(postedMb) {}
	    $mb configure -cursor $tkPriv(cursor)
	    $mb configure -relief $tkPriv(relief)
	} elseif {$tkPriv(popup) != ""} {
	    $tkPriv(popup) unpost
	    set tkPriv(popup) {}
	} elseif {[wm overrideredirect $menu]} {
	    # We're in a cascaded sub-menu from a torn-off menu or popup.
	    # Unpost all the menus up to the toplevel one (but not
	    # including the top-level torn-off one) and deactivate the
	    # top-level torn off menu if there is one.

	    while 1 {
		set parent [winfo parent $menu]
		if {([winfo class $parent] != "Menu")
			|| ![winfo ismapped $parent]} {
		    break
		}
		$parent activate none
		$parent postcascade none
		if {![wm overrideredirect $parent]} {
		    break
		}
		set menu $parent
	    }
	    $menu unpost
	}
    }

    # Release grab, if any, and restore the previous grab, if there
    # was one.

    if {$menu != ""} {
	set grab [grab current $menu]
	if {$grab != ""} {
	    grab release $grab
	}
    }
    if {$tkPriv(oldGrab) != ""} {

	# Be careful restoring the old grab, since it's window may not
	# be visible anymore.

	catch {
	    if {$tkPriv(grabStatus) == "global"} {
		grab set -global $tkPriv(oldGrab)
	    } else {
		grab set $tkPriv(oldGrab)
	    }
	}
	set tkPriv(oldGrab) ""
    }
}

# tkMbMotion --
# This procedure handles mouse motion events inside menubuttons, and
# also outside menubuttons when a menubutton has a grab (e.g. when a
# menu selection operation is in progress).
#
# Arguments:
# w -			The name of the menubutton widget.
# upDown - 		"down" means button 1 is pressed, "up" means
#			it isn't.
# rootx, rooty -	Coordinates of mouse, in (virtual?) root window.

proc tkMbMotion {w upDown rootx rooty} {
    global tkPriv

    if {$tkPriv(inMenubutton) == $w} {
	return
    }
    set new [winfo containing $rootx $rooty]
    if {($new != $tkPriv(inMenubutton)) && (($new == "")
	    || ([winfo toplevel $new] == [winfo toplevel $w]))} {
	if {$tkPriv(inMenubutton) != ""} {
	    tkMbLeave $tkPriv(inMenubutton)
	}
	if {($new != "") && ([winfo class $new] == "Menubutton")
		&& ([$new cget -indicatoron] == 0)
		&& ([$w cget -indicatoron] == 0)} {
	    if {$upDown == "down"} {
		tkMbPost $new $rootx $rooty
	    } else {
		tkMbEnter $new
	    }
	}
    }
}

# tkMbButtonUp --
# This procedure is invoked to handle button 1 releases for menubuttons.
# If the release happens inside the menubutton then leave its menu
# posted with element 0 activated.  Otherwise, unpost the menu.
#
# Arguments:
# w -			The name of the menubutton widget.

proc tkMbButtonUp w {
    global tkPriv

    if  {($tkPriv(postedMb) == $w) && ($tkPriv(inMenubutton) == $w)} {
	tkMenuFirstEntry [$tkPriv(postedMb) cget -menu]
    } else {
	tkMenuUnpost {}
    }
}

# tkMenuMotion --
# This procedure is called to handle mouse motion events for menus.
# It does two things.  First, it resets the active element in the
# menu, if the mouse is over the menu.  Second, if a mouse button
# is down, it posts and unposts cascade entries to match the mouse
# position.
#
# Arguments:
# menu -		The menu window.
# y -			The y position of the mouse.
# state -		Modifier state (tells whether buttons are down).

proc tkMenuMotion {menu y state} {
    global tkPriv
    if {$menu == $tkPriv(window)} {
	$menu activate @$y
    }
    if {($state & 0x1f00) != 0} {
	$menu postcascade active
    }
}

# tkMenuButtonDown --
# Handles button presses in menus.  There are a couple of tricky things
# here:
# 1. Change the posted cascade entry (if any) to match the mouse position.
# 2. If there is a posted menubutton, must grab to the menubutton;  this
#    overrrides the implicit grab on button press, so that the menu
#    button can track mouse motions over other menubuttons and change
#    the posted menu.
# 3. If there's no posted menubutton (e.g. because we're a torn-off menu
#    or one of its descendants) must grab to the top-level menu so that
#    we can track mouse motions across the entire menu hierarchy.
#
# Arguments:
# menu -		The menu window.

proc tkMenuButtonDown menu {
    global tkPriv
    $menu postcascade active
    if {$tkPriv(postedMb) != ""} {
	grab -global $tkPriv(postedMb)
    } else {
	while {[wm overrideredirect $menu]
		&& ([winfo class [winfo parent $menu]] == "Menu")
		&& [winfo ismapped [winfo parent $menu]]} {
	    set menu [winfo parent $menu]
	}

	# Don't update grab information if the grab window isn't changing.
	# Otherwise, we'll get an error when we unpost the menus and
	# restore the grab, since the old grab window will not be viewable
	# anymore.

	if {$menu != [grab current $menu]} {
	    tkSaveGrabInfo $menu
	}

	# Must re-grab even if the grab window hasn't changed, in order
	# to release the implicit grab from the button press.

	grab -global $menu
    }
}

# tkMenuLeave --
# This procedure is invoked to handle Leave events for a menu.  It
# deactivates everything unless the active element is a cascade element
# and the mouse is now over the submenu.
#
# Arguments:
# menu -		The menu window.
# rootx, rooty -	Root coordinates of mouse.
# state -		Modifier state.

proc tkMenuLeave {menu rootx rooty state} {
    global tkPriv
    set tkPriv(window) {}
    if {[$menu index active] == "none"} {
	return
    }
    if {([$menu type active] == "cascade")
	    && ([winfo containing $rootx $rooty]
	    == [$menu entrycget active -menu])} {
	return
    }
    $menu activate none
}

# tkMenuInvoke --
# This procedure is invoked when button 1 is released over a menu.
# It invokes the appropriate menu action and unposts the menu if
# it came from a menubutton.
#
# Arguments:
# w -			Name of the menu widget.
# buttonRelease -	1 means this procedure is called because of
#			a button release;  0 means because of keystroke.

proc tkMenuInvoke {w buttonRelease} {
    global tkPriv

    if {$buttonRelease && ($tkPriv(window) == "")} {
	# Mouse was pressed over a menu without a menu button, then
	# dragged off the menu (possibly with a cascade posted) and
	# released.  Unpost everything and quit.

	$w postcascade none
	$w activate none
	tkMenuUnpost $w
	return
    }
    if {[$w type active] == "cascade"} {
	$w postcascade active
	set menu [$w entrycget active -menu]
	tkMenuFirstEntry $menu
    } elseif {[$w type active] == "tearoff"} {
	tkMenuUnpost $w
	tkTearOffMenu $w
    } else {
	tkMenuUnpost $w
	uplevel #0 [list $w invoke active]
    }
}

# tkMenuEscape --
# This procedure is invoked for the Cancel (or Escape) key.  It unposts
# the given menu and, if it is the top-level menu for a menu button,
# unposts the menu button as well.
#
# Arguments:
# menu -		Name of the menu window.

proc tkMenuEscape menu {
    if {[winfo class [winfo parent $menu]] != "Menu"} {
	tkMenuUnpost $menu
    } else {
	tkMenuLeftRight $menu -1
    }
}

# tkMenuLeftRight --
# This procedure is invoked to handle "left" and "right" traversal
# motions in menus.  It traverses to the next menu in a menu bar,
# or into or out of a cascaded menu.
#
# Arguments:
# menu -		The menu that received the keyboard
#			event.
# direction -		Direction in which to move: "left" or "right"

proc tkMenuLeftRight {menu direction} {
    global tkPriv

    # First handle traversals into and out of cascaded menus.

    if {$direction == "right"} {
	set count 1
	if {[$menu type active] == "cascade"} {
	    $menu postcascade active
	    set m2 [$menu entrycget active -menu]
	    if {$m2 != ""} {
		tkMenuFirstEntry $m2
	    }
	    return
	}
    } else {
	set count -1
	set m2 [winfo parent $menu]
	if {[winfo class $m2] == "Menu"} {
	    $menu activate none
	    focus $m2

	    # This code unposts any posted submenu in the parent.

	    set tmp [$m2 index active]
	    $m2 activate none
	    $m2 activate $tmp
	    return
	}
    }

    # Can't traverse into or out of a cascaded menu.  Go to the next
    # or previous menubutton, if that makes sense.

    set w $tkPriv(postedMb)
    if {$w == ""} {
	return
    }
    set buttons [winfo children [winfo parent $w]]
    set length [llength $buttons]
    set i [expr [lsearch -exact $buttons $w] + $count]
    while 1 {
	while {$i < 0} {
	    incr i $length
	}
	while {$i >= $length} {
	    incr i -$length
	}
	set mb [lindex $buttons $i]
	if {([winfo class $mb] == "Menubutton")
		&& ([$mb cget -state] != "disabled")
		&& ([$mb cget -menu] != "")
		&& ([[$mb cget -menu] index last] != "none")} {
	    break
	}
	if {$mb == $w} {
	    return
	}
	incr i $count
    }
    tkMbPost $mb
    tkMenuFirstEntry [$mb cget -menu]
}

# tkMenuNextEntry --
# Activate the next higher or lower entry in the posted menu,
# wrapping around at the ends.  Disabled entries are skipped.
#
# Arguments:
# menu -			Menu window that received the keystroke.
# count -			1 means go to the next lower entry,
#				-1 means go to the next higher entry.

proc tkMenuNextEntry {menu count} {
    global tkPriv
    if {[$menu index last] == "none"} {
	return
    }
    set length [expr [$menu index last]+1]
    set quitAfter $length
    set active [$menu index active]
    if {$active == "none"} {
	set i 0
    } else {
	set i [expr $active + $count]
    }
    while 1 {
	if {$quitAfter <= 0} {
	    # We've tried every entry in the menu.  Either there are
	    # none, or they're all disabled.  Just give up.

	    return
	}
	while {$i < 0} {
	    incr i $length
	}
	while {$i >= $length} {
	    incr i -$length
	}
	if {[catch {$menu entrycget $i -state} state] == 0} {
	    if {$state != "disabled"} {
		break
	    }
	}
	if {$i == $active} {
	    return
	}
	incr i $count
	incr quitAfter -1
    }
    $menu activate $i
    $menu postcascade $i
}

# tkMenuFind --
# This procedure searches the entire window hierarchy under w for
# a menubutton that isn't disabled and whose underlined character
# is "char".  It returns the name of that window, if found, or an
# empty string if no matching window was found.  If "char" is an
# empty string then the procedure returns the name of the first
# menubutton found that isn't disabled.
#
# Arguments:
# w -				Name of window where key was typed.
# char -			Underlined character to search for;
#				may be either upper or lower case, and
#				will match either upper or lower case.

proc tkMenuFind {w char} {
    global tkPriv
    set char [string tolower $char]

    foreach child [winfo child $w] {
	switch [winfo class $child] {
	    Menubutton {
		set char2 [string index [$child cget -text] \
			[$child cget -underline]]
		if {([string compare $char [string tolower $char2]] == 0)
			|| ($char == "")} {
		    if {[$child cget -state] != "disabled"} {
			return $child
		    }
		}
	    }
	    Frame {
		set match [tkMenuFind $child $char]
		if {$match != ""} {
		    return $match
		}
	    }
	}
    }
    return {}
}

# tkTraverseToMenu --
# This procedure implements keyboard traversal of menus.  Given an
# ASCII character "char", it looks for a menubutton with that character
# underlined.  If one is found, it posts the menubutton's menu
#
# Arguments:
# w -				Window in which the key was typed (selects
#				a toplevel window).
# char -			Character that selects a menu.  The case
#				is ignored.  If an empty string, nothing
#				happens.

proc tkTraverseToMenu {w char} {
    global tkPriv
    if {$char == ""} {
	return
    }
    while {[winfo class $w] == "Menu"} {
	if {$tkPriv(postedMb) == ""} {
	    return
	}
	set w [winfo parent $w]
    }
    set w [tkMenuFind [winfo toplevel $w] $char]
    if {$w != ""} {
	tkMbPost $w
	tkMenuFirstEntry [$w cget -menu]
    }
}

# tkFirstMenu --
# This procedure traverses to the first menubutton in the toplevel
# for a given window, and posts that menubutton's menu.
#
# Arguments:
# w -				Name of a window.  Selects which toplevel
#				to search for menubuttons.

proc tkFirstMenu w {
    set w [tkMenuFind [winfo toplevel $w] ""]
    if {$w != ""} {
	tkMbPost $w
	tkMenuFirstEntry [$w cget -menu]
    }
}

# tkTraverseWithinMenu
# This procedure implements keyboard traversal within a menu.  It
# searches for an entry in the menu that has "char" underlined.  If
# such an entry is found, it is invoked and the menu is unposted.
#
# Arguments:
# w -				The name of the menu widget.
# char -			The character to look for;  case is
#				ignored.  If the string is empty then
#				nothing happens.

proc tkTraverseWithinMenu {w char} {
    if {$char == ""} {
	return
    }
    set char [string tolower $char]
    set last [$w index last]
    if {$last == "none"} {
	return
    }
    for {set i 0} {$i <= $last} {incr i} {
	if [catch {set char2 [string index \
		[$w entrycget $i -label] \
		[$w entrycget $i -underline]]}] {
	    continue
	}
	if {[string compare $char [string tolower $char2]] == 0} {
	    if {[$w type $i] == "cascade"} {
		$w postcascade $i
		$w activate $i
		set m2 [$w entrycget $i -menu]
		if {$m2 != ""} {
		    tkMenuFirstEntry $m2
		}
	    } else {
		tkMenuUnpost $w
		uplevel #0 [list $w invoke $i]
	    }
	    return
	}
    }
}

# tkMenuFirstEntry --
# Given a menu, this procedure finds the first entry that isn't
# disabled or a tear-off or separator, and activates that entry.
# However, if there is already an active entry in the menu (e.g.,
# because of a previous call to tkPostOverPoint) then the active
# entry isn't changed.  This procedure also sets the input focus
# to the menu.
#
# Arguments:
# menu -		Name of the menu window (possibly empty).

proc tkMenuFirstEntry menu {
    if {$menu == ""} {
	return
    }
    focus $menu
    if {[$menu index active] != "none"} {
	return
    }
    set last [$menu index last]
    if {$last == "none"} {
	return
    }
    for {set i 0} {$i <= $last} {incr i} {
	if {([catch {set state [$menu entrycget $i -state]}] == 0)
		&& ($state != "disabled") && ([$menu type $i] != "tearoff")} {
	    $menu activate $i
	    return
	}
    }
}

# tkMenuFindName --
# Given a menu and a text string, return the index of the menu entry
# that displays the string as its label.  If there is no such entry,
# return an empty string.  This procedure is tricky because some names
# like "active" have a special meaning in menu commands, so we can't
# always use the "index" widget command.
#
# Arguments:
# menu -		Name of the menu widget.
# s -			String to look for.

proc tkMenuFindName {menu s} {
    set i ""
    if {![regexp {^active$|^last$|^none$|^[0-9]|^@} $s]} {
	catch {set i [$menu index $s]}
	return $i
    }
    set last [$menu index last]
    if {$last == "none"} {
	return
    }
    for {set i 0} {$i <= $last} {incr i} {
	if ![catch {$menu entrycget $i -label} label] {
	    if {$label == $s} {
		return $i
	    }
	}
    }
    return ""
}

# tkPostOverPoint --
# This procedure posts a given menu such that a given entry in the
# menu is centered over a given point in the root window.  It also
# activates the given entry.
#
# Arguments:
# menu -		Menu to post.
# x, y -		Root coordinates of point.
# entry -		Index of entry within menu to center over (x,y).
#			If omitted or specified as {}, then the menu's
#			upper-left corner goes at (x,y).

proc tkPostOverPoint {menu x y {entry {}}}  {
    if {$entry != {}} {
	if {$entry == [$menu index last]} {
	    incr y [expr -([$menu yposition $entry] \
		    + [winfo reqheight $menu])/2]
	} else {
	    incr y [expr -([$menu yposition $entry] \
		    + [$menu yposition [expr $entry+1]])/2]
	}
	incr x [expr -[winfo reqwidth $menu]/2]
    }
    $menu post $x $y
    if {($entry != {}) && ([$menu entrycget $entry -state] != "disabled")} {
	$menu activate $entry
    }
}

# tkSaveGrabInfo --
# Sets the variables tkPriv(oldGrab) and tkPriv(grabStatus) to record
# the state of any existing grab on the w's display.
#
# Arguments:
# w -			Name of a window;  used to select the display
#			whose grab information is to be recorded.

proc tkSaveGrabInfo w {
    global tkPriv
    set tkPriv(oldGrab) [grab current $w]
    if {$tkPriv(oldGrab) != ""} {
	set tkPriv(grabStatus) [grab status $tkPriv(oldGrab)]
    }
}

# tk_popup --
# This procedure pops up a menu and sets things up for traversing
# the menu and its submenus.
#
# Arguments:
# menu -		Name of the menu to be popped up.
# x, y -		Root coordinates at which to pop up the
#			menu.
# entry -		Index of a menu entry to center over (x,y).
#			If omitted or specified as {}, then menu's
#			upper-left corner goes at (x,y).

proc tk_popup {menu x y {entry {}}} {
    global tkPriv
    if {($tkPriv(popup) != "") || ($tkPriv(postedMb) != "")} {
	tkMenuUnpost {}
    }
    tkPostOverPoint $menu $x $y $entry
    tkSaveGrabInfo $menu
    grab -global $menu
    set tkPriv(popup) $menu
    set tkPriv(focus) [focus]
    focus $menu
}
