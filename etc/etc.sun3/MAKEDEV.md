vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.10 2002/12/05 04:30:21 kjc Exp $-},
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
_DEV(local)
_TITLE(tap)
_DEV(st, 18, 11)
_TITLE(dis)
_DEV(sd, 17, 7)
_DEV(vnd, 19, 5)
_DEV(ccd, 33, 9)
_DEV(cd, 58, 18)
_DEV(xy, 9, 3)
_DEV(xd, 42, 10)
_TITLE(term)
_DEV(tzs, 12)
dnl _DEV(czs, 12)
_TITLE(pty)
_DEV(tty, 20)
_DEV(pty, 21)
_TITLE(prn)
_TITLE(call)
_TITLE(spec)
_DEV(btw, 27)
_DEV(ctw, 31)
_DEV(cfr, 39)
_DEV(bpf, 36)
_DEV(pf, 75)
_DEV(tun, 24)
_DEV(rd, 52, 13)
_DEV(rnd, 72)
_DEV(uk, 98)
_DEV(ss, 99)
_DEV(fdesc, 23)
_DEV(xfs, 51)
_DEV(systrace, 86)
dnl
divert(2)dnl
dnl
# XXX - Keep /usr/etc so SunOS can find chown
test -d /usr/etc && PATH=$PATH:/usr/etc

dnl
divert(7)dnl
dnl
ramdisk)
	R std random bpf0 sd0 sd1 rd0 cd0
	;;

_std(2, 3, 37, 7, 16)
	M kd		c 1 0 600
	M eeprom	c 3 11 640 kmem
	M mouse		c 13 0
	M fb		c 22 0
	M kbd		c 29 0
	M leds		c 3 13 644
	M vme16d16	c 3 5 600; MKlist="$MKlist;ln -s vme16d16 vme16"
	M vme24d16	c 3 6 600; MKlist="$MKlist;ln -s vme24d16 vme24"
	M vme32d16	c 3 7 600
	M vme16d32	c 3 8 600
	M vme24d32	c 3 9 600
	M vme32d32	c 3 10 600; MKlist="$MKlist;ln -s vme32d32 vme32"
	RMlist="$RMlist vme16 vme24 vme32"
	;;
dnl
dnl *** sun3 specific targets
dnl
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
