#!/bin/ksh
#
# $OpenBSD: reorder_kernel.sh,v 1.3 2017/08/25 18:59:55 rpe Exp $
#
# Copyright (c) 2017 Robert Peichaer <rpe@openbsd.org>
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

set -o errexit

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

# Skip if /usr/share is on a nfs mounted filesystem.
DISK_DEV=$(df /usr/share | sed '1d;s/ .*//')
[[ $(mount | grep "^$DISK_DEV") == *" type nfs "* ]] && exit 1

COMPILE_DIR=/usr/share/compile
KERNEL=$(sysctl -n kern.osversion)
KERNEL=${KERNEL%#*}
LOGFILE=$COMPILE_DIR/$KERNEL/relink.log
PROGNAME=${0##*/}
SHA256=/var/db/kernel.SHA256

# Create kernel compile dir and redirect stdout/stderr to a logfile.
mkdir -m 700 -p $COMPILE_DIR/$KERNEL
exec 1>$LOGFILE
exec 2>&1

# Install trap handlers to inform about success or failure via syslog.
trap 'trap - EXIT; logger -st $PROGNAME \
	"kernel relinking failed; see $LOGFILE" >>/dev/console 2>&1' ERR
trap 'logger -t $PROGNAME "kernel relinking done"' EXIT

if [[ -f $COMPILE_DIR.tgz ]]; then
	rm -rf $COMPILE_DIR/$KERNEL/*
	# The directory containing the logfile was just deleted, redirect
	# stdout again to a new logfile.
	exec 1>$LOGFILE
	tar -C $COMPILE_DIR -xzf $COMPILE_DIR.tgz $KERNEL
	rm -f $COMPILE_DIR.tgz
fi

sha256 -C $SHA256 /bsd

cd $COMPILE_DIR/$KERNEL
make newbsd
make newinstall

echo "\nKernel has been relinked and is active on next reboot.\n"
cat $SHA256
