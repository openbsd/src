#!/bin/sh -
#
#	$OpenBSD: MAKEDEV.md,v 1.1 2002/02/08 20:26:36 todd Exp $
#	$NetBSD: MAKEDEV,v 1.5 1997/01/01 23:46:23 pk Exp $
#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# Written and contributed by W. Jolitz 12/90
#
# Redistribution and use in source and binary forms are permitted provided
# that: (1) source distributions retain this entire copyright notice and
# comment, and (2) distributions including binaries display the following
# acknowledgement:  ``This product includes software developed by the
# University of California, Berkeley and its contributors'' in the
# documentation or other materials provided with the distribution and in
# all advertising materials mentioning features or use of this software.
# Neither the name of the University nor the names of its contributors may
# be used to endorse or promote products derived from this software without
# specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)MAKEDEV	5.2 (Berkeley) 6/22/90
#
# Device "make" file.  Valid arguments:
#	all	makes all known devices, including local devices.
#		Tries to make the 'standard' number of each type.
#	std	standard devices
#	local	configuration specific devices
#
# Tapes:
#	st*	SCSI tapes
#
# Disks:
#	sd*	SCSI disks
#	cd*	SCSI CD-ROM
#	rd*	"ramdisk" pseudo-disks
#	vnd*	"file" pseudo-disks
#	ccd*	contatenated disk devices
#
# Pseudo terminals:
#	pty*	set of 16 master and slave pseudo terminals
#
# Printers:
#	lpt*	stock lp
#	lpa*	interruptless lp
#
# Call units:
#
# Special purpose devices:
#	bpf*	packet filter
#	lkm	loadable kernel modules interface
#	tun*	network tunnel driver
#	ss*	SCSI scanner
#	uk*	SCSI unknown
#	ch*	SCSI changer
#	altq	ALTQ control interface
#
# Machine specific devices:
#	sram	static ram available on some models.
#	nvram	non-volatile ram
#	flash	flash ram available on some models.
#	bugtty	(depricated)
#	vmel	32-bit vme interface
#	vmes	16-bit vme interface

PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/etc
this=$0
umask 77
for i
do
case $i in

all)
	sh $this std fd st0 ttya ttyb ttyc ttyd
	sh $this tty00 tty01 tty02 tty03
	sh $this ttyw0
	sh $this sd0 sd1 sd2 sd3 sd4 sd5 sd6 sd7 sd8 sd9
	sh $this vnd0 vnd1 pty0 cd0
	sh $this bpf0 bpf1 bpf2 bpf3 bpf4 bpf5 bpf6 bpf7 bpf8 bpf9
	#sh $this ccd0 ccd1 ccd2 ccd3
	sh $this pf tun0 tun1 lkm local
	sh $this sram0 nvram0 flash0 vmel0 vmes0
	#sh $this lp0 lptwo0
	sh $this random
	sh $this uk0 uk1
	sh $this ss0 altq
	;;

std)
	rm -f console drum mem kmem null zero tty
	rm -f klog stdin stdout stderr ksyms
	mknod console		c 0 0
	mknod drum		c 3 0	; chmod 640 drum ; chgrp kmem drum
	mknod kmem		c 2 1	; chmod 640 kmem ; chgrp kmem kmem
	mknod mem		c 2 0	; chmod 640 mem	; chgrp kmem mem
	mknod null		c 2 2	; chmod 666 null
	mknod zero		c 2 12	; chmod 666 zero
	mknod tty		c 1 0	; chmod 666 tty
	mknod klog		c 6 0	; chmod 600 klog
	mknod stdin		c 21 0	; chmod 666 stdin
	mknod stdout		c 21 1	; chmod 666 stdout
	mknod stderr		c 21 2	; chmod 666 stderr
	mknod ksyms		c 43 0	; chmod 640 ksyms ; chown root.kmem ksyms
	;;

raminst)
	sh $this std fd st0 ttya rd0
	sh $this tty00 tty01 tty02 tty03
	sh $this sd0 sd1 sd2 sd3 
	sh $this pty0 
	#sh $this ccd0 ccd1 ccd2 ccd3
	sh $this tun0 tun1 lkm local
	sh $this sram0 nvram0 flash0 vmel0 vmes0
	#sh $this lp0 lptwo0
	sh $this random
	sh $this uk0 uk1
	sh $this ss0
	;;

