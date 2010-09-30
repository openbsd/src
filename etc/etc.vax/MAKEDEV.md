define(MACHINE,vax)dnl
vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.43 2010/09/30 15:27:54 claudio Exp $-},
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
dnl *** vax specific definitions
dnl
__devitem(hd, hd*, {-HDC9224 disks-})dnl
__devitem(dz, dz*, UNIBUS DZ11 and DZ32)dnl
_mkdev(dz, dz*,
{-case $U in
	[0-7])
		i=0
		while [ $i -lt 8 ]; do
			no=Add(Mult($U, 8), $i)
			if [ $no -lt 10 ]; then
				no="0${no}"
			fi
			M tty${no} c 1 $no 600
			i="$(( $i + 1 ))"
		done
	;;
	*)
		echo bad unit for dz in: $i
		;;
	esac-})dnl
__devitem(dhu, dhu*, UNIBUS DHU11)dnl
_mkdev(dhu, dhu*,
{-set -A cnvtbl 0 1 2 3 4 5 6 7 8 9 a b c d e f
	case $U in
	[0-7])
		case $U in
		0) ch=S ;; 1) ch=T ;; 2) ch=U ;; 3) ch=V ;;
		4) ch=W ;; 5) ch=X ;; 6) ch=Y ;; 7) ch=Z ;;
		esac
		i=0
		while [ $i -lt 16 ]; do
			let=${cnvtbl[$i]}
			M tty${ch}${let} c major_dhu_c Add(Mult($U, 16), $i) 600
			i="$(( $i + 1 ))"
		done
	;;
	*)
		echo bad unit for dhu in: $i
	;;
	esac-})dnl
__devitem(dl, dl*, UNIBUS DL11)dnl
_mkdev(dl, dl*,
{-set -A cnvtbl 0 1 2 3 4 5 6 7 8 9 a b c d e f g h i j k l m n o p q r s t u v
	let=${cnvtbl[$U]}
	if [ -n "$let" ] ;then
		M ttyJ${let} c major_dl_c $U 600
	else
		echo bad unit for dl in: $i
	fi-})dnl
__devitem(qsc, qsc*, VXT2000 serial ports)dnl
_mkdev(qsc, qsc*,
{-
	case $U in
	0)
		for i in 0 1 2 3; do
			M ttyb${i} c major_qsc_c ${i} 600
		done
	;;
	*)
		echo bad unit for qsc in: $i
	;;
	esac-})dnl
dnl
dnl
_TITLE(make)
_DEV(all)
_DEV(ramdisk)
_DEV(std)
_DEV(local)
_TITLE(dis)
_DEV(ccd, 54, 17)
_DEV(cd, 61, 22)
_DEV(flo, 54, 16)
_DEV(hd, 58, 19)
_DEV(hk, 11, 3)
_DEV(hp, 4, 0)
_DEV(ra, 9, 9)
_DEV(raid, 73, 25)
_DEV(rb, 23, 11)
_DEV(rd, 62, 23)
_DEV(rl, 32, 14)
_DEV(rx, 30, 12)
_DEV(sd, 59, 20)
_DEV(up, 13, 2)
_DEV(vnd, 55, 18)
_TITLE(tap)
_DEV(ch, 63)
_DEV(ht, 5, 1)
_DEV(mt, 38, 15)
_DEV(st, 60, 21)
_DEV(tm, 14, 5)
_DEV(ts, 16, 6)
_DEV(ut, 17, 10)
_TITLE(term)
_DEV(dhu, 34)
_DEV(dl, 66)
_DEV(dz, 1)
_DEV(qsc, 48)
_TITLE(pty)
_DEV(ptm, 75)
_DEV(pty, 21)
_DEV(tty, 20)
dnl _TITLE(prn)
_TITLE(cons)
_DEV(wsdisp, 68)
_DEV(wscons)
_DEV(wskbd, 69)
_TITLE(point)
_DEV(wsmouse, 70)
_TITLE(spec)
_DEV(bio, 72)
_DEV(bpf, 56)
_DEV(diskmap, 79)
_DEV(fdesc, 53)
_DEV(lkm, 28)
_DEV(nnpfs, 74)
_DEV(pf, 42)
_DEV(pppx, 80)
_DEV(rnd, 67)
_DEV(systrace, 49)
_DEV(tun, 57)
_DEV(uk, 65)
_DEV(vscsi, 78)
dnl
divert(__mddivert)dnl
dnl
ramdisk)
	_recurse std bpf0 bio diskmap
	_recurse fd0 sd0 sd1 rd0
	;;

_std(2, 3, 50, 33)
	M ttyg0		c 25 0 600
	M ttyg1		c 25 1 600
	M ttyg2		c 25 2 600
	M ttyg3		c 25 3 600
	M crl		c 35 0 600
	M csa1		c 51 0 600
	M csa2		c 51 1 600
	M tu0		b 8 0 600
	M tu1		b 8 1 600
	M floppy	c 8 0 600
	;;

ht*|tm*|mt*|ts*|ut*)
	case $i in
	ht*) n=ht b=1 c=5;;
	mt*) n=mt b=15 c=38;;
	tm*) n=tm b=5 c=14;;
	ts*) n=ts b=6 c=16;;
	ut*) n=ut b=10 c=17;;
	esac
	case $U in
	[0-7])
		four=Add($U, 4)
		eight=Add($U, 8)
		twelve=Add($U, 12)
		twenty=Add($U, 20)
		M $n$U		b $b $U	660 operator
		M $n$four	b $b $four	660 operator
		M $n$eight	b $b $eight	660 operator
		M $n$twelve	b $b $twelve	660 operator
		M n$n$U		b $b $four	660 operator;: sanity w/pdp11 v7
		M n$n$eight	b $b $twelve	660 operator;: ditto
		M nr$n$U	c $c $four	660 operator;: ditto
		M nr$n$eight	c $c $twelve	660 operator;: ditto
		M r$n$U		c $c $U		660 operator
		M r$n$four	c $c $four	660 operator
		M r$n$eight	c $c $eight	660 operator
		M r$n$twelve	c $c $twelve	660 operator
		if [ "$i" = "ut" ];
		then
			M $n$twenty	b $b $twenty 660 operator
			M r$n$twenty	c $b $twenty 660 operator
		fi
		;;
	*)
		echo bad unit for tape in: $i
		;;
	esac
	umask 77
	;;
dnl
dnl *** vax specific targets
dnl
dnl target(all, ut, 0)dnl
dnl target(all, ch, 0)dnl
target(all, nnpfs, 0)dnl
target(all, vscsi, 0)dnl
target(all, diskmap)dnl
target(all, pty, 0)dnl
target(all, bpf, 0, 1, 2, 3, 4, 5, 6, 7)dnl
target(all, bio)dnl
target(all, tun, 0, 1)dnl
dnl target(all, xy, 0, 1, 2, 3)dnl
dnl target(all, hk, 0, 1, 2, 3)dnl
dnl target(all, up, 0, 1, 2, 3)dnl
dnl target(all, rd, 0)dnl
target(all, cd, 0)dnl
target(all, sd, 0, 1, 2, 3)dnl
target(all, vnd, 0)dnl
target( all, ccd, 0)dnl
target( all, hd, 0, 1, 2)dnl
target( all, mt, 0, 1)dnl
target( all, ts, 0, 1)dnl
target( all, uu, 0)dnl
target( all, st, 0, 1)dnl
target( all, dhu, 0)dnl
target( all, dz, 0)dnl
target( all, dl, 0)dnl
dnl target( all, qsc, 0)dnl
