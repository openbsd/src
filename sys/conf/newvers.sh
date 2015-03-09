#!/bin/sh -
#
#	$OpenBSD: newvers.sh,v 1.137 2015/03/09 20:08:55 miod Exp $
#	$NetBSD: newvers.sh,v 1.17.2.1 1995/10/12 05:17:11 jtc Exp $
#
# Copyright (c) 1984, 1986, 1990, 1993
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
#	@(#)newvers.sh	8.1 (Berkeley) 4/20/94

if [ ! -r version -o ! -s version ]
then
	echo 0 > version
fi

touch version
v=`cat version` u=${USER-root} d=`pwd` h=`hostname` t=`date`
id=`basename "${d}"`

# additional things which need version number upgrades:
#	sys/sys/param.h:
#		OpenBSD symbol
#		OpenBSD_X_X symbol
#	share/mk/sys.mk
#		OSMAJOR
#		OSMINOR
#	etc/root/root.mail
#		VERSION and other bits
#	sys/arch/macppc/stand/tbxidata/bsd.tbxi
#		change	/X.X/macppc/bsd.rd
#
# -current and -beta tagging:
#	For release, select STATUS ""
#	Right after release unlock, select STATUS "-current"
#	and enable POOL_DEBUG in sys/conf/GENERIC
#	A month or so before release, select STATUS "-beta"
#	and disable POOL_DEBUG in sys/conf/GENERIC

ost="OpenBSD"
osr="5.7"

cat >vers.c <<eof
#if 0
#define STATUS ""			/* release */
#define STATUS "-beta"			/* just before a release */
#endif
#define STATUS "-current"		/* just after a release */

const char ostype[] = "${ost}";
const char osrelease[] = "${osr}";
const char osversion[] = "${id}#${v}";
const char sccs[] =
    "    @(#)${ost} ${osr}" STATUS " (${id}) #${v}: ${t}\n";
const char version[] =
    "${ost} ${osr}" STATUS " (${id}) #${v}: ${t}\n    ${u}@${h}:${d}\n";
eof

expr ${v} + 1 > version
