vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.7 2004/05/30 08:17:18 grange Exp $-},
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
dnl
_TITLE(make)
_DEV(all)
_DEV(ramdisk)
_DEV(std)
_DEV(local)
_TITLE(dis)
_DEV(ccd, 18, 16)
_DEV(cd, 15, 6)
_DEV(flo, 9, 2)
_DEV(mcd, 39, 7)
_DEV(raid, 54, 19)
_DEV(rd, 47, 17)
_DEV(sd, 13, 4)
_DEV(vnd, 41, 14)
_DEV(wd, 3, 0)
_TITLE(tap)
_DEV(ch, 17)
_DEV(st, 14, 5)
_DEV(wt, 10, 3)
_TITLE(term)
_DEV(com, 8)
_DEV(ttyc, 38)
_TITLE(call)
_DEV(tun, 40)
_TITLE(pty)
_DEV(ptm, 81)
_DEV(pty, 6)
_DEV(tty, 5)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 12)
_DEV(wskbd, 67)
_DEV(wsmux, 69)
_TITLE(point)
_DEV(wsmouse, 68)
_TITLE(prn)
_DEV(lpa)
_DEV(lpt, 16)
_TITLE(usb)
_DEV(uall)
_DEV(ttyU, 66)
_DEV(ugen, 63)
_DEV(uhid, 62)
_DEV(ulpt, 64)
_DEV(urio, 65)
_DEV(usb, 61)
_DEV(uscan, 77)
_TITLE(spec)
_DEV(apm, 21)
_DEV(au, 42)
_DEV(bktr, 49)
_DEV(bpf, 23)
_DEV(cry, 70)
_DEV(fdesc, 22)
_DEV(gpr, 80)
_DEV(hotplug, 82)
_DEV(iop, 75)
_DEV(joy, 26)
_DEV(lkm, 28)
_DEV(music, 53)
_DEV(pci, 72)
_DEV(pctr, 46)
_DEV(pf, 73)
_DEV(radio, 76)
_DEV(rmidi, 52)
_DEV(rnd, 45)
_DEV(ses, 24)
_DEV(speak, 27)
_DEV(ss, 19)
_DEV(systrace, 78)
_DEV(tuner, 49)
_DEV(uk, 20)
_DEV(wdt, 55)
_DEV(xfs, 51)
dnl
divert(__mddivert)dnl
dnl
ramdisk)
	_recurse std bpf0 fd0 wd0 wd1 wd2 sd0 sd1 sd2 tty00 tty01 rd0
	_recurse st0 cd0 ttyC0 random wskbd0
	;;

_std(1, 2, 50, 4, 7)
	M xf86		c 2 4 600
	;;

ttyc*)
	M ttyc$U c 38 $U 660 dialer uucp
	M cuac$U c 38 Add($U, 128) 660 dialer uucp
	;;
dnl
dnl amd64 specific targets
dnl
target(all, ses, 0)dnl
target(all, ch, 0)dnl
target(all, ss, 0, 1)dnl
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
target(all, vnd, 0, 1, 2, 3)dnl
target(all, ccd, 0, 1, 2, 3)dnl
target(all, bktr, 0)dnl
target(ramd, tty0, 0, 1, 2, 3)dnl
twrget(ramd, wsdisp, ttyC, 0)dnl
target(ramd, wt, 0)dnl
target(ramd, fd, 0)dnl
target(ramd, rd, 0)dnl
target(ramd, wd, 0, 1, 2, 3)dnl
target(ramd, sd, 0, 1, 2, 3)dnl
target(ramd, cd, 0, 1)dnl
target(ramd, st, 0, 1)dnl
target(ramd, mcd, 0)dnl
