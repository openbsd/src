#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rmdir/13.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="rmdir returns EBUSY if the directory to be removed is the mount point for a mounted file system"

n0=`namegen`

expect 0 mkdir ${n0} 0755
dd if=/dev/zero of=tmpdisk bs=1k count=1024 2>/dev/null
vnconfig svnd1 tmpdisk
newfs /dev/rsvnd1c >/dev/null
mount /dev/svnd1c ${n0}
expect EBUSY rmdir ${n0}
umount /dev/svnd1c
vnconfig -u svnd1
rm tmpdisk
expect 0 rmdir ${n0}
