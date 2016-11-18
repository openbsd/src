#!/bin/sh
# $OpenBSD: genmap.sh,v 1.6 2016/11/18 16:49:35 reyk Exp $

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
DESCR=0

args=`getopt di:o:h:t:m: $*`

if [ $? -ne 0 ]; then
	echo "usage: $0 [-d] -i input -h header -t token [-m mapfile]"
	exit 1
fi

set -- $args
while [ $# -ne 0 ]; do
	case "$1" in
	-d)
		DESCR=1; shift;
		;;
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
FILE=$(basename ${INPUT})
MAP=$(grep "struct constmap ${tok}_" $MAPFILE |
	sed -Ee "s/.*${tok}_(.+)_map.*/\1/g")

# Print license/copyright notice and headers
cat <<EOF
/* Automatically generated from ${FILE}, do not edit */
EOF
sed -n '1,/^ \*\//p' $INPUT
cat <<EOF

#include <sys/types.h>
${INC}
#include "ofp_map.h"

EOF

for i in $MAP; do
	lower=$(echo $i | tr "[:upper:]" "[:lower:]")
	upper=$(echo $i | tr "[:lower:]" "[:upper:]")

	echo "struct constmap ${tok}_${lower}_map[] = {"

	X="${TOK}_${upper}_"

	if [ $DESCR = 1 ]; then
		# with the description field
		grep "$X" $INPUT | grep -v '\\' | sed -Ee \
		    "s/#define.*${X}([^[:blank:]]+).*\/\* (.+) \*\/$\
/	{ ${X}\1, \"\1\", \"\2\" },/" | grep -v '\#define'
	else
		# without the description field
		grep "$X" $INPUT | grep -v '\\' | sed -Ee \
		    "s/#define.*${X}([^[:blank:]]+).*\/\* .+ \*\/$\
/	{ ${X}\1, \"\1\" },/" | grep -v '\#define'
	fi

	echo "	{ 0 }"
	echo "};"
done
