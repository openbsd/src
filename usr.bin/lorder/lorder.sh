#!/bin/sh -
#	$OpenBSD: lorder.sh,v 1.12 2003/02/08 10:19:30 pvalchev Exp $
#	$NetBSD: lorder.sh.gnm,v 1.3 1995/12/20 04:45:11 cgd Exp $
#
# Copyright (c) 1990, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
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
#	@(#)lorder.sh	8.1 (Berkeley) 6/6/93
#

# one argument can be optimized: put out the filename twice
case $# in
	0)
		echo "usage: lorder file ...";
		exit ;;
	1)
		echo $1 $1;
		exit ;;
esac

# temporary files
R=`mktemp /tmp/_referenceXXXXXX` || exit 1
S=`mktemp /tmp/_symbolXXXXXX` || {
	rm -f ${R}
	exit 1
}

# remove temporary files on HUP, INT, QUIT, PIPE, TERM
trap "rm -f $R $S; exit 0" 0
trap "rm -f $R $S; exit 1" 1 2 3 13 15

# make sure files depend on themselves
for file in "$@"; do echo "$file $file" ; done
# if the line has " T ", " D ", " G ", " R ",  it's a globally defined 
# symbol, put it into the symbol file.
#
# if the line has " U " it's a globally undefined symbol, put it into
# the reference file.
${NM:-nm} -go "$@" | sed "
	/ [TDGR] / {
		s/:.* [TDGR] / /
		w $S
		d
	}
	/ U / {
		s/:.* U / /
		w $R
	}
	d
"

# sort symbols and references on the first field (the symbol)
# join on that field, and print out the file names (dependencies).
sort +1 $R -o $R
sort +1 $S -o $S
join -j 2 -o 1.1 2.1 $R $S
rm -f $R $S
