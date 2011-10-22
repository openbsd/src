define(MACHINE,sparc)dnl
vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.48 2011/10/22 19:31:23 miod Exp $-},
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
dnl
dnl *** some sparc-specific devices
dnl
__devitem(s64_tzs, tty[a-z]*, Zilog 8530 serial ports,zs)dnl
__devitem(s64_czs, cua[a-z]*, Zilog 8530 serial ports,zs)dnl
_mkdev(s64_tzs, {-tty[a-z]-}, {-u=${i#tty*}
	case $u in
	a) n=0 ;;
	b) n=1 ;;
	c) n=2 ;;
	d) n=3 ;;
	e) n=4;;
	f) n=5;;
	*) echo unknown tty device $i ;;
	esac
	M tty$u c major_s64_tzs_c $n 660 dialer uucp-})dnl
_mkdev(s64_czs, cua[a-z], {-u=${i#cua*}
	case $u in
	a) n=0 ;;
	b) n=1 ;;
	c) n=2 ;;
	d) n=3 ;;
	e) n=4;;
	f) n=5;;
	*) echo unknown cua device $i ;;
	esac
	M cua$u c major_s64_czs_c Add($n, 128) 660 dialer uucp-})dnl
__devitem(presto, presto*, Prestoserve NVRAM memory)dnl
disk_q(presto)dnl
__devitem(apm, apm, Power management device, tctrl)dnl
dnl
dnl *** MAKEDEV itself
dnl
_TITLE(make)
_DEV(all)
_DEV(ramdisk)
_DEV(std)
_DEV(local)
_TITLE(dis)
_DEV(cd, 58, 18)
_DEV(flo, 54, 16)
_DEV(presto, 25, 26)
_DEV(sd, 17, 7)
_DEV(raid, 123, 25)
_DEV(rd, 106, 17)
_DEV(vnd, 110, 8)
_DEV(xd, 42, 10)
_DEV(xy, 9, 3)
_TITLE(tap)
_DEV(ch, 19)
_DEV(st, 18, 11)
_TITLE(term)
_DEV(s64_czs, 12)
_DEV(mag, 100)
_DEV(spif, 102)
_DEV(com, 36)
_DEV(s64_tzs, 12)
_TITLE(pty)
_DEV(ptm, 125)
_DEV(pty, 21)
_DEV(tty, 20)
_TITLE(prn)
_DEV(bpp, 104)
_DEV(bppsp, 103)
_DEV(bppmag, 101)
_TITLE(cons)
_DEV(wsdisp, 78)
_DEV(wscons)
_DEV(wskbd, 79)
_DEV(wsmux, 81)
_TITLE(point)
_DEV(wsmouse, 80)
_TITLE(spec)
_DEV(apm, 30)
_DEV(au, 69)
_DEV(bio, 124)
_DEV(bpf, 105)
_DEV(diskmap, 129)
_DEV(fdesc, 24)
_DEV(hotplug, 131)
_DEV(lkm, 112)
_DEV(nnpfs, 51)
_DEV(oppr)
_DEV(pf, 59)
_DEV(pppx, 130)
_DEV(rnd, 119)
_DEV(systrace, 50)
_DEV(tun, 111)
_DEV(uk, 120)
_DEV(vscsi, 128)
dnl
divert(__mddivert)dnl
dnl
ramdisk)
	_recurse std bpf0 bio diskmap
	_recurse fd0 sd0 sd1 sd2 rd0 cd0
	;;

_std(2, 3, 122, 16)
	M eeprom	c 3 11	640 kmem
	M openprom	c 70 0	640 kmem
	;;
dnl
dnl *** some sparc-specific targets
dnl
twrget(all, au, audio, 0, 1, 2)dnl
twrget(all, s64_tzs, tty, a, b, c, d)dnl
twrget(all, s64_czs, cua, a, b, c, d)dnl
target(all, bio)dnl
target(all, ch, 0)dnl
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
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, vnd, 0, 1, 2, 3)dnl
target(all, bpp, 0)dnl
target(all, presto, 0)dnl
twrget(wscons, wscons, ttyD, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
twrget(wscons, wscons, ttyE, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
twrget(wscons, wscons, ttyF, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
twrget(wscons, wscons, ttyG, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
twrget(wscons, wscons, ttyH, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
twrget(wscons, wscons, ttyI, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
