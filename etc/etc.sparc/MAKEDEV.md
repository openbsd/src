vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.13 2002/07/31 16:47:50 jason Exp $-},
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
_TITLE(term)
_DEV(tzs, 12)
_DEV(czs, 12)
_TITLE(spec)
_DEV(au, 69)
_DEV(oppr)
_DEV(btw, 27)
_DEV(ctw, 31)
_DEV(ctr, 55)
_DEV(cfr, 39)
_DEV(csx, 67)
_DEV(ceg, 64)
_DEV(cfo, 99)
_DEV(tcx, 109)
_DEV(bpf, 105)
_DEV(pf, 59)
_DEV(altq, 125)
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
	M fb		c 22 0
	M mouse		c 13 0
	M kbd		c 29 0
	;;
