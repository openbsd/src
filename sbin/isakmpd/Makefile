#	$OpenBSD: Makefile,v 1.13 1999/04/19 21:00:19 niklas Exp $
#	$EOM: Makefile,v 1.48 1999/04/16 21:24:46 niklas Exp $

#
# Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Ericsson Radio Systems.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# This code was written under funding by Ericsson Radio Systems.
#

#
# This makefile is a "pmake" one, i.e. the make variant commonly found in
# BSD derived systems, where it is indeed named "make".  Other systems
# may provide this make variant as "pmake" or maybe "bsdmake".
#

# openbsd means 2.5 or newer, openbsd-encap is for older kernels with PF_ENCAP
# linux is the name for Linux with FreeS/WAN integrated
OS=		openbsd
#OS=		openbsd-encap
#OS=		linux

.PATH:		${.CURDIR}/sysdep/${OS}

PROG=		isakmpd
BINDIR?=	/sbin
SRCS=		app.c asn.c asn_useful.c attribute.c cert.c constants.c \
		conf.c cookie.c crypto.c dh.c doi.c exchange.c exchange_num.c \
		field.c gmp_util.c hash.c if.c ike_auth.c ike_aggressive.c \
		ike_main_mode.c ike_phase_1.c \
		ike_quick_mode.c init.c ipsec.c ipsec_fld.c ipsec_num.c \
		isakmpd.c isakmp_doi.c isakmp_fld.c isakmp_num.c log.c \
		message.c math_2n.c math_ec2n.c math_group.c \
		pkcs.c prf.c sa.c sysdep.c timer.c transport.c udp.c ui.c \
		util.c x509.c

GENERATED=	exchange_num.h ipsec_fld.h ipsec_num.h isakmp_fld.h \
		isakmp_num.h
CLEANFILES=	exchange_num.c exchange_num.h ipsec_num.c ipsec_num.h \
		isakmp_num.c isakmp_num.h ipsec_fld.c ipsec_fld.h \
		isakmp_fld.c isakmp_fld.h
MAN=		isakmpd.8 isakmpd.conf.5
CFLAGS+=	-Wall -DNEED_SYSDEP_APP \
		-I${.CURDIR} -I${.CURDIR}/sysdep/${OS} -I. \

# Different debugging & profiling suggestions

# Include symbolic debugging info
DEBUG=		-g

# Do execution time profiles
#CFLAGS+=	-pg

# If you have ElectricFence available, you can spot abuses of the heap.
# (/usr/ports/devel/ElectricFence)
#LDADD+=		-L/usr/local/lib -lefence
#DPADD+=		/usr/local/lib/libefence.a

# If you like to use Boehm's garbage collector (/usr/ports/devel/boehm-gc).
#LDADD+=		-L/usr/local/lib -lgc
#DPADD+=		/usr/local/lib/libgc.a

# You can also use Boehm's garbage collector as a means to find leaks.
# XXX Expect warnings due to missing prototypes if using this though.
#LDADD+=		-L/usr/local/lib -lleak
#DPADD+=		/usr/local/lib/libleak.a
#CFLAGS+=	-D'malloc(x)=GC_debug_malloc((x),__FILE__,__LINE__)' \
#		-D'realloc(x,y)=GC_debug_realloc((x),(y),__FILE__,__LINE__)' \
#		-D'free(x)=GC_debug_free(x)' -D'calloc(x,y)=malloc((x)*(y))'

.if !make(install)
SUBDIR=		regress
.endif

.include "sysdep/${OS}/Makefile.sysdep"

exchange_num.c exchange_num.h: genconstants.sh exchange_num.cst
	/bin/sh ${.CURDIR}/genconstants.sh ${.CURDIR}/exchange_num

ipsec_fld.c ipsec_fld.h: genfields.sh ipsec_fld.fld
	/bin/sh ${.CURDIR}/genfields.sh ${.CURDIR}/ipsec_fld

ipsec_num.c ipsec_num.h: genconstants.sh ipsec_num.cst
	/bin/sh ${.CURDIR}/genconstants.sh ${.CURDIR}/ipsec_num

isakmp_fld.c isakmp_fld.h: genfields.sh isakmp_fld.fld
	/bin/sh ${.CURDIR}/genfields.sh ${.CURDIR}/isakmp_fld

isakmp_num.c isakmp_num.h: genconstants.sh isakmp_num.cst
	/bin/sh ${.CURDIR}/genconstants.sh ${.CURDIR}/isakmp_num

${PROG} beforedepend: ${GENERATED}

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
