define(MACHINE,macppc)dnl
vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.33 2010/07/03 03:59:15 krw Exp $-},
etc.MACHINE)dnl
dnl
dnl Copyright (c) 2001-2006 Todd T. Fries <todd@OpenBSD.org>
dnl
dnl Permission to use, copy, modify, and distribute this software for any
dnl purpose with or without fee is hereby granted, provided that the above
dnl copyright notice and this permission notice appear in all copies.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
dnl WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
dnl ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
dnl OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl
_TITLE(make)
_DEV(all)
_DEV(ramd)
_DEV(std)
_DEV(local)
_TITLE(dis)
_DEV(ccd, 18, 16)
_DEV(cd, 9, 3)
_DEV(raid, 54, 19)
_DEV(rd, 17, 17)
_DEV(sd, 8, 2)
_DEV(vnd, 19, 14)
_DEV(wd, 11, 0)
_TITLE(tap)
_DEV(ch)
_DEV(st, 20, 5)
_TITLE(term)
_DEV(com, 7)
_TITLE(pty)
_DEV(ptm, 55)
_DEV(pty)
_DEV(tty)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 67)
_DEV(wskbd, 68)
_TITLE(point)
_DEV(wsmouse, 69)
_TITLE(spec)
_DEV(au)
_DEV(bio, 53)
_DEV(bpf)
_DEV(fdesc, 21)
_DEV(lkm)
_DEV(pf, 39)
_DEV(rnd)
_DEV(systrace, 50)
_DEV(tun)
dnl
divert(__mddivert)dnl
dnl
_std(1, 2, 43, 6)
	;;

sd*|wd*|ccd*|ofdisk*|raid*)
	umask 2 ; unit=${i##*[a-z]}
	case $i in
	sd*) name=sd;		blk=2;	chr=8;;
	wd*) name=wd;		blk=0;	chr=11;;
	ofdisk*) name=ofdisk;	blk=4;	chr=13;;
	ccd*) name=ccd;		blk=16;	chr=18;;
	raid*) name=raid;	blk=19;	chr=54;;
	esac
	rm -f $name$unit? r$name$unit?
	case $unit in
	0|1|2|3|4|5|6|7|8|9)
		mknod ${name}${unit}a	b $blk $(( $unit * 16 + 0 ))
		mknod ${name}${unit}b	b $blk $(( $unit * 16 + 1 ))
		mknod ${name}${unit}c	b $blk $(( $unit * 16 + 2 ))
		mknod ${name}${unit}d	b $blk $(( $unit * 16 + 3 ))
		mknod ${name}${unit}e	b $blk $(( $unit * 16 + 4 ))
		mknod ${name}${unit}f	b $blk $(( $unit * 16 + 5 ))
		mknod ${name}${unit}g	b $blk $(( $unit * 16 + 6 ))
		mknod ${name}${unit}h	b $blk $(( $unit * 16 + 7 ))
		mknod ${name}${unit}i	b $blk $(( $unit * 16 + 8 ))
		mknod ${name}${unit}j	b $blk $(( $unit * 16 + 9 ))
		mknod ${name}${unit}k	b $blk $(( $unit * 16 + 10 ))
		mknod ${name}${unit}l	b $blk $(( $unit * 16 + 11 ))
		mknod ${name}${unit}m	b $blk $(( $unit * 16 + 12 ))
		mknod ${name}${unit}n	b $blk $(( $unit * 16 + 13 ))
		mknod ${name}${unit}o	b $blk $(( $unit * 16 + 14 ))
		mknod ${name}${unit}p	b $blk $(( $unit * 16 + 15 ))
		mknod r${name}${unit}a	c $chr $(( $unit * 16 + 0 ))
		mknod r${name}${unit}b	c $chr $(( $unit * 16 + 1 ))
		mknod r${name}${unit}c	c $chr $(( $unit * 16 + 2 ))
		mknod r${name}${unit}d	c $chr $(( $unit * 16 + 3 ))
		mknod r${name}${unit}e	c $chr $(( $unit * 16 + 4 ))
		mknod r${name}${unit}f	c $chr $(( $unit * 16 + 5 ))
		mknod r${name}${unit}g	c $chr $(( $unit * 16 + 6 ))
		mknod r${name}${unit}h	c $chr $(( $unit * 16 + 7 ))
		mknod r${name}${unit}i	c $chr $(( $unit * 16 + 8 ))
		mknod r${name}${unit}j	c $chr $(( $unit * 16 + 9 ))
		mknod r${name}${unit}k	c $chr $(( $unit * 16 + 10 ))
		mknod r${name}${unit}l	c $chr $(( $unit * 16 + 11 ))
		mknod r${name}${unit}m	c $chr $(( $unit * 16 + 12 ))
		mknod r${name}${unit}n	c $chr $(( $unit * 16 + 13 ))
		mknod r${name}${unit}o	c $chr $(( $unit * 16 + 14 ))
		mknod r${name}${unit}p	c $chr $(( $unit * 16 + 15 ))
		chgrp operator ${name}${unit}[a-p] r${name}${unit}[a-p]
		chmod 640 ${name}${unit}[a-p] r${name}${unit}[a-p]
		;;
	*)
		echo bad unit for disk in: $i
		;;
	esac
	umask 77
	;;

