#!/bin/sh
#
# $OpenBSD: run.sh,v 1.1 2018/05/09 19:34:53 anton Exp $
#
# Copyright (c) 2018 Anton Lindqvist <anton@openbsd.org>
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

set -eu

usage() {
	echo "usage: sh run.sh -a addr -n rebound-ns -r rebound file" 1>&2
	exit 1
}

# asserteq WANT GOT [MESSAGE]
asserteq() {
	[ "$1" = "$2" ] && return 0

	printf 'FAIL:\n\tWANT:\t"%s"\n\tGOT:\t"%s"\n' "$1" "$2"
	[ $# -eq 3 ] && printf '\tREASON:\t"%s"\n' "$3"
	FAIL=1
}

atexit() {
	local _err=$?

	# Kill daemons.
	pkillw "^${REBOUND}" "^${NS}"

	# Cleanup temporary files.
	rm -f $@

	if [ $_err -ne 0 ] || [ $FAIL -ne 0 ]; then
		exit 1
	else
		exit 0
	fi
}

pkillw() {
	local _pat _sig

	for _pat; do
		_sig=TERM
		while pgrep -f "$_pat" >/dev/null 2>&1; do
			pkill "-${_sig}" -f "$_pat"
			sleep .5
			_sig=KILL
		done
	done
}

resolve() {
	host $@ | sed -n '$p'
}

ENV='env MALLOC_OPTIONS=S'
FAIL=0
NADDR=0
NS=
REBOUND=

while getopts "a:n:r:" opt; do
	case "$opt" in
	a)	NADDR=$((NADDR + 1))
		eval "ADDR${NADDR}=${OPTARG}"
		;;
	n)	NS=$OPTARG;;
	r)	REBOUND=$OPTARG;;
	*)	usage;;
	esac
done
shift $((OPTIND - 1))
([ $# -ne 1 ] || [ $NADDR -eq 0 ] || [ -z "$NS" ] || [ -z "$REBOUND" ]) && usage

CONF=$(mktemp -t rebound.XXXXXX)
trap 'atexit $CONF' EXIT

. $1
