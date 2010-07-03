define(MACHINE,mvme68k)dnl
vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.28 2010/07/03 03:59:15 krw Exp $-},
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
dnl *** mvme68k-specific devices
dnl
__devitem(mvme_tzs, ttya-d, On-board serial ports,zs)dnl
__devitem(mvme_czs, cuaa-d, On-board call-up devices,zs)dnl
_mkdev(mvme_tzs, {-tty[a-z]-}, {-u=${i#tty*}
	case $u in
	a) n=0 ;;
	b) n=1 ;;
	c) n=2 ;;
	d) n=3 ;;
	*) echo unknown tty device $i ;;
	esac
	case $u in
	a|b|c|d)
		M tty$u c major_mvme_tzs_c $n 660 dialer uucp
		;;
	esac-})dnl
_mkdev(mvme_czs, cua[a-z], {-u=${i#cua*}
	case $u in
	a) n=0 ;;
	b) n=1 ;;
	c) n=2 ;;
	d) n=3 ;;
	*) echo unknown cua device $i ;;
	esac
	case $u in
	a|b|c|d)
		M cua$u c major_mvme_czs_c Add($n, 128) 660 dialer uucp
		;;
	esac-})dnl
__devitem(cl, tty0*, CL-CD2400 serial ports)dnl
_mkdev(cl, {-tty0*-}, {-u=${i#tty0*}
	case $u in
	0|1|2|3)
		M tty0$u c major_cl_c $u 660 dialer uucp
		M cua0$u c major_cl_c Add($u, 128) 660 dialer uucp
		;;
	*) echo unknown tty device $i ;;
	esac-})dnl
__devitem(ttyd, ttyd*, MC68681 serial ports,nothing)dnl
_mkdev(ttyd, {-ttyd[01]-}, {-u=${i#ttyd*}
	case $u in
	0|1)
		M ttyd$u c major_ttyd_c $u 660 dialer uucp
		M cuad$u c major_ttyd_c Add($u, 128) 660 dialer uucp
		;;
	*) echo unknown tty device $i ;;
	esac-})dnl
__devitem(ttyw, ttyw*, WG CL-CD2400 serial ports,nothing)dnl
_mkdev(ttyw, {-ttyw*-}, {-u=${i#ttyw*}
	case $u in
	0|1|2|3)
		M ttyw$u c major_ttyw_c $u 660 dialer uucp
		M cuaw$u c major_ttyw_c Add($u, 128) 660 dialer uucp
		;;
	*) echo unknown tty device $i ;;
	esac-})dnl
__devitem(lp, par0, On-board printer port,nothing)dnl
_mkdev(lp, {-lp*-}, {-u=${i#lp*}
	case $u in
	0) M par$u c major_lp_c $u 600;;
	*) echo unknown lp device $i ;;
	esac-})dnl
__devitem(sram, sram0, On-board static memory)dnl
_mkdev(sram, sram0, {-M sram0 c major_sram_c 0 640 kmem-})dnl
__devitem(nvram, nvram0, On-board non-volatile memory)dnl
_mkdev(nvram, nvram0, {-M nvram0 c major_nvram_c 0 640 kmem-})dnl
__devitem(flash, flash0, On-board flash memory)dnl
_mkdev(flash, flash0, {-M flash0 c major_flash_c 0 640 kmem-})dnl
__devitem(vmes, vmes0, VMEbus D16 space)dnl
_mkdev(vmes, vmes0, {-M vmes0 c major_vmes_c 0 640 kmem-})dnl
__devitem(vmel, vmel0, VMEbus D32 space)dnl
_mkdev(vmel, vmel0, {-M vmel0 c major_vmel_c 0 640 kmem-})dnl
dnl
dnl *** MAKEDEV itself
dnl
_TITLE(make)
dnl
dnl all)
dnl
target(all, sram, 0)dnl
target(all, nvram, 0)dnl
target(all, flash, 0)dnl
target(all, vmes, 0)dnl
target(all, vmel, 0)dnl
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
twrget(all, mvme_tzs, tty, a, b, c, d)dnl
twrget(all, mvme_czs, cua, a, b, c, d)dnl
twrget(all, cl, tty0, 0, 1, 2, 3)dnl
target(all, ttyd, 0, 1)dnl
target(all, ttyw, 0, 1, 2, 3)dnl
dnl target(all, lp, 0)dnl
_DEV(all)
dnl
dnl ramdisk)
dnl
twrget(ramd, mvme_tzs, tty, a)dnl
target(ramd, pty, 0)dnl
target(ramd, bio)dnl
target(ramd, diskmap)dnl
_DEV(ramd)
dnl
_DEV(std)
_DEV(local)
dnl
_TITLE(dis)
_DEV(ccd, 17, 5)
_DEV(cd, 9, 8)
_DEV(rd, 18, 9)
_DEV(sd, 8, 4)
_DEV(vnd, 19, 6)
_TITLE(tap)
_DEV(ch, 44)
_DEV(st, 20, 7)
_TITLE(term)
_DEV(mvme_czs, 12)
_DEV(mvme_tzs, 12)
_DEV(cl, 13)
_DEV(ttyd, 14)
_DEV(ttyw, 30)
_TITLE(pty)
_DEV(ptm, 52)
_DEV(pty, 5)
_DEV(tty, 4)
_TITLE(prn)
_DEV(lp, 28)
_TITLE(spec)
_DEV(bio, 27)
_DEV(bpf, 22)
_DEV(fdesc, 21)
_DEV(flash, 11)
_DEV(lkm, 24)
_DEV(nvram, 10)
_DEV(pf, 39)
_DEV(rnd, 40)
_DEV(sram, 7)
_DEV(systrace, 50)
_DEV(tun, 23)
_DEV(uk, 41)
_DEV(vmel, 31)
_DEV(vmes, 32)
_DEV(nnpfs, 51)
_DEV(vscsi, 53)
_DEV(diskmap, 54)
dnl
divert(__mddivert)dnl
dnl
_std(1, 2, 43, 6)
	;;

