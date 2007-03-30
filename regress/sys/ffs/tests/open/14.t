#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/14.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="open returns EROFS if the named file resides on a read-only file system, and the file is to be modified"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
dd if=/dev/zero of=tmpdisk bs=1k count=1024 2>/dev/null
vnconfig svnd1 tmpdisk
newfs /dev/rsvnd1c >/dev/null
mount /dev/svnd1c ${n0}
expect 0 create ${n0}/${n1} 0644
expect 0 open ${n0}/${n1} O_WRONLY
expect 0 open ${n0}/${n1} O_RDWR
expect 0 open ${n0}/${n1} O_RDONLY,O_TRUNC
mount -ur /dev/svnd1c
expect EROFS open ${n0}/${n1} O_WRONLY
expect EROFS open ${n0}/${n1} O_RDWR
expect EROFS open ${n0}/${n1} O_RDONLY,O_TRUNC
mount -uw /dev/svnd1c
expect 0 unlink ${n0}/${n1}
umount /dev/svnd1c
vnconfig -u svnd1
rm tmpdisk
expect 0 rmdir ${n0}
