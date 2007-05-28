vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.7 2007/05/28 22:26:02 todd Exp $-},
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
dnl *** luna88k-specific devices
dnl
__devitem(lcd, lcd, front panel LCD display)dnl
_mkdev(lcd, {-lcd-},
{-	M lcd c major_lcd_c 0 644 -})dnl
__devitem(sio, ttya, On-board serial console port)dnl
_mkdev(sio, {-ttya-},
{-	M ttya c major_sio_c 0 660 dialer uucp
	M cuaa c major_sio_c 128 660 dialer uucp -})dnl
dnl
dnl *** MAKEDEV itself
dnl
_TITLE(make)
dnl
dnl all)
dnl
dnl
target(all, ch, 0)dnl
target(all, ss, 0)dnl
target(all, xfs, 0)dnl
target(all, pty, 0)dnl
target(all, bpf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)dnl
target(all, bio)dnl
target(all, tun, 0, 1, 2, 3)dnl
target(all, rd, 0)dnl
target(all, cd, 0, 1)dnl
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, uk, 0)dnl
target(all, vnd, 0, 1, 2, 3)dnl
target(all, ccd, 0, 1, 2, 3)dnl
twrget(all, sio, tty, a)dnl
twrget(all, lcd, lcd)dnl
_DEV(all)
dnl
dnl ramdisk)
dnl
twrget(ramd, sio, tty, a)dnl
target(ramd, pty, 0)dnl
_DEV(ramd)
dnl
_DEV(std)
_DEV(local)
dnl
_TITLE(dis)
_DEV(ccd, 17, 9)
_DEV(cd, 9, 6)
_DEV(rd, 18, 7)
_DEV(sd, 8, 4)
_DEV(vnd, 19, 8)
_TITLE(tap)
_DEV(ch, 44)
_DEV(st, 20, 5)
_TITLE(term)
_DEV(sio, 12)
_TITLE(pty)
_DEV(ptm, 52)
_DEV(pty, 5)
_DEV(tty, 4)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 13)
_DEV(wskbd, 14)
_DEV(wsmux, 16)
_TITLE(point)
_DEV(wsmouse, 15)
_TITLE(spec)
_DEV(bio, 49)
_DEV(bpf, 22)
_DEV(fdesc, 21)
_DEV(lcd, 10)
_DEV(lkm, 24)
_DEV(pf, 39)
_DEV(rnd, 40)
_DEV(ss, 42)
_DEV(systrace, 50)
_DEV(tun, 23)
_DEV(uk, 41)
_DEV(xfs, 51)
dnl
divert(__mddivert)dnl
dnl
_std(1, 2, 43, 3, 6)
	;;