vnd*)
	umask 2 ; unit=${i##*[a-z]}
	for name in vnd svnd; do
		blk=14; chr=19;
		case $name in
		vnd)	off=0;;
		svnd)	off=128;;
		esac
		rm -f $name$unit? r$name$unit?
		mknod ${name}${unit}a	b $blk $(( $unit * 16 + $off + 0 ))
		mknod ${name}${unit}b	b $blk $(( $unit * 16 + $off + 1 ))
		mknod ${name}${unit}c	b $blk $(( $unit * 16 + $off + 2 ))
		mknod ${name}${unit}d	b $blk $(( $unit * 16 + $off + 3 ))
		mknod ${name}${unit}e	b $blk $(( $unit * 16 + $off + 4 ))
		mknod ${name}${unit}f	b $blk $(( $unit * 16 + $off + 5 ))
		mknod ${name}${unit}g	b $blk $(( $unit * 16 + $off + 6 ))
		mknod ${name}${unit}h	b $blk $(( $unit * 16 + $off + 7 ))
		mknod ${name}${unit}i	b $blk $(( $unit * 16 + $off + 8 ))
		mknod ${name}${unit}j	b $blk $(( $unit * 16 + $off + 9 ))
		mknod ${name}${unit}k	b $blk $(( $unit * 16 + $off + 10 ))
		mknod ${name}${unit}l	b $blk $(( $unit * 16 + $off + 11 ))
		mknod ${name}${unit}m	b $blk $(( $unit * 16 + $off + 12 ))
		mknod ${name}${unit}n	b $blk $(( $unit * 16 + $off + 13 ))
		mknod ${name}${unit}o	b $blk $(( $unit * 16 + $off + 14 ))
		mknod ${name}${unit}p	b $blk $(( $unit * 16 + $off + 15 ))
		mknod r${name}${unit}a	c $chr $(( $unit * 16 + $off + 0 ))
		mknod r${name}${unit}b	c $chr $(( $unit * 16 + $off + 1 ))
		mknod r${name}${unit}c	c $chr $(( $unit * 16 + $off + 2 ))
		mknod r${name}${unit}d	c $chr $(( $unit * 16 + $off + 3 ))
		mknod r${name}${unit}e	c $chr $(( $unit * 16 + $off + 4 ))
		mknod r${name}${unit}f	c $chr $(( $unit * 16 + $off + 5 ))
		mknod r${name}${unit}g	c $chr $(( $unit * 16 + $off + 6 ))
		mknod r${name}${unit}h	c $chr $(( $unit * 16 + $off + 7 ))
		mknod r${name}${unit}i	c $chr $(( $unit * 16 + $off + 8 ))
		mknod r${name}${unit}j	c $chr $(( $unit * 16 + $off + 9 ))
		mknod r${name}${unit}k	c $chr $(( $unit * 16 + $off + 10 ))
		mknod r${name}${unit}l	c $chr $(( $unit * 16 + $off + 11 ))
		mknod r${name}${unit}m	c $chr $(( $unit * 16 + $off + 12 ))
		mknod r${name}${unit}n	c $chr $(( $unit * 16 + $off + 13 ))
		mknod r${name}${unit}o	c $chr $(( $unit * 16 + $off + 14 ))
		mknod r${name}${unit}p	c $chr $(( $unit * 16 + $off + 15 ))
		chown root:operator ${name}${unit}[a-p] r${name}${unit}[a-p]
		chmod 640 ${name}${unit}[a-p] r${name}${unit}[a-p]
	done
	umask 77
	;;

