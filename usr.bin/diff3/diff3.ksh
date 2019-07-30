#!/bin/ksh -
#
# $OpenBSD: diff3.ksh,v 1.4 2006/02/22 22:35:11 jmc Exp $
#
# Copyright (c) 2003 Todd C. Miller <Todd.Miller@courtesan.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Sponsored in part by the Defense Advanced Research Projects
# Agency (DARPA) and Air Force Research Laboratory, Air Force
# Materiel Command, USAF, under agreement number F39502-99-1-0512.
#

set -o posix		# set POSIX mode to prevent +foo in getopts
OPTIND=1		# force getopts to reset itself

export PATH=/bin:/usr/bin
diff3prog=/usr/libexec/diff3prog
USAGE="usage: diff3 [-3aEeXx] file1 file2 file3"

# Pull out any command line flags (some for diff, some for diff3)
dflags=
d3flags=
while getopts "aeExX3" c; do
	case "$c" in
		a)
			dflags="$dflags -$c"
			;;
		e|E|x|X|3)
			d3flags="-$c"
			;;
		*)
			echo "$USAGE" 1>&2
			exit 1
			;;
	esac
done
shift $(( $OPTIND - 1 ))

if [ $# -lt 3 ]; then
	echo "$USAGE" 1>&2
	exit 1
fi

TMP1=`mktemp -t d3a.XXXXXXXXXX` || exit 1
TMP2=`mktemp -t d3b.XXXXXXXXXX`
if [ $? -ne 0 ]; then
	rm -f $TMP1
	exit 1
fi
trap "/bin/rm -f $TMP1 $TMP2" 0 1 2 13 15
diff $dflags -- $1 $3 > $TMP1
diff $dflags -- $2 $3 > $TMP2
$diff3prog $d3flags -- $TMP1 $TMP2 $1 $2 $3
exit $?
