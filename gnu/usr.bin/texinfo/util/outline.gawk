#! /usr/local/bin/gawk -f

# texi.outline ---  produce an outline from a texinfo source file
# 
# Copyright (C) 1998 Arnold David Robbins (arnold@gnu.org)
# 
# TEXI.OUTLINE is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# TEXI.OUTLINE is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

# NOTE:
#	This program uses gensub(), which is specific to gawk.
#	With some work (split, substr, etc), it could be made to work
#	on other awks, but it's not worth the trouble for me.

BEGIN	\
{
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

	# insure that we were called correctly
	if (ARGC != 2) {
		printf("usage: %s texinfo-file\n", ARGV[0]) > "/dev/stderr"
		exit 1
	}

	# init header counters
	app_letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	app_h = 0
	l1_h = l2_h = l3_h = l4_h = 0
}

# skip lines we're not interested in
/^[^@]/	|| ! ($1 in Level)	{ next }

Level[$1] == 1	{
	if ($1 !~ /^@unnumbered/ || $1 !~ /heading/)
		l1_h++
	l2_h = l3_h = l4_h = 0
	Ntabs = 0
	Number = makenumber($1)
	Title = maketitle($0)
	print_title()
}

Level[$1] == 2	{
	l2_h++
	l3_h = l4_h = 0
	Ntabs = 1
	Number = makenumber($1)
	Title = maketitle($0)
	print_title()
}

Level[$1] == 3	{
	l3_h++
	l4_h = 0
	Ntabs = 2
	Number = makenumber($1)
	Title = maketitle($0)
	print_title()
}

Level[$1] == 4	{
	l4_h++
	Ntabs = 3
	Number = makenumber($1)
	Title = maketitle($0)
	print_title()
}

# maketitle --- extract title

function maketitle(str,		text)
{
	$1 = ""		# clobber section keyword
	text = $0
	gsub(/^[ \t]*/, "", text)
	text = gensub(/@[a-z]+{/, "", "g", text)
	text = gensub(/([^@])}/, "\\1", "g", text)
	return text
}

# print_title --- print the title

function print_title(	i)
{
	for (i = 1; i <= Ntabs; i++)
		printf "\t"
	printf("%s %s\n", Number, Title)
}

# makenumber --- construct a heading number from levels and section command

function makenumber(command,	result, lev1)
{
	result = ""
	if (command ~ /^@appendix/) {
		if (Level[command] == 1)
			app_h++

		lev1 = substr(app_letters, app_h, 1)
	} else if (command ~ /^@unnumbered/ || command ~ /heading/) {
		lev1 = "(unnumbered)"
	} else
		lev1 = l1_h ""

	result = lev1 "."
	if (l2_h > 0) {
		result = result l2_h "."
		if (l3_h > 0) {
			result = result l3_h "."
			if (l4_h > 0) {
				result = result l4_h "."
			}
		}
	}
	return result
}
