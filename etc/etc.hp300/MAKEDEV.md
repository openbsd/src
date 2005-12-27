vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.26 2005/12/27 18:50:26 miod Exp $-},
etc.MACHINE)dnl
dnl
dnl Copyright (c) 2001-2004 Todd T. Fries <todd@OpenBSD.org>
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
dnl *** hp300 specific device scripts/descriptions
dnl
__devitem(ct, ct*, HP300 HP-IB cartridge tape drives,{-\&ct-})dnl
__devitem(hd, {-hd*-}, HP300 HP-IB disks)dnl
_mkdev(ct, ct*|mt*,
{-case $i in
	ct*) name=ct blk=major_ct_b chr=major_ct_c;;
	mt*) name=mt blk=major_mt_b chr=major_mt_c;;
	esac
	case $U in
	[0-7])
		four=Add($U, 4) eight=Add($U, 8)
		twelve=Add($U, 12) twenty=Add($U, 20)
		M r$name$U	c $chr $U 660 operator
		M r$name$four	c $chr $four 660 operator
		M r$name$eight	c $chr $eight 660 operator
		M r$name$twelve	c $chr $twelve 660 operator
		MKlist[${#MKlist[*]}]=";ln r$name$four nr$name$U";: sanity w/pdp11 v7
		MKlist[${#MKlist[*]}]=";ln r$name$twelve nr$name$eight";: ditto
		RMlist[${#RMlist[*]}]="nr$name$U nr$name$eight"
		;;
	*)
		echo bad unit for tape in: $1
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
_DEV(ccd, 17, 5)
_DEV(cd, 18, 9)
_DEV(hd, 9, 2)
_DEV(rd, 34, 8)
_DEV(sd, 8, 4)
_DEV(vnd, 19, 6)
_TITLE(tap)
_DEV(ch, 39)
_DEV(ct, 7, 0)
_DEV(mt, 16, 1)
_DEV(st, 20, 7)
_TITLE(term)
_DEV(apci)
_DEV(dca, 12)
_DEV(dcm, 15)
dnl _TITLE(call)
_TITLE(pty)
_DEV(ptm, 52)
_DEV(pty, 5)
_DEV(tty, 4)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 40)
_DEV(wskbd, 41)
_DEV(wsmux, 43)
_TITLE(point)
_DEV(wsmouse, 42)
_TITLE(prn)
_DEV(ppi, 11)
_TITLE(spec)
_DEV(bpf, 22)
_DEV(fdesc, 21)
_DEV(lkm, 24)
_DEV(pf, 33)
_DEV(rnd, 32)
_DEV(ss, 38)
_DEV(systrace, 50)
_DEV(tun, 23)
_DEV(uk, 37)
_DEV(xfs, 51)
dnl
divert(__mddivert)dnl
dnl
ramdisk)
	_recurse std ct0 ct1 st0 st1 hd0 hd1 hd2 hd3 hd4
	_recurse sd0 sd1 sd2 sd3 sd4 cd0 cd1 rd0 pty0
	_recurse apci0 dca0 dcm0 dcm1
	_recurse bpf0 bpf1 tun0 tun1 lkm random
	;;

_std(1, 2, 36, 3, 6)
	;;

dca*)
	case $U in
	0|1|2|3)
		M tty$U c major_dca_c $U 660 dialer uucp
		M cua$U c major_dca_c Add($U, 128) 660 dialer uucp
		;;
	*)
		echo bad unit for dca in: $i
		;;
	esac
	;;

dcm*)
	case $U in
	0|1|2|3)
		u="$(( $U * 4 ))"
		i=0
		while [ $i -lt 4 ]
		do
			n="$(( $u + $i ))"
			ext=`hex $n`

			M tty0${ext} c 15 ${n} 660 dialer uucp
			M cua0${ext} c 15 "$(( $n + 128 ))" 660 dialer uucp

			i="$(( $i + 1 ))"
		done
		;;
	*)
		echo bad unit for dcm in: $i
		;;
	esac
	;;

apci*)
	# There exists only one Frodo ASIC per HP9000/400 SPU.
	case $U in
	0)
		for i in 0 1 2 3; do
			M ttya${i} c 35 ${i} 660 dialer uucp
			M cuaa${i} c 35 Add($i, 128) 660 dialer uucp
		done
		;;
	*)
		echo bad unit for apci in: $i
		;;
	esac
	;;

ppi*)
	case $U in
	0|1|2|3)
		M ppi$U c major_ppi_c $U 600
		;;
	*)
		echo bad unit for ppi in: $i
		;;
	esac
	;;

dnl
target(all, ch, 0)dnl
target(all, ss, 0)dnl
target(all, xfs, 0)dnl
twrget(all, flo, fd, 0, 0B, 0C, 0D, 0E, 0F, 0G, 0H)dnl
twrget(all, flo, fd, 1, 1B, 1C, 1D, 1E, 1F, 1G, 1H)dnl
target(all, pty, 0)dnl
target(all, bpf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)dnl
target(all, tun, 0, 1, 2, 3)dnl
target(all, xy, 0, 1, 2, 3)dnl
target(all, rd, 0)dnl
target(all, cd, 0, 1)dnl
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, st, 0, 1)dnl
target(all, uk, 0)dnl
target(all, vnd, 0, 1, 2, 3)dnl
target(all, ccd, 0, 1, 2, 3)dnl
target( all, dca, 0, 1)dnl
target( all, dcm, 0, 1, 2, 3)dnl
target( all, hd, 0, 1, 2)dnl
target( all, ct, 0, 1)dnl
target(ramd, cd, 0, 1)dnl
target(ramd, ct, 0, 1)dnl
target(ramd, hd, 0, 1, 2)dnl
target(ramd, sd, 0, 1, 2)dnl
target(ramd, st, 0, 1)dnl
target(ramd, rd, 0, 1)dnl
target(ramd, pty, 0)dnl
target(ramd, apci, 0)dnl
target(ramd, dca, 0)dnl
target(ramd, dcm, 0, 1)dnl
target(ramd, bpf, 0, 1)dnl
target(ramd, tun, 0, 1)dnl
