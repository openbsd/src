vers(__file__,
	{-$OpenBSD: MAKEDEV.md,v 1.2 2002/02/09 16:59:50 deraadt Exp $-},
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
_DEV(st,38,21)
_DEV(mt,38,15)
_DEV(ht,5,1)
_DEV(tm,14,5)
_DEV(ts,16,6)
_DEV(ut,17,10)
_TITLE(dis)
_DEV(hp,4,0)
_DEV(hk,11,3)
_DEV(up,13,2)
_DEV(ra,9,9)
_DEV(rb,23,11)
_DEV(rx,30,12)
_DEV(rl,32,14)
_DEV(hd,58,19)
_DEV(sd,59,20)
_DEV(cd,58,22)
_DEV(ch,19)
_DEV(uk,120)
_DEV(ss,121)
_DEV(xy,9,3)
_DEV(rd,106,23)
_DEV(xd,42,10)
_DEV(flo,54,16)
_DEV(vnd,55,8)
_DEV(ccd,54,17)
_TITLE(pty)
_DEV(tty,20)
_DEV(pty,21)
_TITLE(prn)
_TITLE(term)
dnl _DEV(ttyS,34)
_TITLE(spec)
_DEV(au,69)
_DEV(oppr)
_DEV(bpf,56)
_DEV(pf,42)
_DEV(altq,75)
_DEV(lkm,112)
_DEV(tun,111)
_DEV(rnd,119)
_DEV(mag)
_DEV(xfs,51)
_DEV(raid,73,25)
_DEV(fdesc,53)
_DEV(ses,124)
dnl
divert(7)dnl
dnl
ramdisk)
	_recurse std random
	_recurse fd0 sd0 sd1 rd0
	;;

_std(2,3,50,7,33)
	M ttyg0		c 25 0
	M ttyg1		c 25 1
	M ttyg2		c 25 2
	M ttyg3		c 25 3
	M crl		c 35 0
	M csa1		c 51 0
	M csa2		c 51 1
	M tu0		b 8 0
	M tu1		b 8 1
	M kUmem		c 3 3 660
	;;

ht*|tm*|mt*|ts*|ut*)
	case $i in
	ht*) n=ht b=1 c=5;;
	mt*) n=mt b=15 c=38;;
	tm*) n=tm b=5 c=14;;
	ts*) n=ts b=6 c=16;;
	ut*) n=ut b=10 c=17;;
	esac
	case $U in
	[0-7])
		four=Add($U,4) eight=Add($U,8)
		twelve=Add($U,12) twenty=Add($U,20)
		M $n$U		b $b $U	660 operator
		M $n$four	b $b $four	660 operator
		M $n$eight	b $b $eight	660 operator
		M $n$twelve	b $b $twelve	660 operator
		M n$n$four	b $b $four	660 operator
		M n$n$twelve	b $b $twelve	660 operator
		M nr$n$four	c $c $four	660 operator
		M nr$n$twelve	c $c $twelve	660 operator
		M r$n$U		c $c $U		660 operator
		M r$n$four	c $c $four	660 operator
		M r$n$eight	c $c $eight	660 operator
		M r$n$twelve	c $c $twelve	660 operator
		if [ $i = ut ]; : XXXX
		then
			M $n$twenty	b $b $twenty 660 operator
			M r$n$twenty	c $b $twenty 660 operator
		fi
		#RMlist="$RMlist mt$U mt$four mt$eight mt$twelve"
		#RMlist="$RMlist nmt$U nmt$eight nrmt$eight"
		#RMlist="$RMlist nrmt$unit nrmt$eight"
		#RMlist="$RMlist rmt$unit rmt$four rmt$eight rmt$twelve"
		;;
	*)
		echo bad unit for tape in: $i
		;;
	esac
	umask 77
	;;
