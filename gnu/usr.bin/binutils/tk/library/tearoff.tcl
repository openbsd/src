# tearoff.tcl --
#
# This file contains procedures that implement tear-off menus.
#
# SCCS: @(#) tearoff.tcl 1.9 96/02/23 15:31:30
#
# Copyright (c) 1994 The Regents of the University of California.
# Copyright (c) 1994-1995 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

# tkTearoffMenu --
# Given the name of a menu, this procedure creates a torn-off menu
# that is identical to the given menu (including nested submenus).
# The new torn-off menu exists as a toplevel window managed by the
# window manager.  The return value is the name of the new menu.
#
# Arguments:
# w -			The menu to be torn-off (duplicated).

proc tkTearOffMenu w {
    # Find a unique name to use for the torn-off menu.  Find the first
    # ancestor of w that is a toplevel but not a menu, and use this as
    # the parent of the new menu.  This guarantees that the torn off
    # menu will be on the same screen as the original menu.  By making
    # it a child of the ancestor, rather than a child of the menu, it
    # can continue to live even if the menu is deleted;  it will go
    # away when the toplevel goes away.

    set parent [winfo parent $w]
    while {([winfo toplevel $parent] != $parent)
	    || ([winfo class $parent] == "Menu")} {
	set parent [winfo parent $parent]
    }
    if {$parent == "."} {
	set parent ""
    }
    for {set i 1} 1 {incr i} {
	set menu $parent.tearoff$i
	if ![winfo exists $menu] {
	    break
	}
    }

    tkMenuDup $w $menu
    $menu configure -transient 0

    # Pick a title for the new menu by looking at the parent of the
    # original: if the parent is a menu, then use the text of the active
    # entry.  If it's a menubutton then use its text.

    set parent [winfo parent $w]
    switch [winfo class $parent] {
	Menubutton {
	    wm title $menu [$parent cget -text]
	}
	Menu {
	    wm title $menu [$parent entrycget active -label]
	}
    }

    $menu configure -tearoff 0
    $menu post [winfo x $w] [winfo y $w]

    # Set tkPriv(focus) on entry:  otherwise the focus will get lost
    # after keyboard invocation of a sub-menu (it will stay on the
    # submenu).

    bind $menu <Enter> {
	set tkPriv(focus) %W
    }

    # If there is a -tearoffcommand option for the menu, invoke it
    # now.

    set cmd [$w cget -tearoffcommand]
    if {$cmd != ""} {
	uplevel #0 $cmd $w $menu
    }
}

# tkMenuDup --
# Given a menu (hierarchy), create a duplicate menu (hierarchy)
# in a given window.
#
# Arguments:
# src -			Source window.  Must be a menu.  It and its
#			menu descendants will be duplicated at dst.
# dst -			Name to use for topmost menu in duplicate
#			hierarchy.

proc tkMenuDup {src dst} {
    set cmd "menu $dst"
    foreach option [$src configure] {
	if {[llength $option] == 2} {
	    continue
	}
	lappend cmd [lindex $option 0] [lindex $option 4]
    }
    eval $cmd
    set last [$src index last]
    if {$last == "none"} {
	return
    }
    for {set i [$src cget -tearoff]} {$i <= $last} {incr i} {
	set cmd "$dst add [$src type $i]"
	foreach option [$src entryconfigure $i]  {
	    lappend cmd [lindex $option 0] [lindex $option 4]
	}
	eval $cmd
	if {[$src type $i] == "cascade"} {
	    tkMenuDup [$src entrycget $i -menu] $dst.m$i
	    $dst entryconfigure $i -menu $dst.m$i
	}
    }

    # Duplicate the binding tags and bindings from the source menu.

    regsub -all . $src {\\&} quotedSrc
    regsub -all . $dst {\\&} quotedDst
    regsub -all $quotedSrc [bindtags $src] $dst x
    bindtags $dst $x
    foreach event [bind $src] {
	regsub -all $quotedSrc [bind $src $event] $dst x
	bind $dst $event $x
    }
}