tty0*)
	unit=${i##tty0}
	rm -f tty0$unit cua0$unit
	mknod tty0$unit c 7 $unit
	mknod cua0$unit c 7 `expr $unit + 128`
	chown uucp:dialer tty0$unit cua0$unit
	chmod 660 tty0$unit cua0$unit
	;;

pty*)
	class=${i##*[a-z]}
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
		n=0
		while [ $n -lt 16 ]
		do
			nam=$name`hex $n`
			rm -f {tty,pty}$nam
			mknod tty$nam c 4 $(( $offset + $n ))
			mknod pty$nam c 5 $(( $offset + $n ))
			n="$(( $n + 1 ))"
		done
		umask 77
		;;
	esac
	;;

st*)
	umask 2 ; unit=${i##*[a-z]}
	case $i in
	st*) name=st;  chr=20; blk=5;;
	esac
	rm -f $name$unit n$name$unit e$name$unit en$name$unit \
		r$name$unit nr$name$unit er$name$unit enr$name$unit
	case $unit in
	0|1|2|3|4|5|6)
		mknod ${name}${unit}	b $blk $(( $unit * 16 + 0 ))
		mknod n${name}${unit}	b $blk $(( $unit * 16 + 1 ))
		mknod e${name}${unit}	b $blk $(( $unit * 16 + 2 ))
		mknod en${name}${unit}	b $blk $(( $unit * 16 + 3 ))
		mknod r${name}${unit}	c $chr $(( $unit * 16 + 0 ))
		mknod nr${name}${unit}	c $chr $(( $unit * 16 + 1 ))
		mknod er${name}${unit}	c $chr $(( $unit * 16 + 2 ))
		mknod enr${name}${unit}	c $chr $(( $unit * 16 + 3 ))
		chown root:operator ${name}${unit} n${name}${unit} \
			e$name$unit en$name$unit \
			r${name}${unit} nr${name}${unit} \
			er${name}${unit} enr${name}${unit}
		chmod 660 ${name}${unit} n${name}${unit} \
			e$name$unit en$name$unit \
			r${name}${unit} nr${name}${unit} \
			er${name}${unit} enr${name}${unit}
		;;
	*)
		echo bad unit for tape in: $i
		;;
	esac
	umask 77
	;;

ch*)
	umask 2 ; unit=${i##*[a-z]}
	case $i in
	ch*) name=ch;  chr=10;;
	esac
	rm -f $name$unit
	case $unit in
	0|1|2|3|4|5|6)
		mknod ${name}${unit}	c $chr $unit
		chown root:operator ${name}${unit}
		chmod 660 ${name}${unit}
		;;
	*)
		echo bad unit for media changer in: $i
		;;
	esac
	umask 77
	;;

