vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.1 2002/01/08 03:14:50 todd Exp $-},
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
_DEV(fdesc,21)
_DEV(local)
_DEV(ramd)
_DEV(st,14,5)
_DEV(sd,13,4,8)
_DEV(cd,15,6,8)
_DEV(ch,17)
_DEV(vnd,19,8,8)
_DEV(ccd,20,9,8)
_TITLE(term)
_DEV(mac_ttye)
_DEV(mac_tty0)
_TITLE(pty)
_DEV(tty,4)
_DEV(pty,5)
_TITLE(graph)
_DEV(mac_grf,10)
_TITLE(spec)
_DEV(bpf,22)
_DEV(tun,24)
_DEV(pf,35)
_DEV(altq,52)
_DEV(lkm,25)
_DEV(rnd,32)
_DEV(uk,34)
_DEV(ss,33)
_DEV(xfs,51)
_DEV(adb,23)
_DEV(asc,36)
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std
	_recurse sd0 sd1 sd2 sd3 st0 st1 cd0 cd1
	_recurse adb asc0 grf0 grf1 ttye0
	_recurse tty00 tty01 pty0
	;;

_std(1,2,37,3,6)
	M reload	c 2 20 640 kmem
	;;

tty0*)
	case $U in
	00|01)
		M tty$U c 12 $U 660 dialer uucp
		;;
	*)
		echo bad unit for serial tty in: $i
		;;
	esac
	;;

ttye*)
	case $U in
	0|1)
		M ttye$U c 11 $U 600
		;;
	*)
		echo bad unit for ttye in: $i
		;;
	esac
	;;

grf*)
	case $U in
	0|1|2|3)
		M grf$U c 10 $U
		;;
	*)
		echo bad unit for grf in: $i
		;;
	esac
	;;

adb)
	M adb c 23 0
	;;

asc*)
        M asc$U c 36 $U
        ;;
