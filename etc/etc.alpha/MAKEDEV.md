vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.15 2002/12/05 04:30:21 kjc Exp $-},
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
_DEV(ramdisk)
_DEV(std)
_DEV(loc)
_TITLE(tap)
_DEV(st, 12, 2)
_TITLE(dis)
_DEV(sd, 8, 8)
_DEV(cd, 13, 3)
_DEV(vnd, 9, 9)
_DEV(ccd, 27, 7)
_DEV(wd, 36, 0)
_DEV(rd, 28, 6)
_DEV(raid, 43, 16)
_DEV(flo, 37, 4)
_TITLE(term)
_DEV(ttyB)
_DEV(wscons)
_DEV(wsdisp, 25)
_DEV(wskbd, 29)
_DEV(wsmux, 56)
_DEV(com, 26)
_DEV(ttyc, 38)
_TITLE(point)
_DEV(wsmouse, 30)
_TITLE(pty)
_DEV(tty, 4)
_DEV(pty, 5)
_TITLE(prn)
_DEV(lpt, 31)
_DEV(lpa)
_TITLE(usb)
_DEV(usb, 45)
_DEV(uhid, 46)
_DEV(ulpt, 47)
_DEV(ugen, 48)
_DEV(utty, 49)
_TITLE(spec)
_DEV(ch, 14)
_DEV(pf, 35)
_DEV(bpf, 11)
_DEV(iop, 54)
_DEV(pci, 52)
_DEV(usbs)
_DEV(fdesc, 10)
_DEV(lkm, 16)
_DEV(tun, 7)
_DEV(mmcl)
_DEV(kbd)
_DEV(mouse)
_DEV(rnd, 34)
_DEV(uk, 33)
_DEV(ss, 32)
_DEV(xfs, 51)
_DEV(au, 24)
_DEV(speak, 40)
_DEV(rmidi, 41)
_DEV(music, 42)
_DEV(systrace, 50)
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std fd0 wd0 wd1 wd2 sd0 sd1 sd2
	_recurse st0 cd0 ttyC0 random rd0
	;;

_std(1, 2, 39, 3, 6)
	M xf86		c 2 4 600
	;;

ttyB*|ttyc*)
	U=${i##tty?}
	case $i in
	ttyB*)	type=B major=15 minor=Mult($U, 2);;
	ttyc*)	type=c major=38 minor=$U;;
	esac
	M tty$type$U c $major $minor 660 dialer uucp
	M cua$type$U c $major Add($minor, 128) 660 dialer uucp
	;;

mmclock)
	M mmclock c 28 0 444
	;;
dnl
dnl *** alpha specific targets
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
target(ramd, sd, 0, 1, 2)dnl
target(ramd, wd, 0)dnl
target(ramd, tty0, 0, 1)dnl
target(ramd, st, 0)dnl
target(ramd, cd, 0)dnl
target(ramd, ttyB, 0, 1)dnl
target(ramd, ttyE, 0, 1)dnl
