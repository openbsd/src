#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/link/16.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="link returns EROFS if the requested link requires writing in a directory on a read-only file system"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n0} 0755
dd if=/dev/zero of=tmpdisk bs=1k count=1024 2>/dev/null
vnconfig svnd1 tmpdisk
newfs /dev/rsvnd1c >/dev/null
mount /dev/svnd1c ${n0}
expect 0 create ${n0}/${n1} 0644

expect 0 link ${n0}/${n1} ${n0}/${n2}
expect 0 unlink ${n0}/${n2}
mount -ur /dev/svnd1c
expect EROFS link ${n0}/${n1} ${n0}/${n2}
mount -uw /dev/svnd1c
expect 0 link ${n0}/${n1} ${n0}/${n2}
expect 0 unlink ${n0}/${n2}

expect 0 unlink ${n0}/${n1}
umount /dev/svnd1c
vnconfig -u svnd1
rm tmpdisk
expect 0 rmdir ${n0}
