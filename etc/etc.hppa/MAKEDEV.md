vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.9 2002/10/01 21:10:43 mickey Exp $-},
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
_DEV(st, 11, 5)
_TITLE(dis)
_DEV(flo, 24, 7)
_DEV(sd, 10, 4)
_DEV(cd, 12, 6)
_DEV(ccd, 7, 1)
_DEV(vnd, 8, 2)
_DEV(rd, 9, 3)
_TITLE(term)
_DEV(com, 23)
_TITLE(pty)
_DEV(tty, 4)
_DEV(pty, 5)
_TITLE(prn)
_DEV(lpt, 30)
_TITLE(call)
_TITLE(spec)
_DEV(fdesc, 16)
_DEV(bpf, 17)
_DEV(tun, 18)
_DEV(pf, 21)
_DEV(lkm, 19)
_DEV(altq, 33)
_DEV(rnd, 20)
_DEV(xfs, 31)
_DEV(ch, 13)
_DEV(ss, 14)
_DEV(uk, 15)
_DEV(ses, 37)
_DEV(pdc, 22)
_DEV(systrace, 34)
_DEV(au, 35)
_DEV(cry, 36)
#
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std fd st0 st1 sd0 sd1 sd2 sd3 rd0
	_recurse pty0 bpf0 bpf1 tun0 tun1 lkm random
	;;

_std(1, 2, 29, 3, 6)
	;;
