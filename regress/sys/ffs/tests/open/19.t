#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/19.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="open returns ENOSPC when O_CREAT is specified, the file does not exist, and there are no free inodes on the file system on which the file is being created"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
dd if=/dev/zero of=tmpdisk bs=1k count=256 2>/dev/null
vnconfig svnd1 tmpdisk
newfs /dev/rsvnd1c >/dev/null
mount /dev/svnd1c ${n0}
i=0
while :; do
	touch ${n0}/${i} >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		break
	fi
	i=`expr $i + 1`
done
expect ENOSPC open ${n0}/${i} O_RDONLY,O_CREAT 0644
umount /dev/svnd1c
vnconfig -u svnd1
rm tmpdisk
expect 0 rmdir ${n0}
