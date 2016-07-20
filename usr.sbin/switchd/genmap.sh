#!/bin/sh
# $OpenBSD: genmap.sh,v 1.2 2016/07/20 19:57:54 reyk Exp $

# Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
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

TOKEN=""
MAPFILE=""
INPUT=""
HEADER=""

args=`getopt i:o:h:t:m: $*`

if [ $? -ne 0 ]; then
	echo "usage: $0 -i input -h header -t token [-m mapfile]"
	exit 1
fi

set -- $args
while [ $# -ne 0 ]; do
	case "$1" in
	-i)
		INPUT="$2"; shift; shift;
		;;
	-h)
		HEADER="$2"; shift; shift;
		;;
	-t)
		TOKEN="$2"; shift; shift;
		;;
	-m)
		MAPFILE="$2"; shift; shift;
		;;
	--)
		shift;
		break
		;;
	esac
done

if [ -z "$MAPFILE" ]; then
	MAPFILE=$INPUT
fi

TOK=$(echo ${TOKEN} | tr "[:lower:]" "[:upper:]")
tok=$(echo ${TOKEN} | tr "[:upper:]" "[:lower:]")
INC="#include ${HEADER}"

MAP=$(grep "struct constmap ${tok}_" $MAPFILE |
	sed -Ee "s/.*${tok}_(.+)_map.*/\1/g")

# Print license/copyright notice and headers
cat <<EOF
/* Automatically generated from $1, do not edit */
EOF
sed -n '1,/^ \*\//p' $INPUT
cat <<EOF

#include <sys/types.h>

#include "types.h"
${INC}

EOF

for i in $MAP; do
	lower=$(echo $i | tr "[:upper:]" "[:lower:]")
	upper=$(echo $i | tr "[:lower:]" "[:upper:]")

	echo "struct constmap ${tok}_${lower}_map[] = {"

	X="${TOK}_${upper}_"
	grep "$X" $INPUT | grep -v '\\' | sed -Ee \
	    "s/#define.*${X}([^[:blank:]]+).*\/\* (.+) \*\/$\
/	{ ${X}\1, \"\1\", \"\2\" },/" | grep -v '\#define'

	echo "	{ 0 }"
	echo "};"
done
