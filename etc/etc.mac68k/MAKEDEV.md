vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.10 2002/12/05 04:30:21 kjc Exp $-},
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
_DEV(fdesc, 21)
_DEV(local)
_DEV(ramd)
_DEV(st, 14, 5)
_DEV(sd, 13, 4)
_DEV(cd, 15, 6)
_DEV(rd, 18, 13)
_DEV(ch, 17)
_DEV(vnd, 19, 8)
_DEV(ccd, 20, 9)
_TITLE(term)
_DEV(mac_ttye)
_DEV(mac_tty0)
_TITLE(pty)
_DEV(tty, 4)
_DEV(pty, 5)
_TITLE(graph)
_DEV(grf_mac, 10)
_TITLE(spec)
_DEV(bpf, 22)
_DEV(tun, 24)
_DEV(pf, 35)
_DEV(lkm, 25)
_DEV(rnd, 32)
_DEV(uk, 34)
_DEV(ss, 33)
_DEV(xfs, 51)
_DEV(adb, 23)
_DEV(asc, 36)
_DEV(systrace, 50)
dnl
divert(7)dnl
dnl
_std(1, 2, 37, 3, 6)
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
dnl
dnl *** mac68k specific targets
dnl
target(all, ses, 0)dnl
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, vnd, 0, 1, 2, 3)dnl
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
target(all, ccd, 0, 1, 2, 3)dnl
target(ramd, sd, 0, 1, 2, 3)dnl
target(ramd, st, 0, 1)dnl
target(ramd, rd, 0, 1)dnl
target(ramd, adb)dnl
target(ramd, asc, 0)dnl
target(ramd, grf, 0, 1)dnl
target(ramd, ttye, 0)dnl
twrget(ramd, mac_tty0, tty0, 0, 1)dnl
target(ramd, pty, 0)dnl
