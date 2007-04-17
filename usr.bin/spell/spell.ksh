#!/bin/ksh -
#
# $OpenBSD: spell.ksh,v 1.11 2007/04/17 16:56:23 millert Exp $
#
# Copyright (c) 2001, 2003 Todd C. Miller <Todd.Miller@courtesan.com>
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
SPELLPROG=/usr/libexec/spellprog
DICT=/usr/share/dict/words
LOCAL_DICT=/usr/local/share/dict/words
STOP=/usr/share/dict/stop
LOCAL_STOP=/usr/local/share/dict/stop
AMERICAN=/usr/share/dict/american
BRITISH=/usr/share/dict/british
LANG=$AMERICAN
STOP_LANG=$BRITISH
EXTRA=
FLAGS=
DEROFF="deroff -w"
HISTFILE=
TMP=`mktemp /tmp/spell.XXXXXXXX` || exit 1
VTMP=
USAGE="usage: spell [-biltvx] [-d list] [-h spellhist] [-m a | e | l | m | s]\n\t[-s list] [+extra_list] [file ...]"

set -o posix		# set POSIX mode to prevent +foo in getopts
OPTIND=1		# force getopts to reset itself

trap "rm -f $TMP $VTMP; exit 0" 0 1 2 15

# Use local word/stop lists if they exist
if [ -f $LOCAL_DICT ]; then
	DICT="$DICT $LOCAL_DICT"
fi
if [ -f $LOCAL_STOP ]; then
	STOP="$STOP $LOCAL_STOP"
fi

while getopts "biltvxd:h:m:s:" c; do
	case $c in
	b)	LANG=$BRITISH
		STOP_LANG=$AMERICAN
		FLAGS[${#FLAGS[@]}]="-b"
		;;
	i)	DEROFF="$DEROFF -i"
		;;
	l)	DEROFF="delatex"
		;;
	m)	DEROFF="$DEROFF -m $OPTARG"
		;;
	t)	DEROFF="detex -w"
		;;
	v)	VTMP=`mktemp /tmp/spell.XXXXXXXX` || {
			rm -f ${TMP}
			exit 1
		}
		FLAGS[${#FLAGS[@]}]="-v"
		FLAGS[${#FLAGS[@]}]="-o"
		FLAGS[${#FLAGS[@]}]="$VTMP"
		;;
	x)	FLAGS[${#FLAGS[@]}]="-x"
		;;
	d)	DICT="$OPTARG"
		LANG=
		;;
	s)	STOP="$OPTARG"
		STOP_LANG=
		LOCAL_STOP=
		;;
	h)	HISTFILE="$OPTARG"
		;;
	*)	echo "$USAGE" 1>&2
		exit 1
		;;
	esac
done
shift $(( $OPTIND - 1 ))

while test $# -ne 0; do
	case "$1" in
		+*)	EXTRA="$EXTRA ${1#+}"
			shift
			;;
		*)	break
			;;
	esac
done

# Any parameters left are files to be checked, pass them to deroff
DEROFF="$DEROFF $@"

if [ -n "$HISTFILE" ]; then
	$DEROFF | sort -u | $SPELLPROG -o $TMP $STOP $STOP_LANG | \
	    $SPELLPROG ${FLAGS[*]} $DICT $LANG $EXTRA | sort -u -k1f - $TMP | \
	    tee -a $HISTFILE
	who -m >> $HISTFILE
else
	$DEROFF | sort -u | $SPELLPROG -o $TMP $STOP $STOP_LANG | \
	    $SPELLPROG ${FLAGS[*]} $DICT $LANG $EXTRA | sort -u -k1f - $TMP
fi

if [ -n "$VTMP" ]; then
	sort -u -k2f -k1 $VTMP
fi

exit 0