cd*)
	umask 2 ; unit=${i##*[a-z]}
	case $i in
	cd*) name=cd; blk=3; chr=9;;
	esac
	rm -f $name$unit? r$name$unit?
	case $unit in
	0|1|2|3|4|5|6)
		mknod ${name}${unit}a	b $blk $(( $unit * 8 + 0 ))
		mknod ${name}${unit}c	b $blk $(( $unit * 8 + 2 ))
		mknod r${name}${unit}a	c $chr $(( $unit * 8 + 0 ))
		mknod r${name}${unit}c	c $chr $(( $unit * 8 + 2 ))
		chgrp operator ${name}${unit}[a-h] r${name}${unit}[a-h]
		chmod 640 ${name}${unit}[a-h] r${name}${unit}[a-h]
		;;
	*)
		echo bad unit for disk in: $i
		;;
	esac
	umask 77
	;;

audio*)
	major=44
	audio=audio$unit
	sound=sound$unit
	mixer=mixer$unit
	audioctl=audioctl$unit
	rm -f $sound $audio $mixer $audioctl
	mknod $sound    c $major $unit
	mknod $audio    c $major $(( $unit + 128 ))
	mknod $mixer    c $major $(( $unit + 16 ))
	mknod $audioctl c $major $(( $unit + 192 ))
	chown root:wheel $audio $sound $mixer $audioctl
	chmod 666 $audio $sound $mixer $audioctl
	[ -e audio ] || ln -s $audio audio
	[ -e mixer ] || ln -s $mixer mixer
	[ -e sound ] || ln -s $sound sound
	[ -e audioctl ] || ln -s $audioctl audioctl
	;;

usb*)
	rm -f usb$unit
	mknod usb$unit c 61 $unit
	chown root:wheel usb$unit
	chmod 660 usb$unit
	;;

uhid*)
	rm -f uhid$unit
	mknod uhid$unit c 62 $unit
	chown root:wheel uhid$unit
	chmod 660 uhid$unit
	;;

ugen*)
	for j in 0{0,1,2,3,4,5,6,7,8,9} 1{0,1,2,3,4,5}
	do
		rm -f ugen$unit.$j
		mknod ugen$unit.$j c 63 $(( $unit * 16 + 10#$j ))
		chown root:wheel ugen$unit.$j
		chmod 660 ugen$unit.$j
	done
	;;

ulpt*)
	rm -f ulpt$unit
	mknod ulpt$unit c 64 $unit
	chown root:wheel ulpt$unit
	chmod 660 ulpt$unit
	;;

urio*)
	rm -f urio$unit
	mknod urio$unit c 65 $unit
	chown root:wheel urio$unit
	chmod 660 urio$unit
	;;

utty*)
	rm -f utty$unit
	mknod utty$unit c 66 $unit
	chown root:wheel utty$unit
	chmod 660 utty$unit
	;;


ttyCcfg)
	major=67
	minor=255
	rm -f ttyCcfg
	mknod ttyCcfg c $major $minor
	chown root:wheel ttyCcfg
	;;

ttyC*)
	type=C
	unit=${i##ttyC}
	major=67
	minor=$unit
	rm -f tty$type$unit
	mknod tty$type$unit c $major $minor
	chown root:wheel tty$type$unit
	;;

bpf*)
	unit=${i##*[a-z]}
	rm -f bpf${unit}
	mknod bpf${unit} c 22 ${unit}
	chown root:wheel bpf${unit}
	;;

pf)
	rm -f pf
	mknod pf c 39 0
	chown root:wheel pf
	chmod 600 pf
	;;

tun*)
	unit=${i##*[a-z]}
	rm -f tun$unit
	mknod tun$unit c 23 $unit
	chmod 600 tun$unit
	chown root:wheel tun$unit
	;;

rd*)
	blk=17; chr=17;
	umask 2 ; unit=${i##*[a-z]}
	rm -f rd${unit}a rd${unit}c rrd${unit}a rrd${unit}c
	mknod rd${unit}a b ${blk} $(( $unit * 16 + 0 ))
	mknod rd${unit}c b ${blk} $(( $unit * 16 + 2 ))
	mknod rrd${unit}a c ${chr} $(( $unit * 16 + 0 ))
	mknod rrd${unit}c c ${chr} $(( $unit * 16 + 2 ))
	chown root:operator rd${unit}[ac] rrd${unit}[ac]
	chmod 640 rd${unit}[ac] rrd${unit}[ac]
	umask 77
	;;

lkm)
	rm -f lkm
	mknod lkm c 24 0
	chown root:kmem lkm
	chmod 640 lkm
	;;

