vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.23 2002/07/31 16:47:50 jason Exp $-},
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
__devitem(uperf, uperf, performance counters)dnl
_mkdev(uperf, uperf, {-M uperf c major_uperf_c 0 664-})dnl
_DEV(all)
_DEV(ramdisk)
_DEV(std)
_DEV(loc)
_TITLE(tap)
dnl _DEV(wt, 10, 3)
_DEV(st, 18, 11)
_DEV(ch, 19)
_TITLE(dis)
_DEV(wd, 26, 12)
_DEV(flo, 54, 16)
_DEV(sd, 17, 7)
_DEV(cd, 58, 18)
_DEV(vnd, 110, 8)
_DEV(rd, 61, 5)
_DEV(ccd, 23, 9)
_DEV(raid, 121, 25)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp, 78)
_DEV(wskbd, 79)
_DEV(wsmux, 81)
_DEV(pcons, 122)
_TITLE(point)
_DEV(mouse, 13)
_DEV(wsmouse, 80)
_TITLE(term)
_DEV(s64_tzs, 12)
_DEV(s64_czs, 12)
_DEV(com, 36)
_DEV(tth, 77)
_TITLE(pty)
_DEV(tty, 20)
_DEV(pty, 21)
_TITLE(prn)
_DEV(lpt, 37)
_DEV(lpa)
_TITLE(usb)
_DEV(usb, 90)
_DEV(uhid, 91)
_DEV(ugen, 92)
_DEV(ulpt, 93)
_DEV(urio, 94)
_DEV(utty, 95)
_DEV(uscan, 96)
_TITLE(spec)
_DEV({-usbs-})
_DEV(ses, 4)
_DEV(fdesc, 24)
_DEV(xfs, 51)
_DEV(ss, 59)
_DEV(uk, 60)
_DEV(au, 69)
_DEV(pf, 73)
_DEV(altq, 74)
_DEV(bpf, 105)
_DEV(tun, 111)
_DEV(lkm, 112)
_DEV(rnd, 119)
_DEV(mag, 71)
_DEV(bppmag, 72)
_DEV(spif, 108)
_DEV(bppsp, 109)
_DEV(cry, 75)
_DEV(pci, 52)
_DEV(uperf, 25)
_DEV(systrace, 50)
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std fd0 wd0 wd1 wd2 sd0 sd1 sd2 rd0
	_recurse st0 cd0 bpf0 random
	;;

_std(2, 3, 76, 7, 16)
	M mouse		c 13 0 666
	M fb		c 22 0 666
	M openprom	c 70 0 640 kmem
	;;

mouse*)
	name=${i##mouse-}
	if [ ! -c $name ]; then
		$0 $name	# make the appropriate device
	fi
	RMlist="$RMlist mouse"
	MKlist="$MKlist;ln -s $name mouse"
	;;
