#! /usr/local/bin/gawk -f

# fixref.awk --- fix xrefs in texinfo documents
# Copyright, 1991, Arnold David Robbins, arnold@skeeve.atl.ga.us
# Copyright, 1998, Arnold David Robbins, arnold@gnu.org

# FIXREF is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# FIXREF is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

# Updated: Jul 21  1992	--- change unknown
# Updated: Jul 18  1997 --- bug fix

# usage:		gawk -f fixref.awk input-file > output-file
# or if you have #!:	fixref.awk input-file > output-file

# Limitations:
#	1. no more than one cross reference on a line
#	2. cross references may not cross a newline

BEGIN	\
{
	# we make two passes over the file.  To do that we artificially
	# tweak the argument vector to do a variable assignment

	if (ARGC != 2) {
		printf("usage: %s texinfo-file\n", ARGV[0]) > "/dev/stderr"
		exit 1
	}
	ARGV[2] = "pass=2"
	ARGV[3] = ARGV[1]
	ARGC = 4

	# examine paragraphs
	RS = ""

	heading = "@(chapter|appendix|unnumbered|(appendix(sec|subsec|subsubsec))|section|subsection|subsubsection|unnumberedsec|heading|top)"

	pass = 1

	# put space between paragraphs on output
	ORS = "\n\n"
}

pass == 1 && NF == 0	{ next }

# pass == 1 && /@node/	\
# bug fix 7/18/96
pass == 1 && /^@node/	\
{
	lname = name = ""
	n = split($0, lines, "\n")
	for (i = 1; i <= n; i++) {
		if (lines[i] ~ ("^" heading)) {
			sub(heading, "", lines[i])
			sub(/^[ \t]*/, "", lines[i])
			lname = lines[i]
#			printf "long name is '%s'\n", lines[i]
		} else if (lines[i] ~ /@node/) {
			sub(/@node[ \t]*/, "", lines[i])
			sub(/[ \t]*,.*$/, "", lines[i])
			name = lines[i]
#			printf "node name is '%s'\n", lines[i]
		}
	}
	if (name && lname)
		names[name] = lname
	else if (lname)
		printf("node name for %s missing!\n", lname) > "/dev/stderr"
	else
		printf("long name for %s missing!\n", name) > "/dev/stderr"

	if (name ~ /:/)
		printf("node `%s' contains a `:'\n", name) > "/dev/stderr"

	if (lname) {
		if (lname ~ /:/)
			printf("name `%s' contains a `:'\n", lname) > "/dev/stderr"
		else if (lname ~ /,/) {
			printf("name `%s' contains a `,'\n", lname) > "/dev/stderr"
			gsub(/,/, " ", lname)
			names[name] = lname	# added 7/18/97
		}
	}
}

pass == 2 && /@(x|px)?ref{/	\
{
	# split the paragraph into lines
	# write them out one by one after fixing them
	n = split($0, lines, "\n")
	for (i = 1; i <= n; i++)
		if (lines[i] ~ /@(x|px)?ref{/) {
			res = updateref(lines[i])
			printf "%s\n", res
		} else
			printf "%s\n", lines[i]

	printf "\n"	# avoid ORS
	next
}

function updateref(orig,	refkind, line)
{
	line = orig	# work on a copy

	# find the beginning of the reference
	match(line, "@(x|px)?ref{")
	refkind = substr(line, RSTART, RLENGTH)

	# pull out just the node name
	sub(/.*ref{/, "", line)
	sub(/}.*$/, "", line)
	sub(/,.*/, "", line)

#	debugging
#	printf("found ref to node '%s'\n", line) > "/dev/stderr"

	# If the node name and the section name are the same
	# we don't want to bother doing this.

	if (! (line in names))	# sanity checking
		printf("no long name for %s\n", line) > "/dev/stderr"
	else if (names[line] != line && names[line] !~ /[:,]/) {
		# build up new ref
		newref = refkind line ", ," names[line] "}"
		pat = refkind line "[^}]*}"

		sub(pat, newref, orig)
	}

	return orig
}

pass == 2	{ print }
