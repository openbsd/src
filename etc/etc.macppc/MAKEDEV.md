vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.18 2004/04/11 18:05:23 millert Exp $-},
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
_DEV(ch, 10)
_DEV(st, 20, 5)
_TITLE(term)
_DEV(com, 7)
_TITLE(pty)
_DEV(ptm, 77)
_DEV(pty, 5)
_DEV(tty, 4)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 67)
_DEV(wskbd, 68)
_DEV(wsmux, 70)
_TITLE(point)
_DEV(wsmouse, 69)
_TITLE(usb)
_DEV(uall)
_DEV(ttyU, 66)
_DEV(ugen, 63)
_DEV(uhid, 62)
_DEV(ulpt, 64)
_DEV(urio, 65)
_DEV(usb, 61)
_DEV(uscan, 74)
_TITLE(spec)
_DEV(apm, 25)
_DEV(au, 44)
_DEV(bktr, 75)
_DEV(bpf, 22)
_DEV(cry, 47)
_DEV(fdesc, 21)
_DEV(iop, 73)
_DEV(lkm, 24)
_DEV(pci, 71)
_DEV(pf, 39)
_DEV(radio, 76)
_DEV(rnd, 40)
_DEV(ss, 42)
_DEV(systrace, 50)
_DEV(tun, 23)
_DEV(tuner, 75)
_DEV(uk, 41)
_DEV(xfs, 51)
dnl
divert(__mddivert)dnl
dnl
_std(1, 2, 43, 3, 6)
	M xf86		c 2 4 600
	M reload	c 2 20 640 kmem
	;;

dnl
dnl *** macppc specific targets
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
target(ramd, sd, 0, 1, 2, 3, 4)dnl
target(ramd, wd, 0, 1, 2, 3, 4)dnl
target(ramd, st, 0, 1)dnl
target(ramd, cd, 0, 1)dnl)dnl
target(ramd, rd, 0)dnl
target(ramd, tty0, 0, 1)dnl
target(ramd, pty, 0)dnl
