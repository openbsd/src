#!/bin/ksh
#
# $OpenBSD: reorder_kernel.sh,v 1.9 2019/09/28 17:30:07 ajacoutot Exp $
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

export PATH=/usr/bin:/bin:/usr/sbin:/sbin

# Skip if /usr/share is on a nfs mounted filesystem.
df -t nfs /usr/share >/dev/null 2>&1 && exit 1

KERNEL=$(sysctl -n kern.osversion)
KERNEL=${KERNEL%#*}
KERNEL_DIR=/usr/share/relink/kernel
LOGFILE=$KERNEL_DIR/$KERNEL/relink.log
PROGNAME=${0##*/}
SHA256=/var/db/kernel.SHA256

# Create kernel compile dir and redirect stdout/stderr to a logfile.
mkdir -m 700 -p $KERNEL_DIR/$KERNEL
exec 1>$LOGFILE
exec 2>&1

# Install trap handlers to inform about success or failure via syslog.
trap 'trap - EXIT; logger -st $PROGNAME \
	"failed -- see $LOGFILE" >>/dev/console 2>&1' ERR
trap 'logger -t $PROGNAME "kernel relinking done"' EXIT

if [[ -f $KERNEL_DIR.tgz ]]; then
	rm -rf $KERNEL_DIR/$KERNEL/*
	# The directory containing the logfile was just deleted, redirect
	# stdout again to a new logfile.
	exec 1>$LOGFILE
	tar -C $KERNEL_DIR -xzf $KERNEL_DIR.tgz $KERNEL
	rm -f $KERNEL_DIR.tgz
fi

if ! sha256 -C $SHA256 /bsd; then
	cat <<__EOF

Failed to verify /bsd's checksum, therefore a randomly linked kernel (KARL)
is not being built. KARL can be re-enabled for next boot by issuing as root:

sha256 -h $SHA256 /bsd
__EOF
	# Trigger ERR trap
	false
fi

cd $KERNEL_DIR/$KERNEL
make newbsd
make newinstall
sync

echo "\nKernel has been relinked and is active on next reboot.\n"
cat $SHA256
