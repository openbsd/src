#	$OpenBSD: Makefile,v 1.19 2000/01/16 11:43:57 jakob Exp $
#
#  Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
# 	The Regents of the University of California.  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that: (1) source code distributions
#  retain the above copyright notice and this paragraph in its entirety, (2)
#  distributions including binary code include the above copyright notice and
#  this paragraph in its entirety in the documentation or other materials
#  provided with the distribution, and (3) all advertising materials mentioning
#  features or use of this software display the following acknowledgement:
#  ``This product includes software developed by the University of California,
#  Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
#  the University nor the names of its contributors may be used to endorse
#  or promote products derived from this software without specific prior
#  written permission.
#  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#

PROG=	tcpdump
MAN=	tcpdump.8

CFLAGS+=-DCSLIP -DPPP -DHAVE_FDDI -DETHER_SERVICE -DRETSIGTYPE=void -DHAVE_NET_SLIP_H -DHAVE_ETHER_NTOHOST
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "mips") || (${MACHINE_ARCH} == "sparc")
CFLAGS+=-DLBL_ALIGN
.endif

LDADD+=	-lpcap -ll
DPADD+=	${LIBL} ${LIBPCAP}

SRCS=	tcpdump.c addrtoname.c \
	print-ether.c print-ip.c print-arp.c print-tcp.c print-udp.c \
	print-atalk.c print-domain.c print-tftp.c print-bootp.c print-nfs.c \
	print-icmp.c print-sl.c print-ppp.c print-rip.c \
	print-snmp.c print-ntp.c print-null.c print-egp.c print-ospf.c \
	print-fddi.c print-llc.c print-sunrpc.c \
	print-wb.c print-decnet.c print-isoclns.c print-ipx.c \
	print-atm.c print-dvmrp.c print-krb.c print-pim.c print-netbios.c \
	util.c bpf_dump.c parsenfsfh.c version.c machdep.c print-igrp.c \
	print-gre.c print-radius.c print-enc.c print-cnfp.c \
	print-ipsec.c print-ike.c print-raw.c print-l2tp.c print-mobile.c \
	print-bgp.c \
	gmt2local.c savestr.c setsignal.c

AWKS =	atime.awk packetdat.awk send-ack.awk stime.awk

.include <bsd.prog.mk>
