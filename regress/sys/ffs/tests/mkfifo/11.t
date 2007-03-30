#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkfifo/11.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="mkfifo returns ENOSPC if there are no free inodes on the file system on which the file is being created"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
dd if=/dev/zero of=tmpdisk bs=1k count=256 2>/dev/null
vnconfig svnd1 tmpdisk
newfs /dev/rsvnd1c >/dev/null
mount /dev/svnd1c ${n0}
i=0
while :; do
	mkfifo ${n0}/${i} >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		break
	fi
	i=`expr $i + 1`
done
expect ENOSPC mkfifo ${n0}/${n1} 0644
umount /dev/svnd1c
vnconfig -u svnd1
rm tmpdisk
expect 0 rmdir ${n0}
