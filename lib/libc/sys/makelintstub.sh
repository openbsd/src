#!/bin/sh -
#	$OpenBSD: makelintstub.sh,v 1.9 2010/07/24 23:32:52 guenther Exp $
#	$NetBSD: makelintstub,v 1.2 1997/11/05 05:46:18 thorpej Exp $
#
# Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
#      This product includes software developed for the NetBSD Project
#      by Christopher G. Demetriou.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
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

usage()
{

	echo "usage: $0 [-n|-p] [-o filename] object ..."
	exit 1
}

header()
{

	cat <<- __EOF__
	/*
	 * THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT EDIT.
	 */

	#include <sys/param.h>
	#include <sys/time.h>
	#include <sys/mount.h>
	#include <sys/stat.h>
	#include <ufs/ufs/quota.h>
	#include <ufs/ufs/inode.h>
	#include <sys/resource.h>
	#include <sys/poll.h>
	#include <sys/uio.h>
	#include <sys/ipc.h>
	#include <sys/msg.h>
	#include <sys/sem.h>
	#include <sys/shm.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <sys/ktrace.h>
	#include <sys/mman.h>
	#include <sys/event.h>
	#include <nnpfs/nnpfs_pioctl.h>
	#include <sys/wait.h>
	#include <stdio.h>
	#undef DIRBLKSIZ
	#include <dirent.h>
	#include <fcntl.h>
	#include <signal.h>
	#include <unistd.h>
	#ifdef __STDC__
	#include <stdarg.h>
	#else
	#include <varargs.h>
	#endif
	#include <err.h>

	__EOF__
}

syscall_stub()
{

	syscallhdr="$1"
	syscallname="$2"
	funcname="$3"

	arglist=$(printf '#include "%s"\n' "$syscallhdr" | cpp -C | \
    	grep '^/\* syscall: "'"$syscallname"'" ' | \
    	sed -e 's,^/\* syscall: ,,;s, \*/$,,')

	eval set -f -- "$arglist"

	if [ $# -lt 4 ]; then
		echo syscall $syscallname not found! 1>&2
		exit 1
	fi

	syscallname=$1
	shift 2			# kill name and "ret:"
	returntype=$1
	shift 2			# kill return type and "args:"

	if [ "`eval echo -n \\$$#`" = "..." ]; then
		varargs=YES
		nargs=$(($# - 1))
	else
		varargs=NO
		nargs=$#
	fi
	nargswithva=$#

	echo "/*ARGSUSED*/"
	echo "$returntype"

	# do ANSI C function header

	echo -n	"$funcname("
	if [ $nargs -eq 0 ]; then
		echo -n "void"
	else
		i=1
		while [ $i -le $nargs ]; do
			eval echo -n \""\$$i"\"
			echo -n	" arg$i"
			if [ $i -lt $nargswithva ]; then
				echo -n	", "
			fi
			i=$(($i + 1))
		done
	fi
	if [ $varargs = YES ]; then
		echo -n "..."
	fi
	echo	")"
	echo	"{"
	if [ "$returntype" != "void" ]; then
		echo "        return (($returntype)0);"
	fi
	echo	"}"
}

trailer()
{

	cat <<- __EOF__
	/* END */
	__EOF__
}

pflag=NO
nflag=NO
oarg=""
syscallhdr=/usr/include/sys/syscall.h

args=`getopt no:ps: $*`
if test $? -ne 0; then
	usage
fi
set -- $args

for i; do
	case "$i" in
	-n)	nflag=YES; shift;;
	-o)	oarg=$2; shift; shift;;
	-p)	pflag=YES; shift;;
	-s)	syscallhdr=$2; shift; shift;;
	--)	shift; break;;
	esac
done

if [ $pflag = YES ] && [ $nflag = YES ]; then
	echo "$0: -n flag and -p flag may not be used together"
	echo ""
	usage
fi

if [ "X$oarg" != "X" ]; then
	exec > $oarg
fi

header
for syscall; do
	fnname=`echo $syscall | sed -e 's,\.o$,,'`
	if [ $pflag = YES ]; then
		scname=`echo $fnname | sed -e 's,^_,,'`
	else
		scname=$fnname
	fi
	syscall_stub $syscallhdr $scname $fnname
	echo ""
done
trailer

exit 0
