vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.5 2002/02/14 13:29:20 todd Exp $-},
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
dnl
_TITLE(make)
_DEV(all)
_DEV(std)
_DEV(local)
_TITLE(tap)
_DEV(st, 20, 5)
_TITLE(dis)
_DEV(aflo, 18, 2)
_DEV(sd, 8, 4)
_DEV(rd, 41, 16)
_DEV(ccd, 7, 8)
_DEV(wd, 37, 0)
_DEV(cd, 9, 7, 8)
_DEV(vnd, 19, 6)
_TITLE(cons)
_DEV(ttye)
_TITLE(point)
_DEV(amouse)
_TITLE(term)
_DEV(com, 12)
_DEV(ttyA)
_DEV(attyB)
_TITLE(pty)
_DEV(tty, 4)
_DEV(pty, 5)
_TITLE(prn)
_DEV(par)
_DEV(lpt, 33)
_DEV(lpa)
_TITLE(spec)
_DEV(grf_amiga, 10)
_DEV(kbd, 14)
_DEV(joy, 43)
_DEV(akbd, 14)
_DEV(view)
_DEV(aconf)
_DEV(lkm, 24)
_DEV(bpf, 22)
_DEV(tun, 23)
_DEV(pf, 34)
_DEV(ss, 25)
_DEV(uk, 36)
_DEV(rnd, 35)
_DEV(au, 39)
_DEV(xfs, 51)
_DEV(ch, 40)
_DEV(altq, 52)
_DEV(fdesc, 21)
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std kbd pty0 tty00 ttyA0 ttyA1 ttyB0 ttyB1
	_recurse ttye0 ttye1 ttye2 ttye3 ttye4 ttye5 ttye6
	_recurse cd0 cd1 sd0 sd1 sd2 sd3 st0 st1
	_recurse fd0 fd1 wd0 wd1 rd0 random
	;;

_std(1, 2, 42, 3, 6)
	M reload	c 2 20 600
	;;

ttyA[01])
	M ttyA$U c 17 $U 660 dialer uucp
	M ttyM$U c 17 Add($U, 128) 660 dialer uucp
	;;

par*)
	case $U in
	0)
		M par$U c 11 $U 600
		;;
	*)
		echo bad unit for par in: $i
		;;
	esac
	;;

ttye*)
	case $U in
	0|1|2|3|4|5|6)
		M ttye$U c 13 $U 600
		;;
	*)
		echo bad unit for ttye in: $i
		;;
	esac
	;;

grf*)
	case $U in
	0|1|2|3|4|5|6)
		M grf$U c 10 $U
		;;
	*)
		echo bad unit for grf in: $i
		;;
	esac
# for those that need it, also make overlay and image devices:
	case $U in
	4)
		M grfov$U c 10 Add($U, 16) 600
		M grfim$U c 10 Add($U, 32) 600
		;;
	esac
	;;

mouse*)
	case $U in
	0|1)
		M mouse$U c 15 $U
		if [ $U = 0 ]
		then 
			MKlist="$MKlist;rm -f mouse; ln -s mouse$U mouse"
		fi
		;;
	*)
		echo bad unit for mouse in: $i
		;;
	esac
	;;

view*)
	case $U in
	00|01|02|03|04|05|06|07|08|09)
		M view$U c 16 $U
		;;
	*)
		echo bad unit for view in: $i
		;;
	esac
	;;

ttyB*)
	M ttyB$U c 32 $U 660 dialer uucp
	M cuaB$U c 32 Add($U, 128) 660 dialer uucp
	;;
