#!/bin/sh
#
# $OpenBSD: ctfstrip,v 1.12 2019/10/15 10:27:25 mpi Exp $
#
# Copyright (c) 2017 Martin Pieuchot
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

set -o posix

cleanup() {
	rm -f ${TMPFILE}
	exit 1
}

usage() {
	echo "usage: $(basename $0) [-S] [-o outfile] file" >&2
	exit 1
}

trap "cleanup" 1 2 3 13 15

while getopts So: opt; do
	case $opt in
		S)	STRIPFLAG=-g;;
		o)	OUTFILE="$OPTARG";;
		\?)	usage;;
	esac
done

shift $((OPTIND - 1))

if [ $# -ne 1 ]; then
	usage
fi

INFILE="$1"
LABEL="unknown"
TMPFILE=$(mktemp /tmp/.ctf.XXXXXXXXXX)

# Extract kernel version
if [ -z "${INFILE##bsd*}" ]; then
	LABEL=`what "$INFILE" | sed -n '$s/^   //p'`
fi

# If ctfstrip was passed a file that lacks useful debug sections, ctfconv will
# fail.  So try to run ctfconv and silently fallback to plain strip(1) if that
# failed.
ctfconv -o ${TMPFILE} -l "${LABEL}" "${INFILE}" 2> /dev/null

if [ $? -eq 0 ]; then
	objcopy  ${STRIPFLAG} \
		--add-section .SUNW_ctf=${TMPFILE} "${INFILE}" ${OUTFILE}

	# Also add CTF data to the debug kernel
	if [ -z "${INFILE##bsd.gdb}" ]; then
		objcopy --add-section .SUNW_ctf=${TMPFILE} "${INFILE}"
	fi
else
	strip ${STRIPFLAG} ${OUTFILE:+"-o${OUTFILE}"} "${INFILE}"
fi

rm -f ${TMPFILE}
