#!/bin/ksh -
#
# $OpenBSD: spell.ksh,v 1.3 2002/11/27 01:18:34 margarida Exp $
#
# Copyright (c) 2001 Todd C. Miller <Todd.Miller@courtesan.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
USAGE="usage: spell [-biltvx] [-d list] [-h spellhist] [-s stop] [+extra_list] [file ...]"

trap "rm -f $TMP $VTMP; exit 0" 0

# Use local word/stop lists if they exist
if [ -f $LOCAL_DICT ]; then
	DICT="$DICT $LOCAL_DICT"
fi
if [ -f $LOCAL_STOP ]; then
	STOP="$STOP $LOCAL_STOP"
fi

# getopts will treat +foo the same as -foo so we have to make a copy
# of the args and quit the loop when we find something starting with '+'
set -A argv $0 "$@"
while test "${argv[$OPTIND]#+}" = "${argv[$OPTIND]}" && \
    getopts "biltvxd:h:m:s:" c; do
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
	t)	DEROFF="detex"
		;;
	v)	VTMP=`mktemp /tmp/spell.XXXXXXXX` || exit 1
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