pci*)
	rm -f pci
	mknod pci c 71 0
	chown root:kmem pci
	chmod 600 pci
	;;

random|srandom|urandom|prandom|arandom)
	rm -f random urandom srandom prandom arandom
	mknod  random c 40 0
	mknod srandom c 40 1
	mknod urandom c 40 2
	mknod prandom c 40 3
	mknod arandom c 40 4
	chown root:wheel random srandom urandom prandom arandom
	chmod 644 random srandom urandom prandom arandom
	;;
uk*)
	unit=${i##*[a-z]}
	rm -f uk$unit
	mknod uk$unit c 41 $unit
	chown root:operator uk$unit
	chmod 640 uk$unit
	;;

wscons)
	sh $this wskbd0 wskbd1 wskbd2 wskbd3
	sh $this wsmouse0 wsmouse1 wsmouse2 wsmouse3
	sh $this ttyCcfg
	sh $this wsmux
	;;
wsmux|wsmouse|wskbd)
	rm -f wsmouse wskbd
	mknod wsmouse c 70 0
	mknod wskbd c 70 1
	chown root:wheel wsmouse wskbd
	chmod 600 wsmouse wskbd
	;;

wskbd*)
	unit=${i##*[a-z]}
	rm -f wskbd${unit}
	mknod wskbd${unit} c 68 ${unit}
	# XXX
	chmod 660 wskbd${unit}
	chown root:wheel wskbd${unit}
	;;
wsmouse*)
	unit=${i##*[a-z]}
	rm -f wsmouse${unit}
	mknod wsmouse${unit} c 69 ${unit}
	# XXX
	chmod 660 wsmouse${unit}
	chown root:wheel wsmouse${unit}
	;;

nnpfs*)
	rm -f nnpfs$unit
	mknod nnpfs$unit c 46 $unit
	chmod 600 nnpfs$unit
	chown root:wheel nnpfs$unit
	;;

vscsi*)
	rm -f vscsi$unit
	mknod vscsi$unit c 51 $unit
	chmod 600 vscsi$unit
	chown root:wheel vscsi$unit
	;;
diskmap)
	rm -f diskmap
	mknod diskmap c 57 0
	chmod 640 diskmap
	chown root:operator diskmap
	;;

altq)
	mkdir -p altq
	chmod 755 altq
	unit=0
	for dev in altq cbq wfq afm fifoq red rio localq hfsc \
	    cdnr blue priq; do
		rm -f altq/$dev
		mknod altq/$dev c 71 $unit
		chmod 644 altq/$dev
		unit=$(($unit + 1))
	done
	;;

*)
	echo $i: unknown device
esac
done
dnl
dnl *** mvmeppc specific devices
dnl
target(all, ch, 0)dnl
target(all, nnpfs, 0)dnl
target(all, vscsi, 0)dnl
twrget(all, flo, fd, 0, 0B, 0C, 0D, 0E, 0F, 0G, 0H)dnl
twrget(all, flo, fd, 1, 1B, 1C, 1D, 1E, 1F, 1G, 1H)dnl
target(all, pty, 0)dnl
target(all, bpf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)dnl
target(all, bio)dnl
target(all, tun, 0, 1, 2, 3)dnl
target(all, xy, 0, 1, 2, 3)dnl
target(all, rd, 0)dnl
target(all, cd, 0, 1)dnl
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, vnd, 0, 1, 2, 3)dnl
target(all, ccd, 0, 1, 2, 3)dnl
twrget(ramd, wsdisp, ttyC, 0)dnl
target(ramd, bio)dnl
target(ramd, diskmap)dnl
