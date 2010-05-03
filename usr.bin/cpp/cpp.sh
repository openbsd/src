#!/bin/sh
#	$OpenBSD: cpp.sh,v 1.8 2010/05/03 18:34:01 drahn Exp $

#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# This code is derived from software contributed to Berkeley by
# the Systems Programming Group of the University of Utah Computer
# Science Department.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)usr.bin.cpp.sh	6.5 (Berkeley) 4/1/91
#
# Transitional front end to CCCP to make it behave like (Reiser) CCP:
#	specifies -traditional
#	doesn't search gcc-include
#
PATH=/usr/bin:/bin
TRAD=-traditional
DGNUC="@GNUC@"
STDINC="-I/usr/include"
DOLLAR="@dollaropt@"
OPTS=""
INCS="-nostdinc"
FOUNDFILES=false

CPP=/usr/libexec/cpp
if [ ! -x $CPP ]; then
	CPP=`cc -print-search-dirs | sed -ne '/^install: /s/install: \(.*\)/\1cpp/p'`;
	if [ ! -x $CPP ]; then
		echo "$0: installation problem: $CPP not found/executable" >&2
		exit 1
	fi
fi

while [ $# -gt 0 ]
do
	A="$1"
	shift

	case $A in
	-nostdinc)
		STDINC=
		;;
	-traditional)
		TRAD=-traditional
		;;
	-notraditional)
		TRAD=
		;;
	-I*)
		INCS="$INCS $A"
		;;
	-U__GNUC__)
		DGNUC=
		;;
	-imacros|-include|-idirafter|-iprefix|-iwithprefix)
		INCS="$INCS '$A' '$1'"
		shift
		;;
	-*)
		OPTS="$OPTS '$A'"
		;;
	*)
		FOUNDFILES=true
		eval $CPP $TRAD $DGNUC $DOLLAR $INCS $STDINC $OPTS $A || exit $?
		;;
	esac
done

if ! $FOUNDFILES
then
	# read standard input
	eval exec $CPP $TRAD $DGNUC $DOLLAR $INCS $STDINC $OPTS
fi

exit 0
