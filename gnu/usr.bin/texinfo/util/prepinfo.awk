#! /usr/local/bin/gawk -f

# prepinfo.awk --- fix node lines and menus
#
# Copyright, 1998, Arnold Robbins, arnold@gnu.org
#
# PREPINFO is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# PREPINFO is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

BEGIN	\
{
	# manifest constants
	TRUE = 1
	FALSE = 0

	# Levels at which different nodes can be
	Level["@top"] =	0
	Level["@appendix"] = 1
	Level["@chapter"] = 1
	Level["@majorheading"] = 1
	Level["@unnumbered"] = 1
	Level["@appendixsec"] = 2
	Level["@heading"] = 2
	Level["@section"] = 2
	Level["@unnumberedsec"] = 2
	Level["@unnumberedsubsec"] = 3
	Level["@appendixsubsec"] = 3
	Level["@subheading"] = 3
	Level["@subsection"] = 3
	Level["@appendixsubsubsec"] = 4
	Level["@subsubheading"] = 4
	Level["@subsubsection"] = 4
	Level["@unnumberedsubsubsec"] = 4

	# Length of menus
	Menumargin = 78

	# Length of menu item
	Min_menitem_length = 29

	# insure that we were called correctly
	if (ARGC != 2) {
		printf("usage: %s texinfo-file\n", ARGV[0]) > "/dev/stderr"
		exit 1
	}

	# Arrange for two passes over input file
	Pass = 1
	ARGV[2] = "Pass=2"
	ARGV[3] = ARGV[1]
	ARGC = 4
	Lastlevel = -1

	# Initialize stacks
	Up[-1] = "(dir)"
	Prev[0] = "(dir)"

	if (Debug == "args") {
		for (i = 0; i < ARGC; i++)
			printf("ARGV[%d] = %s\n", i, ARGV[i]) > "/dev/stderr"
	}
}

$1 == "@node"	\
{
	Name = getnodename($0)
	Nodeseen = TRUE

	if ((l = length(Name)) > Maxlen)
		Maxlen = l

	if (Debug == "nodenames")
		printf("Name = %s\n", Name) > "/dev/stderr"

	if (Pass == 1)
		next
}

Pass == 1 && /^@c(omment)?[ \t]+fakenode/ \
{
	if (Debug == "fakenodes")
		printf("fakenode at %d\n", FNR) > "/dev/stderr"
	Fakenode = TRUE
	next
}

Pass == 1 && ($1 in Level)	\
{
	# skip fake nodes --- titles without associated @node lines
	if (Fakenode) {
		if (Debug == "fakenodes")
			printf("%s at %d is a fakenode\n", $1, FNR) > "/dev/stderr"
		Fakenode = FALSE
		next
	}

	if (Debug == "titles")
		printf("Processing %s: Name = %s\n", $1, Name) > "/dev/stderr"

	# save type
	type = $1

	if (! Nodeseen) {
		err_prefix()
		printf("%s line with no @node or fakenode line\n",
			type) > "/dev/stderr"
		Badheading[FNR] = 1
		# ??? used ???
		next
	} else
		Nodeseen = FALSE	# reset it

	# Squirrel away the info
	levelnum = Level[type]
	Node[Name ".level"] = levelnum
	Node[Name ".name"] = Name
	if (Debug == "titles") {
		printf("Node[%s\".level\"] = %s\n", Name, Node[Name ".level"]) > "/dev/stderr"
		printf("Node[%s\".name\"] = %s\n", Name, Node[Name ".name"]) > "/dev/stderr"
	}

	if (levelnum == Lastlevel) {	# e.g., two sections in a row
		Node[Name ".up"] = Up[levelnum - 1]
		if (levelnum in Prev) {
			Node[Prev[levelnum] ".next"] = Name
			Node[Name ".prev"] = Prev[levelnum]
		}
		Prev[levelnum] = Name
		Up[levelnum] = Name	# ???
	} else if (levelnum < Lastlevel) {	# section, now chapter
		Lastlevel = levelnum
		Node[Name ".up"] = Up[levelnum - 1]
		if (levelnum in Prev) {
			Node[Name ".prev"] = Prev[levelnum]
			Node[Prev[levelnum] ".next"] = Name
		}
		Prev[levelnum] = Name
		Up[levelnum] = Name
	} else {		# chapter, now section, levelnum > Lastlevel
		Node[Name ".up"] = Up[levelnum - 1]
		Node[Up[Lastlevel] ".child"] = Name
		Up[levelnum] = Name
		Prev[levelnum] = Name
		Lastlevel = levelnum
	}

	# For master menu
	if (Level[$1] >= 2)
		List[++Sequence] = Name

	if (Debug == "titles") {
		printf("Node[%s\".prev\"] = %s\n", Name, Node[Name ".prev"]) > "/dev/stderr"
		printf("Node[%s\".up\"] = %s\n", Name, Node[Name ".up"]) > "/dev/stderr"
		printf("Node[%s\".child\"] = %s\n", Name, Node[Name ".child"]) > "/dev/stderr"
	}
}

Pass == 2 && Debug == "dumptitles" && FNR <= 1	\
{
	for (i in Node)
		printf("Node[%s] = %s\n", i, Node[i]) | "sort 1>&2"
	close("sort 1>&2")
}

