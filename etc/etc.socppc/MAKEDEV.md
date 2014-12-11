define(MACHINE,socppc)dnl
vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.26 2014/12/11 19:48:03 tedu Exp $-},
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
dnl __devitem(apm, apm, Power management device)dnl
_TITLE(make)
_DEV(all)
_DEV(ramd)
_DEV(std)
_DEV(local)
_TITLE(dis)
_DEV(cd, 9, 3)
_DEV(rd, 17, 17)
_DEV(sd, 8, 2)
_DEV(vnd, 19, 14)
_DEV(wd, 11, 0)
_TITLE(tap)
_DEV(ch, 10)
_DEV(st, 20, 5)
_TITLE(term)
_DEV(com, 26)
_TITLE(pty)
_DEV(ptm, 77)
_DEV(pty, 5)
_DEV(tty, 4)
_TITLE(cons)
_DEV(wsdisp, 67)
_DEV(wscons)
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
_DEV(usb, 61)
_TITLE(spec)
dnl _DEV(apm, 25)
dnl _DEV(au, 44)
dnl _DEV(bio, 80)
dnl dnl _DEV(bktr, 75)
_DEV(bpf, 22)
_DEV(diskmap, 82)
_DEV(fdesc, 21)
_DEV(fuse, 85)
dnl _DEV(gpio, 79)
_DEV(hotplug, 84)
_DEV(pci, 71)
_DEV(pf, 39)
_DEV(pppx, 83)
dnl _DEV(radio, 76)
_DEV(rnd, 40)
_DEV(systrace, 50)
_DEV(tun, 23)
dnl _DEV(tuner, 75)
dnl _DEV(uk, 41)
_DEV(vi, 44)
_DEV(vscsi, 78)
dnl
divert(__mddivert)dnl
dnl
_std(1, 2, 43, 6)
	;;

dnl
dnl *** socppc specific targets
dnl
twrget(all, au, audio, 0, 1, 2)dnl
target(all, ch, 0)dnl
target(all, vscsi, 0)dnl
target(all, diskmap)dnl
target(all, pty, 0)dnl
target(all, bpf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)dnl
target(all, tun, 0, 1, 2, 3)dnl
target(all, rd, 0)dnl
target(all, cd, 0, 1)dnl
target(all, sd, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)dnl
target(all, vnd, 0, 1, 2, 3)dnl
dnl target(all, gpio, 0, 1, 2)dnl
dnl target(all, bio)dnl
target(ramd, diskmap)dnl
target(ramd, random)dnl
