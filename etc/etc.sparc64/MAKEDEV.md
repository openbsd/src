vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.5 2002/01/12 21:08:59 jason Exp $-},
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
_DEV(floppy)
_DEV(std)
_DEV(loc)
_TITLE(tap)
dnl _DEV(wt,10,3)
_DEV(st,18,11)
_DEV(ch,19)
_TITLE(dis)
_DEV(wd,26,12)
_DEV(flo,54,16)
_DEV(sd,17,7)
_DEV(cd,58,18)
_DEV(vnd,110,8)
_DEV(rd,61,5)
_DEV(ccd,23,9)
_DEV(raid,121,25)
_TITLE(cons)
_DEV(wscons)
_DEV(wsdisp,78)
_DEV(wskbd,67)
_DEV(wsmux,69)
_DEV(pcons,122)
_TITLE(point)
_DEV(mouse,13)
_DEV(wsmouse,80)
_TITLE(term)
_DEV(tzs,12)
_DEV(czs,12)
_DEV(com,36)
_DEV(tth,77)
_TITLE(pty)
_DEV(tty,20)
_DEV(pty,21)
dnl #
dnl _TITLE(prn)
dnl _DEV(lpt,16)
dnl _DEV(lpa)
dnl #
_TITLE(usb)
_DEV(usb,90)
_DEV(uhid,91)
_DEV(ugen,92)
_DEV(ulpt,93)
_DEV(urio,94)
_DEV(utty,95)
_DEV(uscan,96)
_TITLE(spec)
_DEV({-usbs-})
_DEV(ses,4)
_DEV(fdesc,24)
_DEV(xfs,51)
_DEV(ss,59)
_DEV(uk,60)
_DEV(au,69)
_DEV(pf,73)
_DEV(altq,74)
_DEV(bpf,105)
_DEV(tun,111)
_DEV(lkm,112)
_DEV(rnd,119)
_DEV(cry,75)
dnl
divert(7)dnl
dnl
floppy)
	_recurse std fd0 wd0 wd1 sd0 sd1
	_recurse st0 cd0 random
	;;

ramdisk)
	_recurse std lkm random
	_recurse fd0 rd0 wd0 wd1 wd2 wd3 bpf0
	_recurse sd0 sd1 sd2 sd3 st0 st1 cd0 cd1
	;;

_std(2,3,76,7,16)
	M mouse		c 13 0 666
	M fb		c 22 0 666
	M openprom	c 70 0 644
	;;

mouse*)name=${i##mouse-}
	if [ ! -c $name ]; then
		$0 $name	# make the appropriate device
	fi
	RMlist="$RMlist mouse"
	MKlist="$MKlist;ln -s $name mouse";;

magma*)
	case $U in
	0)	offset=0  nam=m;;
	1)	offset=16 nam=n;;
	2)	offset=32 nam=o;;
	*)	echo "bad unit for $i: $U"; exit 127;;
	esac
	offset=Mult($U,64)
	n=0
	while [ $n -lt 16 ]
	do
		name=${nam}`hex $n`
		M tty$name c 71 Add($offset,$n)
		n=Add($n,1)
	done
	M bpp${nam}0 c 72 Add($offset,0)
	M bpp${nam}1 c 72 Add($offset,1)
	;;

dnl No number allocated yet...
dnl spif*)
dnl 	case $U in
dnl 	0)	offset=0  nam=j;;
dnl 	1)	offset=16 nam=k;;
dnl 	2)	offset=32 nam=l;;
dnl 	*)	echo "bad unit for $i: $U"; exit 127;;
dnl 	esac
dnl 	offset=Mult($U,64)
dnl 	n=0
dnl 	while [ $n -lt 16 ]
dnl 	do
dnl 		name=${nam}`hex $n`
dnl 		M tty$name c 102 Add($offset,$n)
dnl 		n=Add($n,1)
dnl 	done
dnl 	M bpp${nam}0 c 103 Add($offset,0)
dnl 	;;
