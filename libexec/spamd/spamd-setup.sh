#!/bin/sh

#	$OpenBSD: spamd-setup.sh,v 1.7 2003/02/14 05:51:57 jason Exp $
#
# Copyright (c) 2002 Theo de Raadt.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

usage() {
	echo "usage: spamd-setup [-s12] [-f file] [-w file]";
	exit 1
}

case $# in
0)	usage
	;;
esac

filter() {
	cut -f1 -d' '
}

fetch() {
	ftp -V -o - "$1"
}

R=`mktemp /tmp/_spamdXXXXXX` || exit 1
W=`mktemp /tmp/_spamwXXXXXX` || {
	rm -f ${R}
	exit 1
}
trap "rm -f $R; exit 0" 0
trap "rm -f $R; exit 1" 1 2 3 13 15

while :
	do case "$1" in
	-s|-1)
		fetch http://www.spews.org/spews_list_level1.txt | filter >> $R
		;;
	-2)
		fetch http://www.spews.org/spews_list_level2.txt | filter >> $R
		;;
	-f)
		cat $2 | filter >> $R
		shift
		;;
	-w)
		cat $2 | filter >> $W
		shift
		;;
	*)
		break
		;;
	esac
	shift
done

if [ $# != 0 ]; then
	usage
fi

# knock out whitelist here

pfctl -t spamd -T replace -f $R
pfctl -t spamd -T delete -f $W

exit 0
