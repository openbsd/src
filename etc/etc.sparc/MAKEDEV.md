vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.16 2002/12/05 04:30:21 kjc Exp $-},
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
dnl
dnl *** some sparc-specific devices
dnl
__devitem(s64_tzs, tty[a-z]*, Zilog 8530 Serial Port)dnl
__devitem(s64_czs, cua[a-z]*, Zilog 8530 Serial Port)dnl
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
dnl
dnl *** MAKEDEV itself
dnl
_TITLE(make)
_DEV(all)
_DEV(std)
_DEV(loc)
_TITLE(tap)
_DEV(st, 18, 11)
_TITLE(dis)
_DEV(sd, 17, 7)
_DEV(cd, 58, 18)
_DEV(ch, 19)
_DEV(uk, 120)
_DEV(ss, 121)
_DEV(xy, 9, 3)
_DEV(rd, 106, 17)
_DEV(xd, 42, 10)
_DEV(flo, 54, 16)
_DEV(vnd, 110, 8)
_DEV(ccd, 23, 9)
_TITLE(pty)
_DEV(tty, 20)
_DEV(pty, 21)
_TITLE(prn)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 78)
_DEV(wskbd, 79)
_DEV(wsmux, 81)
_TITLE(point)
_DEV(wsmouse, 80)
_TITLE(term)
_DEV(s64_tzs, 12)
_DEV(s64_czs, 12)
_TITLE(spec)
_DEV(au, 69)
_DEV(oppr)
_DEV(bpf, 105)
_DEV(pf, 59)
_DEV(lkm, 112)
_DEV(tun, 111)
_DEV(rnd, 119)
_DEV(mag, 100)
_DEV(bppmag, 101)
_DEV(spif, 102)
_DEV(bppsp, 103)
_DEV(xfs, 51)
_DEV(raid, 123, 25)
_DEV(fdesc, 24)
_DEV(ses, 124)
_DEV(systrace, 50)
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std random bpf0
	_recurse fd0 sd0 sd1 sd2 rd0 cd0
	;;

_std(2, 3, 122, 7, 16)
	M eeprom	c 3 11	640 kmem
	M openprom	c 70 0	640 kmem
	;;
dnl
dnl *** some sparc-specific targets
dnl
twrget(all, s64_tzs, tty, a, b, c, d)dnl
twrget(all, s64_czs, cua, a, b, c, d)dnl
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
target(all, hk, 0, 1, 2, 3)dnl
target(all, rd, 0)dnl
target(all, cd, 0, 1)dnl
target(all, sd, 0, 1, 2, 3, 4)dnl
target(all, vnd, 0, 1, 2, 3)dnl
target(all, ccd, 0, 1, 2, 3)dnl
target(ramd, fd, 0)dnl
target(ramd, sd, 0, 1, 2, 3)dnl
target(ramd, rd, 0)dnl
target(ramd, cd, 0)dnl
twrget(wscons, wscons, ttyD, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
twrget(wscons, wscons, ttyE, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
twrget(wscons, wscons, ttyF, cfg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b)dnl
