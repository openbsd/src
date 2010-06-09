define(MACHINE,mac68k)dnl
vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.31 2010/06/09 16:10:25 todd Exp $-},
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
dnl *** mac68k specific definitions
dnl
__devitem(ttye, ttye*, ITE bitmapped consoles,ite)dnl
dnl
_TITLE(make)
_DEV(all)
_DEV(ramd)
_DEV(std)
_DEV(local)
_TITLE(dis)
_DEV(ccd, 20, 9)
_DEV(cd, 15, 6)
_DEV(rd, 18, 13)
_DEV(sd, 13, 4)
_DEV(vnd, 19, 8)
_TITLE(tap)
_DEV(ch, 17)
_DEV(st, 14, 5)
_TITLE(term)
_DEV(mac_tty0)
dnl _TITLE(call)
_TITLE(pty)
_DEV(ptm, 52)
_DEV(pty, 5)
_DEV(tty, 4)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 38)
_DEV(wskbd, 39)
_DEV(wsmux, 41)
_TITLE(point)
_DEV(wsmouse, 40)
_TITLE(spec)
_DEV(asc, 36)
_DEV(bio, 49)
_DEV(bpf, 22)
_DEV(fdesc, 21)
_DEV(lkm, 25)
_DEV(pf, 35)
_DEV(rnd, 32)
_DEV(ss, 33)
_DEV(systrace, 50)
_DEV(tun, 24)
_DEV(uk, 34)
_DEV(nnpfs, 51)
_DEV(vscsi, 53)
_DEV(diskmap, 54)
dnl
divert(__mddivert)dnl
dnl
_std(1, 2, 37, 6)
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

asc*)
	M asc$U c 36 $U
	;;
dnl
dnl *** mac68k specific targets
dnl
target(all, bio)dnl
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, vnd, 0, 1, 2, 3)dnl
target(all, ch, 0)dnl
target(all, ss, 0, 1)dnl
target(all, nnpfs, 0)dnl
target(all, vscsi, 0)dnl
target(all, diskmap)dnl
twrget(all, flo, fd, 0, 0B, 0C, 0D, 0E, 0F, 0G, 0H)dnl
twrget(all, flo, fd, 1, 1B, 1C, 1D, 1E, 1F, 1G, 1H)dnl
target(all, pty, 0)dnl
target(all, bpf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)dnl
target(all, tun, 0, 1, 2, 3)dnl
target(all, xy, 0, 1, 2, 3)dnl
target(all, rd, 0)dnl
target(all, cd, 0, 1)dnl
target(all, ccd, 0, 1, 2, 3)dnl
target(ramd, asc, 0)dnl
twrget(ramd, mac_tty0, tty0, 0, 1)dnl
target(ramd, pty, 0)dnl
target(ramd, bio)dnl