/^@menu/ && Pass == 1, /^@end[ \t]+menu/ && Pass == 1	\
{
	if (/^@menu/ || /^@end[ \t]+menu/)
		next

#	if (Debug == "menu")
#		printf("processing: %s\n", $0) > "/dev/stderr"

	if (/^\*/) {
		if (In_menitem) {	# file away info from previousline
			Node[node ".mendesc"] = desc
			Node[node ".longdesc"] = longdesc
			if (Debug == "mendesc") {
				printf("Node[%s.mendesc] = %s\n",
					node, Node[node ".mendesc"]) > "/dev/stderr"
				printf("Node[%s.longdesc] = %s\n",
					node, Node[node ".longdesc"]) > "/dev/stderr"
			}
		}
		In_menitem = TRUE

		# pull apart menu item
		$1 = ""		# nuke ``*''
		$0 = $0		# reparse line
		i1 = index($0, ":")
		if (i1 <= 0) {
			err_prefix()
			printf("badly formed menu item") > "/dev/stderr"
			next
		}
		if (substr($0, i1+1, 1) != ":") { # desc: node.  long desc
			i2 = index($0, ".")
			if (i2 <= 0) {
				err_prefix()
				printf("badly formed menu item") > "/dev/stderr"
				next
			}
			desc = substr($0, 1, i1 - 1)
			sub(/^[ \t]+/, "", node)
			sub(/[ \t]+$/, "", node)
			longdesc = substr($0, i2 + 1)
		} else {	# nodname:: long desc
			desc = ""
			node = substr($0, 1, i1 - 1)
			sub(/^[ \t]+/, "", node)
			sub(/[ \t]+$/, "", node)
			longdesc = substr($0, i1 + 2)
		}
	} else if (In_menitem) {	# continuation line
		longdesc = longdesc " " $0
	} else
		In_menitem = FALSE

	Node[node ".mendesc"] = desc
	Node[node ".longdesc"] = longdesc
	if (Debug == "mendesc") {
		printf("Node[%s.mendesc] = %s\n",
			node, Node[node ".mendesc"]) > "/dev/stderr"
		printf("Node[%s.longdesc] = %s\n",
			node, Node[node ".longdesc"]) > "/dev/stderr"
	}

	if (Debug == "menu")
		printf("Menu:: Name %s: desc %s: longdesc %s\n",
			node, desc, longdesc) > "/dev/stderr"
}

function err_prefix()
{
	printf("%s: %s: %d: ", ARGV[0], FILENAME, FNR) > "/dev/stderr"
}

function getnodename(str)
{
	sub(/@node[ \t]+/, "", str)
	sub(/,.*/, "", str)
	if (Debug == "nodenames")
		printf("getnodename: return %s\n", str) > "/dev/stderr"
	return str
}

Pass == 2 && /^@node/	\
{
	Name = getnodename($0)

	# Top node is special. It's next is the first child
	n = Node[Name ".next"]
	if (Node[Name ".level"] == 0 && n == "")
		n = Node[Name ".child"]

	printf("@node %s, %s, %s, %s\n", Name, n,
		Node[Name ".prev"] ? Node[Name ".prev"] : Node[Name ".up"],
		Node[Name ".up"])
	next
}

Pass == 2 && /^@menu/	\
{
	# First, nuke current contents of menu
	do {
		if ((getline) <= 0) {
			err_prefix()
			printf("unexpected EOF inside menu\n") > "/dev/stderr"
			exit 1
		}
	} while (! /^@end[ \t]+menu/)

	# next, compute maximum length of a node name
	max = 0
	for (n = Node[Name ".child"]; (n ".next") in Node; n = Node[n ".next"]) {
		if ((n ".desc") in Node)
			s = Node[n ".desc"] ": " n "."
		else
			s = n "::"
		l = length(s)
		if (l > max)
			max = l
	}
	if (max < Min_menitem_length)
		max = Min_menitem_length

	# now dump the menu
	print "@menu"

	for (n = Node[Name ".child"]; (n ".next") in Node; n = Node[n ".next"]) {
		print_menuitem(n, max)
	}
	print_menuitem(n, max)

	if (Name == "Top") {	# Master Menu
		if (Maxlen < Min_menitem_length)
			Maxlen = Min_menitem_length
		print ""
		for (i = 1; i <= Sequence; i++)
			print_menuitem(List[i], Maxlen)
		print ""
	}
	print "@end menu"
	next
}

Pass == 2	# print


function print_menuitem(n, max,		nodesc, i, dwords, count, p)
{
	nodesc = FALSE
	if (! ((n ".longdesc") in Node)) {
		err_prefix()
		printf("warning: %s: no long description\n", n) > "/dev/stderr"
		nodesc = TRUE
	} else {
		for (i in dwords)
			delete dwords[i]
		count = split(Node[n ".longdesc"], dwords, "[ \t\n]+")
	}
	if ((n ".desc") in Node)
		s = Node[n ".desc"] ": " n "."
	else
		s = n "::"
	printf("* %-*s", max, s)

	if (Debug == "mendescitem")
		printf("<* %-*s>\n", max, s) > "/dev/stderr"

	p = max + 2
	if (! nodesc) {
		for (i = 1; i <= count; i++) {
			l = length(dwords[i])
			if (l == 0)
				continue
			if (p + l + 1 > Menumargin) {
				printf("\n%*s", max + 2, " ")
				p = max + 2
			}
			printf(" %s", dwords[i])
			p += l + 1
		}
	}
	print ""
}