fd)
	rm -f fd/*
	mkdir fd > /dev/null 2>&1
	(cd fd && eval `echo "" | awk ' BEGIN { \
		for (i = 0; i < 64; i++) \
	 		printf("mknod %d c 21 %d;", i, i)}'`)
	chown -R bin.bin fd
	chmod 555 fd
	chmod 666 fd/*
	;;

ss*)
	case $i in
	ss*) name=ss;	unit=${i#ss};	chr=33;;
	esac
	rm -f $name$unit n$name$unit
	mknod $name$unit	c $chr `expr $unit '*' 16 + 0`
	mknod n$name$unit	c $chr `expr $unit '*' 16 + 1`
	chgrp operator $name$unit n$name$unit
	chmod 640 $name$unit n$name$unit
	;;

ccd*|sd*)
	case $i in
	ccd*) name=ccd;	unit=${i#ccd};	blk=5;	chr=17;;
	sd*) name=sd;	unit=${i#sd};	blk=4;	chr=8;;
	esac
	rm -f $name$unit? r$name$unit?
	minor=`expr $unit '*' 16`
	for slice in a b c d e f g h i j k l m n o p
	do
		dev=${name}${unit}${slice}
		mknod $dev b $blk $minor
		mknod r$dev c $chr $minor
		minor=$(( $minor + 1 ))
	done
	chown root.operator $name$unit? r$name$unit?
	chmod 640 $name$unit? r$name$unit?
	;;

vnd*)
	unit=${i#vnd}
	for name in vnd svnd; do
		blk=8; chr=19;
		case $name in
		vnd)	off=0;;
		svnd)	off=128;;
		esac
		rm -f $name$unit? r$name$unit?
		minor=`expr $unit '*' 16 '+' $off`
		for slice in a b c d e f g h i j k l m n o p
		do
			dev=${name}${unit}${slice}
			mknod $dev b $blk $minor
			mknod r$dev c $chr $minor
			minor=$(( $minor + 1 ))
		done
		chown root.operator ${name}${unit}? r${name}${unit}?
		chmod 640 ${name}${unit}? r${name}${unit}?
	done
	;;

pty*)
	class=${i#pty}
	case $class in
	0) offset=0 name=p;;
	1) offset=16 name=q;;
	2) offset=32 name=r;;
	3) offset=48 name=s;;
	4) offset=64 name=t;;
	5) offset=80 name=u;;
	6) offset=96 name=v;;
	7) offset=112 name=w;;
	8) offset=128 name=x;;
	9) offset=144 name=y;;
	10) offset=160 name=z;;
	11) offset=176 name=P;;
	12) offset=192 name=Q;;
	13) offset=208 name=R;;
	14) offset=224 name=S;;
	15) offset=240 name=T;;
	*) echo bad unit for pty in: $i;;
	esac
	case $class in
	0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15)
		umask 0
		eval `echo $offset $name | awk ' { b=$1; n=$2 } END {
			for (i = 0; i < 16; i++)
				printf("rm -f tty%s%x; mknod tty%s%x c 4 %d; \
				    rm -f pty%s%x; mknod pty%s%x c 5 %d; ", \
				    n, i, n, i, b+i, n, i, n, i, b+i); }'`
		umask 77
		;;
	esac
	;;

st*)
	case $i in
	st*) name=st;	unit=${i#st};	chr=20;	blk=7;;
	esac
	rm -f $name$unit n$name$unit e$name$unit en$name$unit \
		r$name$unit nr$name$unit er$name$unit enr$name$unit 
	mknod $name$unit	b $blk `expr $unit '*' 16 + 0`
	mknod n$name$unit	b $blk `expr $unit '*' 16 + 1`
	mknod e$name$unit	b $blk `expr $unit '*' 16 + 2`
	mknod en$name$unit	b $blk `expr $unit '*' 16 + 3`
	mknod r$name$unit	c $chr `expr $unit '*' 16 + 0`
	mknod nr$name$unit	c $chr `expr $unit '*' 16 + 1`
	mknod er$name$unit	c $chr `expr $unit '*' 16 + 2`
	mknod enr$name$unit	c $chr `expr $unit '*' 16 + 3`
	chgrp operator $name$unit n$name$unit e$name$unit en$name$unit \
		r$name$unit nr$name$unit er$name$unit enr$name$unit 
	chmod 660 $name$unit n$name$unit e$name$unit en$name$unit \
		r$name$unit nr$name$unit er$name$unit enr$name$unit 
	;;

ch*)
	case $i in
	ch*) name=ch;	unit=${i#ch};	chr=31;;
	esac
	rm -f $name$unit
	mknod $name$unit	c $chr $unit
	chown root.operator $name$unit
	chmod 660 $name$unit
	;;

uk*)
	case $i in
	uk*) name=uk;	unit=${i#uk};	chr=34;;
	esac
	rm -f $name$unit
	mknod $name$unit	c $chr $unit
	chown root.wheel $name$unit
	chmod 600 $name$unit
	;;

cd*)
	case $i in
	cd*) name=cd;	unit=${i#cd};	chr=9;	blk=6;;
	esac
	rm -f $name$unit? r$name$unit?
	mknod ${name}${unit}a	b $blk `expr $unit '*' 16 + 0`
	mknod ${name}${unit}c	b $blk `expr $unit '*' 16 + 2`
	mknod r${name}${unit}a	c $chr `expr $unit '*' 16 + 0`
	mknod r${name}${unit}c	c $chr `expr $unit '*' 16 + 2`
	chgrp operator $name$unit? r$name$unit?
	chmod 640 $name$unit? r$name$unit?
	;;

lpt*|lpa*)
	case $i in
	lpt*) name=lpt;	unit=${i#lpt};	chr=11;	flags=0;;
	lpa*) name=lpa;	unit=${i#lpa};	chr=11;	flags=128;;
	esac
	rm -f $name$unit
	mknod $name$unit	c $chr `expr $unit + $flags`
	chown root.wheel $name$unit
	;;

pf)
	rm -f pf
	mknod pf c 39 0
	chown root.wheel pf
	chmod 600 pf
	;;

bpf*|tun*)
	case $i in
	bpf*) name=bpf;	unit=${i#bpf};	chr=22;;
	tun*) name=tun;	unit=${i#tun};	chr=23;;
	esac
	rm -f $name$unit
	mknod $name$unit	c $chr $unit
	chown root.wheel $name$unit
	;;

sram*|nvram*|flash*|vmel*|vmes*)
        rm -f $i
	case $i in
	sram*) maj=7;;
	nvram*) maj=10;;
	flash*) maj=11;;
	vmel*) maj=31;;
	vmes*) maj=32;;
	esac
        mknod $i c ${maj} 0
        chown root.kmem $i
        chmod 640 $i
	;;

random|srandom|urandom|prandom|arandom)
	rm -f random urandom srandom prandom arandom
	mknod  random c 40 0
	mknod srandom c 40 1
	mknod urandom c 40 2
	mknod prandom c 40 3
	mknod arandom c 40 4
	chown root.wheel random srandom urandom prandom arandom
	chmod 644 random srandom urandom prandom arandom
	;;

rd*)
	umask 2 ; unit=`expr $i : '.*d\(.*\)'`
	mknod rd${unit}a b 7 `expr $unit '*' 16 + 0`
	mknod rd${unit}c b 7 `expr $unit '*' 16 + 2`
	mknod rrd${unit}a c 18 `expr $unit '*' 16 + 0`
	mknod rrd${unit}c c 18 `expr $unit '*' 16 + 2`
	chown root.operator rd${unit}[ac] rrd${unit}[ac]
	chmod 640 rd${unit}[ac] rrd${unit}[ac]
	umask 77
	;;

lkm)
	rm -f lkm
	mknod lkm c 24 0
	chown root.kmem lkm
	chmod 640 lkm
	;;

altq)
	mkdir -p altq
	chmod 755 altq
	unit=0
	for dev in altq cbq wfq afm fifoq red rio localq hfsc \
	    cdnr blue priq; do
		rm -f altq/$dev
		mknod altq/$dev c 52 $unit
		chmod 644 altq/$dev
		unit=$(($unit + 1))
	done
	;;

local)
	umask 0
	test -s MAKEDEV.local && sh MAKEDEV.local
	;;

esac
done
