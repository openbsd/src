vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.10 2002/10/16 15:48:31 todd Exp $-},
etc.MACHINE)dnl
dnl
dnl Copyright (c) 2001 Todd T. Fries <todd@OpenBSD.org>
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. The name of the author may not be used to endorse or promote products
dnl    derived from this software without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
dnl INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
dnl AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
dnl THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
dnl EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
dnl PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
dnl OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
dnl WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
dnl OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
dnl ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl
dnl *** hp300 specific device scripts/descriptions
dnl
_mkdev(st_hp300, ct*|mt*|st*,
{-case $i in
	ct*) name=ct blk=major_ct_b chr=major_ct_c;;
	mt*) name=mt blk=major_mt_b chr=major_mt_c;;
	st*) name=st blk=major_st_hp300_b chr=major_st_hp300_c;;
	esac
	case $U in
	[0-7])
		four=Add($U, 4) eight=Add($U, 8)
		twelve=Add($U, 12) twenty=Add($U, 20)
		M r$name$U	c $chr $U 660 operator
		M r$name$four	c $chr $four 660 operator
		M r$name$eight	c $chr $eight 660 operator
		M r$name$twelve	c $chr $twelve 660 operator
		MKlist="$MKlist;ln r$name$four nr$name$U";: sanity w/pdp11 v7
		MKlist="$MKlist;ln r$name$twelve nr$name$eight";: ditto
		RMlist="$RMlist nr$name$U nr$name$eight"
		;;
	*)
		echo bad unit for tape in: $1
		;;
	esac-})dnl
__devitem(st_hp300, st*, Exabyte tape)dnl
__devitem(grf, grf*, raw interface to HP300 graphics devices)dnl
dnl
dnl
_TITLE(make)
_DEV(all)
_DEV(std)
_DEV(local)
_TITLE(tap)
_DEV(ct, 7, 0)
_DEV(mt, 16, 1)
_DEV(st_hp300, 20, 6)
_TITLE(dis)
_DEV(ccd, 17, 5)
_DEV(hd, 9, 2)
_DEV(sd, 8, 4)
_DEV(vnd, 19, 6)
_DEV(rd, 34, 8)
_TITLE(termp)
_DEV(dca, 12)
_DEV(dcm, 15)
_DEV(apci)
_TITLE(pty)
_DEV(tty, 4)
_DEV(pty, 5)
_TITLE(prn)
_DEV(ppi, 11)
_TITLE(call)
_TITLE(spec)
_DEV(fdesc, 21)
_DEV(grf, 10)
_DEV(ite)
_DEV(hil, 14)
_DEV(bpf, 22)
_DEV(tun, 23)
_DEV(pf, 33)
_DEV(lkm, 24)
_DEV(rnd, 32)
_DEV(xfs, 51)
_DEV(altq, 52)
_DEV(systrace, 50)
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std ct0 ct1 st0 st1 hd0 hd1 hd2 hd3 hd4
	_recurse sd0 sd1 sd2 sd3 sd4 rd0 pty0
	_recurse hil grf0 apci0 ite0 dca0 dcm0 dcm1
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

ite*)
	case $U in
	0|1|2|3)
		M ttye$U c 13 $U 600
		;;
	*)
		echo bad unit for ite in: $i
		;;
	esac
	;;

grf*)
	case $U in
	0|1|2|3)
		M grf$U c major_grf_c $U 600
		;;
	*)
		echo bad unit for grf in: $i
		;;
	esac
	;;

hil)
	for U in 0 1 2 3 4 5 6 7
	do
		M hil$U c 14 $U 600
	done
	MKlist="$MKlist;ln hil1 keyboard"
	MKlist="$MKlist;ln hil3 locator"
	RMlist="$RMlist keyboard locator"
	;;
dnl
target(all, ses, 0)dnl
target(all, ch, 0)dnl
target(all, ss, 0, 1)dnl
target(all, xfs, 0)dnl
twrget(all, flo, fd, 0, 0B, 0C, 0D, 0E, 0F, 0G, 0H)dnl
twrget(all, flo, fd, 1, 1B, 1C, 1D, 1E, 1F, 1G, 1H)dnl
target(all, pty, 0, 1, 2)dnl
target(all, bpf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)dnl
target(all, tun, 0, 1, 2, 3)dnl
target(all, xy, 0, 1, 2, 3)dnl
target(all, rd, 0)dnl
target(all, cd, 0, 1)dnl
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, vnd, 0, 1, 2, 3)dnl
target(all, ccd, 0, 1, 2, 3)dnl
target( all, grf, 0)dnl
dnl XXX target( all, hil, 0, 1, 2, 3, 4, 5, 6, 7)dnl
target( all, hil, )dnl
twrget( all, st_hp300, st, 0, 1)dnl
target( all, dca, 0, 1)dnl
target( all, dcm, 0, 1, 2, 3)dnl
target( all, hd, 0, 1, 2)dnl
target( all, ct, 0, 1)dnl
target( all, ite, 0)dnl
target(ramd, ct, 0, 1)dnl
target(ramd, hd, 0, 1, 2)dnl
target(ramd, sd, 0, 1, 2)dnl
target(ramd, rd, 0, 1)dnl
target(ramd, pty, 0)dnl
target(ramd, hil, )dnl
target(ramd, grf, 0)dnl
target(ramd, apci, 0)dnl
target(ramd, ite, 0)dnl
target(ramd, dca, 0)dnl
target(ramd, dcm, 0, 1)dnl
target(ramd, bpf, 0, 1)dnl
target(ramd, tun, 0, 1)dnl
