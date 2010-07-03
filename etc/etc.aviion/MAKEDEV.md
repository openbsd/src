define(MACHINE,aviion)dnl
vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.11 2010/07/03 03:59:15 krw Exp $-},
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
dnl *** mvme88k-specific devices
dnl
__devitem(dart, ttya-c, on-board serial and mouse ports)dnl
_mkdev(dart, {-tty[a-c]-}, {-u=${i#tty*}
	case $u in
	a) n=0 ;;
	b) n=1 ;;
	c) n=2 ;;
	*) echo unknown tty device $i ;;
	esac
	case $u in
	a|b|c)
		M tty$u c major_dart_c $n 660 dialer uucp
		M cua$u c major_dart_c Add($n, 128) 660 dialer uucp
		;;
	esac-})dnl
__devitem(nvram, nvram0, On-board non-volatile memory)dnl
_mkdev(nvram, nvram0, {-M nvram0 c major_nvram_c 0 640 kmem-})dnl
dnl
dnl *** MAKEDEV itself
dnl
_TITLE(make)
dnl
dnl all)
dnl
target(all, nvram, 0)dnl
dnl
target(all, ch, 0)dnl
target(all, nnpfs, 0)dnl
target(all, vscsi, 0)dnl
target(all, diskmap)dnl
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
twrget(all, dart, tty, a, b, c)dnl
_DEV(all)
dnl
dnl ramdisk)
dnl
twrget(ramd, dart, tty, a)dnl
target(ramd, pty, 0)dnl
target(ramd, bio)dnl
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
_DEV(dart, 12)
_TITLE(pty)
_DEV(ptm, 52)
_DEV(pty, 5)
_DEV(tty, 4)
_TITLE(spec)
_DEV(bio, 49)
_DEV(bpf, 22)
_DEV(fdesc, 21)
_DEV(lkm, 24)
_DEV(nvram, 10)
_DEV(pf, 39)
_DEV(rnd, 40)
_DEV(systrace, 50)
_DEV(tun, 23)
_DEV(uk, 41)
_DEV(nnpfs, 51)
_DEV(vscsi, 53)
_DEV(diskmap, 54)
dnl
divert(__mddivert)dnl
dnl
_std(1, 2, 43, 6)
	;;

